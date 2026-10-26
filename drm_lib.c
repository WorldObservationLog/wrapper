/*
 * drm_lib.c — in-process C API implementation.
 *
 * Built with -DDRM_LIB_BUILD alongside main.c (which guards int main() with
 * #ifndef DRM_LIB_BUILD).  The result is libdrm-native.so, loaded by the Go
 * engine via CGO instead of forking drm-native as a subprocess.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define IMPORT_H_NO_DATA_DEFS
#include "import.h"
#include "drm_lib.h"
#include "cmdline.h"

#ifdef _WIN32
static int drm_windows_setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name) != NULL) return 0;
    return _putenv_s(name, value != NULL ? value : "");
}
#define setenv drm_windows_setenv
#endif

/* ── Externs from hybris_stubs.c ────────────────────────────────────────────*/

extern uint8_t endLeaseCallback[32];
extern uint8_t pbErrCallback[32];

/* ── Forward declarations for functions in main.c ──────────────────────────*/

/* init helpers (defined in main.c, de-staticized) */
extern void              drm_init_internal(void);
extern struct shared_ptr drm_init_ctx(void);

/* globals in main.c (de-staticized) */
extern struct shared_ptr apInf;
extern uint8_t           leaseMgr[16];
extern struct shared_ptr reqCtx;
extern struct gengetopt_args_info args_info;
extern char             *amUsername, *amPassword;
extern size_t            amPasswordLen;
extern int               decryptCount;
extern int               offlineFlag;
extern char             *device_infos[9];
extern void             *FHinstance;
extern void             *preshareCtx;
extern struct shared_ptr g_itun_decryptor;
extern unsigned long     g_itun_adam_id;
extern pthread_mutex_t   g_itun_mutex;

extern drm_auth_cb_t  g_drm_auth_cb;
extern void          *g_drm_auth_ud;
extern drm_state_cb_t g_drm_state_cb;
extern void          *g_drm_state_ud;

/* init/codec helpers in main.c / hybris_stubs.c */
extern void hybris_init_callbacks(void);
extern void start_recovery_thread(void);
extern int hybris_init_libs(const char *lib64_path);

/* account helpers */
extern char *get_account_storefront_id(struct shared_ptr reqCtx);
extern char *get_dev_token(struct shared_ptr reqCtx);
extern char *get_music_user_token(char *guid, char *authToken, struct shared_ptr reqCtx);
extern char *get_guid(void);
extern void  write_storefront_id(void);
extern void  write_music_token(void);
extern int   offline_available(void);

/* decrypt helpers */
extern void *getKdContext(const char *adam, const char *uri);

/* m3u8 / progressive helpers */
extern const char *get_m3u8_method_play(uint8_t leaseMgr[16], unsigned long adamID, char **dk);
extern const char *get_m3u8_method_download(struct shared_ptr reqCtx, unsigned long adamID, char **dk);
extern const char *get_progressive_method_play(uint8_t leaseMgr[16], unsigned long adamID, char **dk);

/* NfcRKVnxuKZy04KWbdFu71Ou and _ZN17SVPastisDecryptor13decryptSampleEPKhRKjPj
 * are declared by import.h (included above with IMPORT_H_NO_DATA_DEFS). */

/* cached account info (in main.c) */
extern char *g_storefront_id;
extern char *g_dev_token;
extern char *g_music_token;

/* ── Module state ───────────────────────────────────────────────────────────*/

static int             g_init_result = 0;  /* 0 = success, -1 = failed */
static pthread_mutex_t g_init_mutex  = PTHREAD_MUTEX_INITIALIZER;

/* ── drm_lib_init ───────────────────────────────────────────────────────────*/

