#include "internal.h"
#include "dobby.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <mutex>
#include <dlfcn.h>
#include <link.h>

extern const char* const fairplayCert;

const char* get_m3u8_download(unsigned long adam) {
    void* purchase_request = malloc(1024);
    memset(purchase_request, 0, 1024);
    _ZN17storeservicescore15PurchaseRequestC2ERKNSt6__ndk110shared_ptrINS_14RequestContextEEE(purchase_request, &g_reqCtx);
    _ZN17storeservicescore15PurchaseRequest23setProcessDialogActionsEb(purchase_request, 1);
    union std_string urlBagKey = new_std_string("subDownload");
    _ZN17storeservicescore15PurchaseRequest12setURLBagKeyERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(purchase_request, &urlBagKey);
    char buyParams[128];
    snprintf(buyParams, sizeof(buyParams), "salableAdamId=%lu&price=0&pricingParameters=SUBS&productType=S", adam);
    union std_string bp = new_std_string(buyParams);
    _ZN17storeservicescore15PurchaseRequest16setBuyParametersERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(purchase_request, &bp);
    _ZN17storeservicescore15PurchaseRequest3runEv(purchase_request);
    struct shared_ptr* response = _ZNK17storeservicescore15PurchaseRequest8responseEv(purchase_request);
    if (!response) { free(purchase_request); return nullptr; }
    struct shared_ptr* error = _ZN17storeservicescore16PurchaseResponse5errorEv(response->obj);
    if (!error || error->obj) { free(purchase_request); return nullptr; }

    struct std_vector items = _ZNK17storeservicescore16PurchaseResponse5itemsEv(response->obj);
    if (!items.begin) { free(purchase_request); return nullptr; }
    struct shared_ptr* firstItem = (struct shared_ptr*)items.begin;
    struct std_vector assets = _ZNK17storeservicescore12PurchaseItem6assetsEv(firstItem->obj);
    struct shared_ptr* lastAsset = (struct shared_ptr*)((uint8_t*)assets.end - sizeof(struct shared_ptr));
    union std_string* url_str = (union std_string*)malloc(sizeof(union std_string));
    memset(url_str, 0, sizeof(union std_string));
    _ZNK17storeservicescore13PurchaseAsset3URLEv(url_str, lastAsset->obj);
    const char* url = std_string_data(url_str);
    if (url) {
        char* result = strdup(url);
        free(url_str); free(purchase_request);
        return result;
    }
    free(url_str); free(purchase_request);
    return nullptr;
}

const char* get_m3u8_play(unsigned long adam) {
    union std_string HLS = new_std_string("HLS");
    struct std_vector HLSParam;
    memset(&HLSParam, 0, sizeof(HLSParam));
    HLSParam.begin = (struct shared_ptr*)malloc(sizeof(union std_string));
    memcpy(HLSParam.begin, &HLS, sizeof(union std_string));
    HLSParam.end = (struct shared_ptr*)((uint8_t*)HLSParam.begin + 1);
    HLSParam.end_capacity = HLSParam.end;

    static uint8_t z0 = 0;
    struct shared_ptr ptr_result;
    memset(&ptr_result, 0, sizeof(ptr_result));
    _ZN22SVPlaybackLeaseManager12requestAssetERKmRKNSt6__ndk16vectorINS2_12basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEENS7_IS9_EEEERKb(
        &ptr_result, leaseMgr, &adam, &HLSParam, &z0);
    free(HLSParam.begin);

    if (!ptr_result.obj) return nullptr;
    if (!_ZNK23SVPlaybackAssetResponse13hasValidAssetEv(ptr_result.obj)) return nullptr;

    struct shared_ptr* playbackAsset = _ZNK23SVPlaybackAssetResponse13playbackAssetEv(ptr_result.obj);
    if (!playbackAsset || !playbackAsset->obj) return nullptr;

    union std_string* m3u8 = (union std_string*)malloc(sizeof(union std_string));
    memset(m3u8, 0, sizeof(union std_string));
    _ZNK17storeservicescore13PlaybackAsset9URLStringEv(m3u8, (uint8_t*)playbackAsset->obj);

    const char* m3u8_str = std_string_data(m3u8);
    if (m3u8_str) {
        char* result = strdup(m3u8_str);
        free(m3u8);
        return result;
    }
    free(m3u8);
    return nullptr;
}

