/*
 * drm_lib.h — in-process C API for drm-native.
 *
 * When drm-native is built as a shared library (-DDRM_LIB_BUILD), the Go
 * engine loads it via CGO and calls these functions directly instead of
 * communicating over TCP sockets.
 *
 * Thread safety: all functions are safe to call from multiple goroutines.
 * drm_lib_init() must complete before any other call.
 */

#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Callback types ──────────────────────────────────────────────────────────
 *
 * drm_auth_cb_t — called when the DRM library needs a credential or 2FA code.
 *   type:     "credentials" → write "user:pass\0" into out_buf
 *             "2fa"         → write 6-digit code string into out_buf
 *   out_buf:  caller-provided buffer, out_size bytes
 *   userdata: opaque pointer passed to drm_lib_init
 *
 * drm_state_cb_t — called when DRM state transitions.
 *   state:    one of "STARTING", "LOGIN", "WAITING_2FA", "RUNNING",
 *             "INITIALIZING_FAIRPLAY", "FAILED"
 */
typedef void (*drm_auth_cb_t)(const char *type, char *out_buf, int out_size,
                               void *userdata);
typedef void (*drm_state_cb_t)(const char *state, void *userdata);

/* ── Init config ─────────────────────────────────────────────────────────────*/
typedef struct {
    const char *base_dir;     /* path containing mpl_db/ (e.g. drm/rootfs/…) */
    const char *lib64_dir;    /* path to system/lib64/ with Android .so files  */
    const char *username;     /* NULL → session-reuse; set for fresh login      */
    const char *password;     /* NULL → session-reuse; set for fresh login      */
    const char *device_info;  /* 9-field slash-separated device string, or NULL */
    int offline_only;         /* 1 = force download method; 0 = play method     */
    drm_auth_cb_t  auth_cb;   /* called for credential/2FA challenges            */
    void          *auth_ud;
    drm_state_cb_t state_cb;  /* called on state transitions                     */
    void          *state_ud;
} drm_lib_config_t;

/* ── Lifecycle ───────────────────────────────────────────────────────────────*/

/* drm_lib_init — run the full DRM initialisation sequence and return.
 * Blocks until the FairPlay lease is acquired and account tokens are cached.
 * Returns 0 on success, -1 on failure (check stderr for details).
 * Must be called exactly once before any other drm_lib_* function. */
int drm_lib_init(const drm_lib_config_t *cfg);

/* drm_lib_shutdown — stop background threads and free global state.
 * After this returns, no further calls to drm_lib_* are valid. */
void drm_lib_shutdown(void);

/* ── DRM operations ──────────────────────────────────────────────────────────*/

/* drm_lib_get_m3u8 — return the HLS m3u8 URL for adamId.
 * Returns a malloc'd string the caller must free(), or NULL on error. */
char *drm_lib_get_m3u8(unsigned long adam_id);

/* drm_lib_get_account — return {"storefront_id":…,"dev_token":…,"music_token":…}.
 * Returns a malloc'd JSON string the caller must free(), or NULL on error. */
char *drm_lib_get_account(void);

/* drm_lib_get_mv — progressive MV URL + download key for adamId.
 * out_url, out_dk: set to malloc'd strings; caller must free both.
 * out_has_itun: set to 1 if an itun decryptor is ready for this adamId.
 * Returns 0 on success, -1 on error. */
int drm_lib_get_mv(unsigned long adam_id, char **out_url, char **out_dk,
                   int *out_has_itun);

/* drm_lib_open_kd_ctx — acquire a FairPlay key-delivery context for
 * (adam_id, uri).  The context is internally cached; the returned pointer
 * is valid until drm_lib_shutdown().  Returns NULL on failure. */
void *drm_lib_open_kd_ctx(const char *adam_id, const char *uri);

/* drm_lib_decrypt — decrypt one FP-encrypted sample in-place.
 * kd_ctx: value returned by drm_lib_open_kd_ctx.
 * Returns 0 on success. */
int drm_lib_decrypt(void *kd_ctx, uint8_t *sample, uint32_t size);

/* drm_lib_decrypt_itun — decrypt one itun-encrypted sample in-place.
 * Must call drm_lib_get_mv first (creates the itun decryptor).
 * out_size: number of output bytes (may differ from in_size).
 * Returns 0 on success, -1 on error. */
int drm_lib_decrypt_itun(unsigned long adam_id, uint8_t *sample,
                          uint32_t in_size, uint32_t *out_size);

/* drm_lib_is_recovery_active — 1 if lease recovery is in progress. */
int drm_lib_is_recovery_active(void);

#ifdef __cplusplus
}
#endif