int drm_lib_init(const drm_lib_config_t *cfg)
{
    pthread_mutex_lock(&g_init_mutex);

    /* Store callbacks before any library call so write_drm_state fires them */
    g_drm_auth_cb  = cfg->auth_cb;
    g_drm_auth_ud  = cfg->auth_ud;
    g_drm_state_cb = cfg->state_cb;
    g_drm_state_ud = cfg->state_ud;

    /* If the Android lib64 dir is provided, initialise hybris now.
     * The hybris_ctor constructor may have been a no-op (env var not set at
     * load time when running in-process), so we re-run init here. */
    if (cfg->lib64_dir && cfg->lib64_dir[0]) {
        setenv("HYBRIS_ANDROID_LIB64", cfg->lib64_dir, 1);
        if (hybris_init_libs(cfg->lib64_dir) != 0) {
            g_init_result = -1;
            pthread_mutex_unlock(&g_init_mutex);
            return -1;
        }
    }

    /* Synthesise a fake args_info from cfg so the rest of main.c still works */
    memset(&args_info, 0, sizeof(args_info));
    if (cfg->base_dir) {
        args_info.base_dir_arg = (char *)cfg->base_dir;
        args_info.base_dir_given = 1;
    }
    {
        /* Use provided device_info or fall back to the same default as cmdline.c */
        const char *di = (cfg->device_info && cfg->device_info[0])
            ? cfg->device_info
            : "Music/4.9/Android/10/Samsung S9/7663313/en-US/en-US/dc28071e981c439e";
        args_info.device_info_arg = (char *)di;
        args_info.device_info_given = (cfg->device_info && cfg->device_info[0]) ? 1 : 0;
        /* split device_infos just like main() does */
        static char *di_copy = NULL;
        if (di_copy) free(di_copy);
        di_copy = strdup(di);
        char *tok = strtok(di_copy, "/");
        for (int i = 0; i < 9 && tok; i++) {
            device_infos[i] = tok;
            tok = strtok(NULL, "/");
        }
    }
    offlineFlag = cfg->offline_only ? 1 : 0;

    /* Credentials for fresh login */
    if (cfg->username && cfg->password) {
        amUsername = (char *)cfg->username;
        /* Allocate a mutable buffer (credentialHandler appends the 2FA code) */
        static char pw_buf[256];
        strncpy(pw_buf, cfg->password, sizeof(pw_buf) - 1);
        pw_buf[sizeof(pw_buf) - 1] = '\0';
        amPassword = pw_buf;
        amPasswordLen = strlen(pw_buf);
        args_info.login_arg = NULL; /* not used in library mode */
        args_info.login_given = 0;
    }

    /*
     * Run the exact same sequence as main() up to RUNNING state.
     * hybris_init_libs() was already called by hybris_ctor.c's __attribute__((constructor)).
     */
    drm_init_internal();
    hybris_init_callbacks();

    reqCtx = drm_init_ctx();
    if (reqCtx.obj == NULL) {
        fprintf(stderr, "[drm_lib] drm_init_ctx failed\n");
        g_init_result = -1;
        pthread_mutex_unlock(&g_init_mutex);
        return -1;
    }

    /* Login if credentials supplied */
    extern uint8_t login(struct shared_ptr ctx);
    if (cfg->username && cfg->password) {
        if (!login(reqCtx)) {
            fprintf(stderr, "[drm_lib] login failed\n");
            g_init_result = -1;
            pthread_mutex_unlock(&g_init_mutex);
            return -1;
        }
    }

    /* Lease */
    DRM_DEBUG("[drm_lib] lease ctor\n");
    _ZN22SVPlaybackLeaseManagerC2ERKNSt6__ndk18functionIFvRKiEEERKNS1_IFvRKNS0_10shared_ptrIN17storeservicescore19StoreErrorConditionEEEEEE(
        leaseMgr, &endLeaseCallback, &pbErrCallback);
    uint8_t autom = 1;
    DRM_DEBUG("[drm_lib] refreshLeaseAutomatically\n");
    _ZN22SVPlaybackLeaseManager25refreshLeaseAutomaticallyERKb(leaseMgr, &autom);
    DRM_DEBUG("[drm_lib] requestLease\n");
    _ZN22SVPlaybackLeaseManager12requestLeaseERKb(leaseMgr, &autom);
    DRM_DEBUG("[drm_lib] requestLease returned\n");

    DRM_DEBUG("[drm_lib] FHinstance\n");
    FHinstance = _ZN21SVFootHillSessionCtrl8instanceEv();
    DRM_DEBUG("[drm_lib] start_recovery_thread\n");
    start_recovery_thread();
    DRM_DEBUG("[drm_lib] offline_available\n");

    offlineFlag = offline_available();
    DRM_DEBUG("[drm_lib] offline=%d, get_storefront_id\n", offlineFlag);

    /* Cache account tokens */
    g_storefront_id = get_account_storefront_id(reqCtx);
    if (!g_storefront_id) {
        fprintf(stderr, "[drm_lib] failed to get storefront ID\n");
        g_init_result = -1;
        pthread_mutex_unlock(&g_init_mutex);
        return -1;
    }
    DRM_DEBUG("[drm_lib] storefront_id=%s, get_dev_token\n", g_storefront_id);
    g_dev_token = get_dev_token(reqCtx);
    if (!g_dev_token) {
        fprintf(stderr, "[drm_lib] failed to get dev token\n");
        g_init_result = -1;
        pthread_mutex_unlock(&g_init_mutex);
        return -1;
    }
    DRM_DEBUG("[drm_lib] dev_token ok, get_music_token\n");
    g_music_token = get_music_user_token(get_guid(), g_dev_token, reqCtx);
    if (!g_music_token && cfg->base_dir) {
        /* createMusicToken failed (session may be expired) — fall back to the
         * MUSIC_TOKEN stored in drm/files/ by the web-session path. */
        char tok_path[1024];
        snprintf(tok_path, sizeof(tok_path), "%s/MUSIC_TOKEN", cfg->base_dir);
        FILE *tf = fopen(tok_path, "r");
        if (tf) {
            char buf[8192];
            size_t n = fread(buf, 1, sizeof(buf) - 1, tf);
            fclose(tf);
            buf[n] = '\0';
            while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ')) buf[--n] = '\0';
            if (n > 0) {
                g_music_token = strdup(buf);
                DRM_DEBUG("[drm_lib] music_token: using stored token from disk (%zu bytes)\n", n);
            }
        }
    }
    if (!g_music_token) {
        fprintf(stderr, "[drm_lib] failed to get music token\n");
        g_init_result = -1;
        pthread_mutex_unlock(&g_init_mutex);
        return -1;
    }
    DRM_DEBUG("[drm_lib] music_token ok\n");

    if (cfg->base_dir) {
        DRM_DEBUG("[drm_lib] write_storefront_id\n");
        write_storefront_id();
        DRM_DEBUG("[drm_lib] write_music_token\n");
        write_music_token();
    }
    DRM_DEBUG("[drm_lib] calling state_cb RUNNING\n");

    if (g_drm_state_cb)
        g_drm_state_cb("RUNNING", g_drm_state_ud);

    g_init_result = 0;
    pthread_mutex_unlock(&g_init_mutex);
    return 0;
}