static char* get_content_key_impl(const std::string& adamId, const std::string& keyUri) {
    union std_string defaultId = new_std_string(adamId.c_str());
    union std_string keyUriStr = new_std_string(keyUri.c_str());
    union std_string keyFormat = new_std_string("com.apple.streamingkeydelivery");
    union std_string keyFormatVer = new_std_string("1");
    union std_string serverUri = new_std_string("https://play.itunes.apple.com/WebObjects/MZPlay.woa/music/fps");
    union std_string protocolType = new_std_string("simplified");
    union std_string fpsCertStr = new_std_string(fairplayCert);

    struct shared_ptr persistK;
    memset(&persistK, 0, sizeof(persistK));
    _ZN21SVFootHillSessionCtrl16getPersistentKeyERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_S8_S8_S8_S8_S8_(
        &persistK, FHinstance, &defaultId, &defaultId, &keyUriStr, &keyFormat,
        &keyFormatVer, &serverUri, &protocolType, &fpsCertStr);

    if (!persistK.obj) return nullptr;
    union std_string* pkey = (union std_string*)persistK.obj;
    const char* data = std_string_data(pkey);
    if (!data || !*data) return nullptr;
    return strdup(data);
}

static volatile int g_cap_armed = 0;
static volatile int g_cap_done = 0;
static int g_r1_hooked = 0;
static uint8_t g_cap_ctx[0x8000];
static uint8_t g_cap_state[0x2100];
static uint64_t g_cap_rcx, g_cap_rax, g_cap_rdx, g_cap_r9, g_cap_rbp;

extern "C" void r1_capture_cb(RegisterContext* ctx, const HookEntryInfo* info) {
    (void)info;
    if (!g_cap_armed || g_cap_done) return;
    if ((ctx->general.regs.rsi & 0xff) != 8) return;
    uint64_t r9 = ctx->general.regs.r9;
    uint64_t rbp = ctx->general.regs.rbp;
    memcpy(g_cap_ctx, (void*)(uintptr_t)r9, 0x8000);
    memcpy(g_cap_state, (void*)(uintptr_t)(rbp - 0x2000), 0x2100);
    g_cap_rcx = ctx->general.regs.rcx;
    g_cap_rax = ctx->general.regs.rax;
    g_cap_rdx = ctx->general.regs.rdx;
    g_cap_r9 = r9;
    g_cap_rbp = rbp;
    g_cap_done = 1;
}

static int _find_lib_cb(struct dl_phdr_info* info, size_t size, void* data) {
    (void)size;
    if (info->dlpi_name && strstr(info->dlpi_name, "libCoreLSKD.so")) {
        *(uintptr_t*)data = info->dlpi_addr;
        return 1;
    }
    return 0;
}
static uintptr_t get_lib_core_lskd_base(void) {
    uintptr_t base = 0;
    dl_iterate_phdr(_find_lib_cb, &base);
    return base;
}
static void setup_r1_hook(void) {
    if (g_r1_hooked) return;
    uintptr_t base = get_lib_core_lskd_base();
    if (!base) { LOG_WARN("libCoreLSKD not loaded"); return; }
    void* r1 = (void*)(base + 0x1d5709);
    if (DobbyInstrument(r1, r1_capture_cb) == 0) {
        g_r1_hooked = 1;
        LOG_DEBUG("R1 hook installed @ %p (base 0x%lx)", r1, base);
    } else {
        LOG_WARN("R1 hook install failed @ %p", r1);
    }
}

