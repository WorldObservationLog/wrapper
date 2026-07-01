/**
 * main.cpp — C++ glue: lease/playback callbacks and recovery subsystem.
 *
 * Recovery state machine
 * ─────────────────────
 *
 *   ┌──────────────────────────────────────────────────────┐
 *   │                       Running                        │◄──────────────┐
 *   └──────────────────────────────┬───────────────────────┘               │
 *                                  │ Lease end / Playback error             │
 *                                  ▼                                        │
 *   ┌──────────────────────────────────────────────────────┐               │
 *   │                      Scheduled                       │               │
 *   │   (event queued; worker waking; backoff pending)     │               │
 *   └──────────────────────────────┬───────────────────────┘               │
 *                                  │ backoff elapsed                        │
 *                                  ▼                                        │
 *   ┌──────────────────────────────────────────────────────┐               │
 *   │                     Refreshing                       │               │
 *   │   (refresh_decrypt_ctx() in progress; requests       │               │
 *   │    gated at handle() / handle_m3u8())                │               │
 *   └───────────────┬──────────────────────────────┬───────┘               │
 *                   │ preshareCtx non-null          │ preshareCtx NULL      │
 *                   ▼                               ▼                       │
 *   ┌───────────────────────────┐   ┌───────────────────────────┐          │
 *   │         Running           │   │          Failed            │          │
 *   │ consec_fails = 0          │   │ consec_fails++             │          │
 *   │ normal service resumes    │   │ auto-schedules retry       │──────────┘
 *   └───────────────────────────┘   └───────────────────────────┘
 *
 * Auto-retry on failure
 * ─────────────────────
 * When refresh_decrypt_ctx() succeeds but preshareCtx remains NULL (e.g.
 * transient Apple server error, network interruption), no further Apple event
 * arrives, so the worker would stall waiting on the condition variable.
 * To avoid this, the worker self-schedules a retry via schedule_recovery()
 * before looping back.  The retry goes through the same queue and backoff
 * path, ensuring exponential spacing even for self-triggered retries.
 *
 * Coalescing
 * ──────────
 * The queue is drained completely on each wake.  A burst of events
 * (3084 → 3084 → PLAYBACK_ERR) results in one refresh cycle, not three.
 *
 * RecoveryState visibility
 * ────────────────────────
 * get_recovery_state() is exported as extern "C" and returns RecoveryState
 * as a plain int so main.c (and a future HTTP status endpoint or Electron IPC
 * handler) can expose the daemon's current condition without C++ knowledge.
 * Values map directly to the RecoveryState enum below.
 */

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <unistd.h>   /* sleep() */
#include <vector>

extern "C" void refresh_decrypt_ctx(void);
extern "C" int  is_preshare_ctx_ready(void);
extern "C" void handle(int fd);

// ---------------------------------------------------------------------------
// RecoveryState — the daemon's lifecycle state machine
// ---------------------------------------------------------------------------

enum class RecoveryState : int {
    Running    = 0,  // Normal operation; all requests served
    Scheduled  = 1,  // Recovery event queued; worker waking; backoff pending
    Refreshing = 2,  // refresh_decrypt_ctx() in flight; requests gated
    Failed     = 3,  // Last cycle failed; auto-retry queued
};

static std::atomic<RecoveryState> g_recovery_state{RecoveryState::Running};

// Returns the current RecoveryState as a plain int (C-visible).
// 0 = Running, 1 = Scheduled, 2 = Refreshing, 3 = Failed.
extern "C" int get_recovery_state(void)
{
    return static_cast<int>(g_recovery_state.load());
}