/* ── drm_lib_login ──────────────────────────────────────────────────────────*/

int drm_lib_login(const char *username, const char *password)
{
    if (!username || !password) return -1;

    extern char *amUsername, *amPassword;
    amUsername = (char *)username;
    static char pw_buf[256];
    strncpy(pw_buf, password, sizeof(pw_buf) - 1);
    pw_buf[sizeof(pw_buf) - 1] = '\0';
    amPassword = pw_buf;
    amPasswordLen = strlen(pw_buf);

    extern uint8_t login(struct shared_ptr ctx);
    if (!login(reqCtx)) {
        fprintf(stderr, "[drm_lib] login failed\n");
        return -1;
    }
    DRM_DEBUG("[drm_lib] login ok\n");
    return 0;
}

/* ── drm_lib_shutdown ───────────────────────────────────────────────────────*/

void drm_lib_shutdown(void)
{
    /* No clean shutdown API in the underlying library — just reset callbacks
     * so no further callbacks fire after the Go engine destroys its context. */
    pthread_mutex_lock(&g_init_mutex);
    g_drm_auth_cb  = NULL;
    g_drm_state_cb = NULL;
    pthread_mutex_unlock(&g_init_mutex);
}

/* ── drm_lib_get_m3u8 ───────────────────────────────────────────────────────*/

char *drm_lib_get_m3u8(unsigned long adam_id)
{
    const char *url;
    if (offlineFlag)
        url = get_m3u8_method_download(reqCtx, adam_id, NULL);
    else
        url = get_m3u8_method_play(leaseMgr, adam_id, NULL);

    if (!url)
        return NULL;
    char *ret = strdup(url);
    free((void *)url);
    return ret;
}