static void* preshareCtx = nullptr;
static void* getKdContext(const std::string& adam, const std::string& uri) {
    uint8_t isPreshare = (adam == "0");
    if (isPreshare && preshareCtx) return preshareCtx;
    LOG_INFO("adamId: %s, uri: %s", adam.c_str(), uri.c_str());

    union std_string defaultId = new_std_string(adam.c_str());
    union std_string keyUri = new_std_string(uri.c_str());
    union std_string keyFormat = new_std_string("com.apple.streamingkeydelivery");
    union std_string keyFormatVer = new_std_string("1");
    union std_string serverUri = new_std_string("https://play.itunes.apple.com/WebObjects/MZPlay.woa/music/fps");
    union std_string protocolType = new_std_string("simplified");
    union std_string fpsCert = new_std_string(fairplayCert);

    struct shared_ptr persistK = {.obj = nullptr};
    _ZN21SVFootHillSessionCtrl16getPersistentKeyERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_S8_S8_S8_S8_S8_(
        &persistK, FHinstance, &defaultId, &defaultId, &keyUri, &keyFormat,
        &keyFormatVer, &serverUri, &protocolType, &fpsCert);
    if (!persistK.obj) return nullptr;

    struct shared_ptr SVFootHillPContext = {.obj = nullptr, .ctrl_blk = nullptr};
    _ZN21SVFootHillSessionCtrl14decryptContextERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEERKN11SVDecryptor15SVDecryptorTypeERKb(
        &SVFootHillPContext, FHinstance, (union std_string*)persistK.obj);
    if (!SVFootHillPContext.obj) return nullptr;

    void* kdContext = *_ZNK18SVFootHillPContext9kdContextEv(SVFootHillPContext.obj);
    if (kdContext && isPreshare) preshareCtx = kdContext;
    return kdContext;
}

static void refresh_decrypt_ctx() {
    uint8_t autom = 1;
    _ZN22SVPlaybackLeaseManager12requestLeaseERKb(leaseMgr, &autom);
    _ZN21SVFootHillSessionCtrl16resetAllContextsEv(FHinstance);
    preshareCtx = nullptr;
    preshareCtx = getKdContext("0", "skd://itunes.apple.com/P000000000/s1/e1");
    LOG_WARN("refreshed context");
}

static int capture_content_template(const std::string& adam, const std::string& uri,
                                     uint8_t* cap_ctx, uint8_t* cap_state,
                                     uint64_t* rcx, uint64_t* rax, uint64_t* rdx,
                                     uint64_t* r9, uint64_t* rbp, int with_refresh) {
    if (with_refresh) refresh_decrypt_ctx();
    void* kdPtr = (void*)getKdContext(adam, uri);
    if (!kdPtr || !*(void**)kdPtr) { LOG_WARN("getKdContext failed"); return -1; }
    void* kd = *(void**)kdPtr;
    setup_r1_hook();
    if (!g_r1_hooked) return -1;
    g_cap_armed = 1;
    g_cap_done = 0;
    uint8_t dummy[64] = {0};
    NfcRKVnxuKZy04KWbdFu71Ou(kd, 5, dummy, dummy, 64);
    g_cap_armed = 0;
    if (!g_cap_done) { LOG_WARN("R1 capture triggered but no data"); return -1; }
    memcpy(cap_ctx, g_cap_ctx, 0x8000);
    memcpy(cap_state, g_cap_state, 0x2100);
    *rcx = g_cap_rcx; *rax = g_cap_rax; *rdx = g_cap_rdx;
    *r9 = g_cap_r9; *rbp = g_cap_rbp;
    return 0;
}

std::string get_m3u8(const std::string& adamId) {
    unsigned long adamID = strtoul(adamId.c_str(), nullptr, 10);
    if (adamID == 0) return "";
    std::lock_guard<std::mutex> lock(g_playback_mutex);
    const char* m3u8 = nullptr;
    if (offlineFlag) {
        m3u8 = get_m3u8_download(adamID);
        if (!m3u8) m3u8 = get_m3u8_play(adamID);
    } else {
        m3u8 = get_m3u8_play(adamID);
        if (!m3u8) m3u8 = get_m3u8_download(adamID);
    }
    if (!m3u8) return "";
    std::string result(m3u8);
    free((void*)m3u8);
    return result;
}

std::string get_key(const std::string& adamId, const std::string& uri,
                    uint8_t* ctx, uint8_t* state,
                    uint64_t* rcx, uint64_t* rax, uint64_t* rdx,
                    uint64_t* r9, uint64_t* rbp) {
    std::lock_guard<std::mutex> lock(g_playback_mutex);
    char* ck = get_content_key_impl(adamId, uri);
    if (!ck) return "";
    std::string result(ck);
    free(ck);
    if (ctx && state) {
        if (capture_content_template(adamId, uri, ctx, state, rcx, rax, rdx, r9, rbp, 0) != 0) {
            LOG_INFO("capture without refresh failed, retry with refresh");
            if (capture_content_template(adamId, uri, ctx, state, rcx, rax, rdx, r9, rbp, 1) != 0) {
                LOG_WARN("content template capture failed");
            }
        }
    }
    return result;
}