// Convenience predicate used by handle() / handle_m3u8() to gate requests.
// Returns 1 when the daemon is not in its normal Running state.
extern "C" int is_recovery_active(void)
{
    return (g_recovery_state.load() != RecoveryState::Running) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Recovery queue
// ---------------------------------------------------------------------------

// Event codes pushed into the recovery queue.
static const int kPlaybackErrSentinel = -1;  // playback error from pbErrCb
static const int kRetryInternal       = -2;  // self-scheduled retry on failure

// Exponential backoff: 1 → 2 → 5 → 10 → 30 seconds.
// kBackoffMaxIdx is reused indefinitely so retries never stop.
static const int kBackoffSecs[]   = {1, 2, 5, 10, 30};
static const int kBackoffMaxIdx   = 4;

static std::mutex               g_recovery_mtx;
static std::condition_variable  g_recovery_cv;
static std::queue<int>          g_recovery_q;

static void schedule_recovery(int code)
{
    {
        std::lock_guard<std::mutex> lk(g_recovery_mtx);
        g_recovery_q.push(code);
    }
    g_recovery_cv.notify_one();
}

// ---------------------------------------------------------------------------
// Recovery worker — the single owner of retry timing and reacquisition
// ---------------------------------------------------------------------------

static void recovery_worker()
{
    int consec_fails = 0;

    while (true) {

        // ── Wait for at least one event ─────────────────────────────────────
        std::vector<int> burst;
        {
            std::unique_lock<std::mutex> lk(g_recovery_mtx);
            g_recovery_cv.wait(lk, []{
                return !g_recovery_q.empty();
            });
            // Drain the entire queue (coalescing).
            while (!g_recovery_q.empty()) {
                burst.push_back(g_recovery_q.front());
                g_recovery_q.pop();
            }
        }

        g_recovery_state.store(RecoveryState::Scheduled);

        // ── Log every code in the burst ─────────────────────────────────────
        fprintf(stderr, "[recovery] STATE=Scheduled — %zu event(s) coalesced:\n",
                burst.size());
        for (int c : burst) {
            switch (c) {
                case kPlaybackErrSentinel:
                    fprintf(stderr, "[recovery]   PLAYBACK_ERROR\n");
                    break;
                case kRetryInternal:
                    fprintf(stderr, "[recovery]   RETRY_INTERNAL "
                                    "(self-scheduled after failed refresh)\n");
                    break;
                case 3084:
                    fprintf(stderr, "[recovery]   LEASE_END code=3084 "
                                    "(server revoke — simultaneous playback "
                                    "or natural expiry)\n");
                    break;
                default:
                    fprintf(stderr, "[recovery]   LEASE_END code=%d "
                                    "(unknown — logging for future mapping)\n", c);
                    break;
            }
        }

        // ── Exponential backoff ─────────────────────────────────────────────
        int idx   = (consec_fails <= kBackoffMaxIdx) ? consec_fails
                                                      : kBackoffMaxIdx;
        int delay = kBackoffSecs[idx];

        if (consec_fails == 0) {
            fprintf(stderr,
                "[recovery] RECOVERY_SCHEDULED attempt=1 backoff=%ds\n",
                delay);
        } else {
            fprintf(stderr,
                "[recovery] RECOVERY_SCHEDULED attempt=%d backoff=%ds "
                "(Apple may still be rejecting — retrying indefinitely)\n",
                consec_fails + 1, delay);
        }
        sleep(delay);

        // ── Enter Refreshing state — gate incoming client requests ──────────
        g_recovery_state.store(RecoveryState::Refreshing);
        fprintf(stderr, "[recovery] STATE=Refreshing — calling refresh_decrypt_ctx()\n");

        // Single-threaded by design: the worker is the only caller of
        // refresh_decrypt_ctx().  No other code path calls it directly.
        refresh_decrypt_ctx();

        // ── Evaluate success ────────────────────────────────────────────────
        // is_preshare_ctx_ready() reads preshareCtx under g_ctx_mutex (main.c).
        // A non-null context means Apple accepted the new lease and FairPlay
        // key derivation succeeded — requests can resume.
        if (is_preshare_ctx_ready()) {
            consec_fails = 0;
            g_recovery_state.store(RecoveryState::Running);
            fprintf(stderr,
                "[recovery] STATE=Running — REFRESH_SUCCESS, "
                "decrypt context ready, HTTP servers resuming normal operation\n");
        } else {
            consec_fails++;
            g_recovery_state.store(RecoveryState::Failed);

            int next_idx   = (consec_fails <= kBackoffMaxIdx) ? consec_fails
                                                               : kBackoffMaxIdx;
            int next_delay = kBackoffSecs[next_idx];
            fprintf(stderr,
                "[recovery] STATE=Failed — REFRESH_FAILED "
                "ctx_ready=false consecutive_fails=%d retrying_in=%ds\n",
                consec_fails, next_delay);

            // Self-schedule a retry so the daemon keeps trying even when
            // Apple sends no further lease-end event (e.g. transient network
            // failure during the refresh).  The retry goes through the normal
            // queue + backoff path, so exponential spacing is preserved.
            schedule_recovery(kRetryInternal);
        }
    }
}

// Called from main() in main.c, after leaseMgr and FHinstance are ready.
extern "C" void start_recovery_thread(void)
{
    std::thread(recovery_worker).detach();
    fprintf(stderr, "[+] recovery thread started\n");
}

// ---------------------------------------------------------------------------
// FairPlay decrypt exception shim
// ---------------------------------------------------------------------------

extern "C" uint8_t handle_cpp(int fd)
{
    try {
        handle(fd);
        return 1;
    } catch (const std::exception &e) {
        fprintf(stderr, "[!] catched an exception: %s\n", e.what());
        return 0;
    }
}

// ---------------------------------------------------------------------------
// SVPlaybackLeaseManager callbacks
//
// CONTRACT: return as fast as possible.  Do NOT call any library function,
// perform I/O, or acquire any lock that the lease manager might also hold.
// ---------------------------------------------------------------------------

static void endLeaseCb(int const &c)
{
    fprintf(stderr, "[.] LEASE_END code=%d — scheduling async recovery\n", c);
    schedule_recovery(c);
    // Returns immediately.  recovery_worker handles reacquisition.
}

static void pbErrCb(void *)
{
    fprintf(stderr, "[.] PLAYBACK_ERROR — scheduling async recovery\n");
    schedule_recovery(kPlaybackErrSentinel);
    // Returns immediately.  recovery_worker handles context refresh.
}

extern "C" std::function<void (int const&)> endLeaseCallback(endLeaseCb);
extern "C" std::function<void (void *)>     pbErrCallback(pbErrCb);