/* ── drm_lib_get_account ────────────────────────────────────────────────────*/

char *drm_lib_get_account(void)
{
    if (!g_storefront_id || !g_dev_token || !g_music_token)
        return NULL;

    /* Build JSON: {"storefront_id":"…","dev_token":"…","music_token":"…"} */
    size_t len = strlen(g_storefront_id) + strlen(g_dev_token) +
                 strlen(g_music_token) + 80;
    char *buf = malloc(len);
    if (!buf)
        return NULL;
    snprintf(buf, len,
        "{\"storefront_id\":\"%s\",\"dev_token\":\"%s\",\"music_token\":\"%s\"}",
        g_storefront_id, g_dev_token, g_music_token);
    return buf;
}

/* ── drm_lib_get_mv ─────────────────────────────────────────────────────────*/

int drm_lib_get_mv(unsigned long adam_id, char **out_url, char **out_dk,
                   int *out_has_itun)
{
    char *dk = NULL;
    const char *url = get_progressive_method_play(leaseMgr, adam_id, &dk);
    if (!url) {
        url = get_m3u8_method_play(leaseMgr, adam_id, &dk);
    }
    if (!url)
        return -1;

    *out_url = strdup(url);
    free((void *)url);
    *out_dk = dk ? strdup(dk) : NULL;
    if (dk) free(dk);

    pthread_mutex_lock(&g_itun_mutex);
    *out_has_itun = (g_itun_decryptor.obj != NULL && g_itun_adam_id == adam_id) ? 1 : 0;
    pthread_mutex_unlock(&g_itun_mutex);

    return 0;
}

/* ── drm_lib_open_kd_ctx ────────────────────────────────────────────────────*/

void *drm_lib_open_kd_ctx(const char *adam_id, const char *uri)
{
    return getKdContext(adam_id, uri);
}

/* ── drm_lib_decrypt ────────────────────────────────────────────────────────*/

int drm_lib_decrypt(void *kd_ctx, uint8_t *sample, uint32_t size)
{
    if (!kd_ctx || !sample)
        return -1;
    /* getKdContext() returns the key-context slot, not the context itself:
     * handle() in main.c (the proven TCP path) calls the decryptor with
     * *kdContext. Passing kd_ctx directly decrypts with the wrong context and
     * yields garbage samples. */
    void *ctx = *(void **)kd_ctx;
    if (!ctx)
        return -1;
    NfcRKVnxuKZy04KWbdFu71Ou(ctx, (uint32_t)5, sample, sample, (size_t)size);
    return 0;
}

/* ── drm_lib_decrypt_itun ───────────────────────────────────────────────────*/

int drm_lib_decrypt_itun(unsigned long adam_id, uint8_t *sample,
                          uint32_t in_size, uint32_t *out_size)
{
    pthread_mutex_lock(&g_itun_mutex);
    void *dec_obj   = g_itun_decryptor.obj;
    unsigned long dec_adam = g_itun_adam_id;
    pthread_mutex_unlock(&g_itun_mutex);

    if (!dec_obj) {
        fprintf(stderr, "[drm_lib] itun: no decryptor (call drm_lib_get_mv first)\n");
        return -1;
    }
    if (dec_adam != adam_id) {
        fprintf(stderr, "[drm_lib] itun: adamId mismatch: have %lu, got %lu\n",
                dec_adam, adam_id);
        return -1;
    }

    uint32_t out_len = 0;
    _ZN17SVPastisDecryptor13decryptSampleEPKhRKjPj(dec_obj, sample, &in_size, &out_len);
    *out_size = out_len;
    return 0;
}

/* ── drm_lib_is_recovery_active ─────────────────────────────────────────────*/

int drm_lib_is_recovery_active(void)
{
    extern int is_recovery_active(void);
    return is_recovery_active();
}

/* ── drm_lib_debug_enabled ──────────────────────────────────────────────────*/

int drm_lib_debug_enabled(void)
{
    static int cached = -1;
    if (cached < 0) cached = getenv("ADAM_DRM_DEBUG") != NULL;
    return cached;
}
