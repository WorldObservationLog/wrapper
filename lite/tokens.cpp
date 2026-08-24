#include "internal.h"
#include "cJSON.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <mutex>
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>

static std::string read_token_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    while (!content.empty() && (content.back() == ' ' || content.back() == '\n' ||
                                 content.back() == '\r')) content.pop_back();
    return content;
}

std::string get_storefront() {
    return read_token_file(std::string(g_base_dir) + "/STOREFRONT_ID");
}

std::string get_music_token() {
    return read_token_file(std::string(g_base_dir) + "/MUSIC_TOKEN");
}

long long getCurrentTimeMillis() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

static char* get_guid() {
    char* ret[2];
    _ZN17storeservicescore10DeviceGUID4guidEv(ret, GUID.obj);
    char* guid = _ZNK13mediaplatform4Data5bytesEv(ret[0]);
    return strdup(guid);
}

static char* get_dev_token_impl() {
    static uint8_t ptr[480];
    memset(ptr, 0, sizeof(ptr));
    *(void**)ptr = &_ZTVNSt6__ndk120__shared_ptr_emplaceIN13mediaplatform11HTTPMessageENS_9allocatorIS2_EEEE + 2;
    struct shared_ptr httpMessage = {.obj = ptr + 32, .ctrl_blk = ptr};
    union std_string url = new_std_string("https://sf-api-token-service.itunes.apple.com/apiToken");
    union std_string method = new_std_string("GET");
    _ZN13mediaplatform11HTTPMessageC2ENSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES7_(httpMessage.obj, &url, &method);

    uint8_t urlRequest[512];
    memset(urlRequest, 0, sizeof(urlRequest));
    _ZN17storeservicescore10URLRequestC2ERKNSt6__ndk110shared_ptrIN13mediaplatform11HTTPMessageEEERKNS2_INS_14RequestContextEEE(urlRequest, &httpMessage, &g_reqCtx);
    union std_string clientIdName = new_std_string("clientId");
    union std_string clientIdValue = new_std_string("musicAndroid");
    _ZN17storeservicescore10URLRequest19setRequestParameterERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(urlRequest, &clientIdName, &clientIdValue);
    union std_string versionName = new_std_string("version");
    union std_string versionValue = new_std_string("1");
    _ZN17storeservicescore10URLRequest19setRequestParameterERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(urlRequest, &versionName, &versionValue);
    _ZN17storeservicescore10URLRequest3runEv(urlRequest);

    struct shared_ptr* err = _ZNK17storeservicescore10URLRequest5errorEv(urlRequest);
    if (err->obj != nullptr) {
        LOG_WARN("devToken error");
        return nullptr;
    }
    struct shared_ptr* urlResp = _ZNK17storeservicescore10URLRequest8responseEv(urlRequest);
    struct shared_ptr* resp = _ZNK17storeservicescore11URLResponse18underlyingResponseEv(urlResp->obj);
    void* http_message_obj = resp->obj;
    void** data_ptr_location = (void**)((char*)http_message_obj + 48);
    void* data_ptr = *data_ptr_location;
    char* respBody = _ZNK13mediaplatform4Data5bytesEv(data_ptr);
    if (!respBody) return nullptr;
    std::string respBodyStr(respBody, strnlen(respBody, 4096));
    const std::string key = "\"token\":\"";
    size_t ks = respBodyStr.find(key);
    if (ks == std::string::npos) return nullptr;
    ks += key.size();
    size_t ke = respBodyStr.find('"', ks);
    if (ke == std::string::npos) return nullptr;
    return strdup(respBodyStr.substr(ks, ke - ks).c_str());
}

static char* get_music_user_token_impl(const char* authToken) {
    char* guid = get_guid();
    if (!guid) return nullptr;

    static uint8_t ptr[480];
    memset(ptr, 0, sizeof(ptr));
    *(void**)ptr = &_ZTVNSt6__ndk120__shared_ptr_emplaceIN13mediaplatform11HTTPMessageENS_9allocatorIS2_EEEE + 2;
    struct shared_ptr httpMessage = {.obj = ptr + 32, .ctrl_blk = ptr};
    union std_string url = new_std_string("https://play.itunes.apple.com/WebObjects/MZPlay.woa/wa/createMusicToken");
    union std_string method = new_std_string("POST");
    _ZN13mediaplatform11HTTPMessageC2ENSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES7_(httpMessage.obj, &url, &method);

    union std_string ctHeader = new_std_string("Content-Type");
    union std_string ctValue = new_std_string("application/json; charset=UTF-8");
    _ZN13mediaplatform11HTTPMessage9setHeaderERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(httpMessage.obj, &ctHeader, &ctValue);
    union std_string expectHeader = new_std_string("Expect");
    union std_string expectValue = new_std_string("");
    _ZN13mediaplatform11HTTPMessage9setHeaderERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(httpMessage.obj, &expectHeader, &expectValue);
    union std_string bundleIdHeader = new_std_string("X-Apple-Requesting-Bundle-Id");
    union std_string bundleIdValue = new_std_string("com.apple.android.music");
    _ZN13mediaplatform11HTTPMessage9setHeaderERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(httpMessage.obj, &bundleIdHeader, &bundleIdValue);
    union std_string bundleVerHeader = new_std_string("X-Apple-Requesting-Bundle-Version");
    union std_string bundleVerValue = new_std_string("Music/4.9 Android/10 model/Samsung S9 build/7663313 (dt:66)");
    _ZN13mediaplatform11HTTPMessage9setHeaderERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(httpMessage.obj, &bundleVerHeader, &bundleVerValue);
    union std_string guidHeader = new_std_string("X-Apple-Store-Front");
    union std_string guidValue = new_std_string(g_base_dir);
    _ZN13mediaplatform11HTTPMessage9setHeaderERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(httpMessage.obj, &guidHeader, &guidValue);

    cJSON* bodyJson = cJSON_CreateObject();
    cJSON_AddStringToObject(bodyJson, "guid", guid);
    cJSON_AddStringToObject(bodyJson, "assertion", authToken ? authToken : "");
    char tcc[32];
    snprintf(tcc, sizeof(tcc), "%lld", (long long)getCurrentTimeMillis());
    cJSON_AddStringToObject(bodyJson, "tcc-acceptance-date", tcc);
    char* body = cJSON_PrintUnformatted(bodyJson);
    cJSON_Delete(bodyJson);
    if (body) {
        _ZN13mediaplatform11HTTPMessage11setBodyDataEPcm(httpMessage.obj, body, strlen(body));
        cJSON_free(body);
    }

    uint8_t urlRequest[512];
    memset(urlRequest, 0, sizeof(urlRequest));
    _ZN17storeservicescore10URLRequestC2ERKNSt6__ndk110shared_ptrIN13mediaplatform11HTTPMessageEEERKNS2_INS_14RequestContextEEE(urlRequest, &httpMessage, &g_reqCtx);
    _ZN17storeservicescore10URLRequest3runEv(urlRequest);
    free(guid);

    struct shared_ptr* err = _ZNK17storeservicescore10URLRequest5errorEv(urlRequest);
    if (err->obj != nullptr) {
        LOG_WARN("createMusicToken error");
        return nullptr;
    }
    struct shared_ptr* urlResp = _ZNK17storeservicescore10URLRequest8responseEv(urlRequest);
    struct shared_ptr* resp = _ZNK17storeservicescore11URLResponse18underlyingResponseEv(urlResp->obj);
    void* http_message_obj = resp->obj;
    void** data_ptr_location = (void**)((char*)http_message_obj + 48);
    void* data_ptr = *data_ptr_location;
    char* respBody = _ZNK13mediaplatform4Data5bytesEv(data_ptr);
    if (!respBody) return nullptr;
    std::string respBodyStr(respBody, strnlen(respBody, 4096));
    const std::string key = "\"music_token\":\"";
    size_t ks = respBodyStr.find(key);
    if (ks == std::string::npos) {
        LOG_WARN("createMusicToken failed: no music_token in response");
        return nullptr;
    }
    ks += key.size();
    size_t ke = respBodyStr.find('"', ks);
    if (ke == std::string::npos) return nullptr;
    return strdup(respBodyStr.substr(ks, ke - ks).c_str());
}

static char* get_account_storefront_id_impl() {
    union std_string* region = (union std_string*)malloc(sizeof(union std_string));
    memset(region, 0, sizeof(union std_string));
    struct shared_ptr urlbag = {.obj = 0x0, .ctrl_blk = 0x0};
    _ZNK17storeservicescore14RequestContext20storeFrontIdentifierERKNSt6__ndk110shared_ptrINS_6URLBagEEE(region, g_reqCtx.obj, &urlbag);
    const char* region_str = std_string_data(region);
    if (region_str) {
        char* result = strdup(region_str);
        free(region);
        return result;
    }
    free(region);
    return nullptr;
}

int offline_available_impl() {
    struct shared_ptr* fairplay = (struct shared_ptr*)malloc(16);
    memset(fairplay, 0, 16);
    _ZN17storeservicescore14RequestContext8fairPlayEv(fairplay, g_reqCtx.obj);
    if (!fairplay->obj) {
        free(fairplay);
        return 0;
    }
    struct std_vector fairplay_status = _ZN17storeservicescore8FairPlay21getSubscriptionStatusEv(fairplay->obj);
    char* begin_ptr = (char*)fairplay_status.begin;
    char* end_ptr = (char*)fairplay_status.end;
    free(fairplay);
    if (!begin_ptr || !end_ptr || (end_ptr - begin_ptr) < 32) return 0;
    char* second_item_ptr = begin_ptr + 16;
    int state = *(int*)((char*)second_item_ptr + 8);
    if (state == 2 || state == 3) return 1;
    return 0;
}

static bool write_token_file(const std::string& path, const std::string& content) {
    std::string tmp = path + ".tmp";
    FILE* fp = fopen(tmp.c_str(), "w");
    if (!fp) {
        LOG_WARN("failed to write %s", path.c_str());
        return false;
    }
    fwrite(content.data(), 1, content.size(), fp);
    fclose(fp);
    if (rename(tmp.c_str(), path.c_str()) != 0) {
        LOG_WARN("failed to rename %s -> %s", tmp.c_str(), path.c_str());
        remove(tmp.c_str());
        return false;
    }
    return true;
}

bool refresh_tokens(std::string& out_storefront, std::string& out_dev_token, std::string& out_music_token) {
    std::lock_guard<std::mutex> lock(g_token_mutex);

    LOG_INFO("refresh_tokens: fetching storefront");
    char* storefront = get_account_storefront_id_impl();
    LOG_INFO("refresh_tokens: storefront fetched");
    if (!storefront) {
        LOG_WARN("failed to get storefront ID");
        return false;
    }
    std::string sf(storefront);
    free(storefront);

    LOG_INFO("refresh_tokens: fetching dev token");
    char* devToken = get_dev_token_impl();
    LOG_INFO("refresh_tokens: dev token fetched");
    if (!devToken) {
        LOG_WARN("failed to get dev token");
        return false;
    }
    write_token_file(std::string(g_base_dir) + "/DEV_TOKEN", std::string(devToken));
    LOG_INFO("refresh_tokens: dev token file written");

    LOG_INFO("refresh_tokens: fetching music token");
    char* musicToken = get_music_user_token_impl(devToken);
    LOG_INFO("refresh_tokens: music token fetched");
    std::string music;
    if (musicToken) {
        music = musicToken;
        free(musicToken);
        LOG_INFO("refresh_tokens: music freed");
    } else {
        music = read_token_file(std::string(g_base_dir) + "/MUSIC_TOKEN");
        if (!music.empty()) {
            LOG_WARN("using cached MUSIC_TOKEN (%.14s...)", music.c_str());
        }
    }

    if (music.empty()) {
        LOG_WARN("failed to get music token");
        return false;
    }

    write_token_file(std::string(g_base_dir) + "/STOREFRONT_ID", sf);
    LOG_INFO("refresh_tokens: storefront file written");
    write_token_file(std::string(g_base_dir) + "/MUSIC_TOKEN", music);
    LOG_INFO("refresh_tokens: music file written");

    std::string freshDev = read_token_file(std::string(g_base_dir) + "/DEV_TOKEN");
    LOG_INFO("refresh_tokens: assigning out_storefront");
    out_storefront = std::move(sf);
    LOG_INFO("refresh_tokens: assigning out_dev_token");
    out_dev_token = std::move(freshDev);
    LOG_INFO("refresh_tokens: assigning out_music_token");
    out_music_token = std::move(music);
    LOG_INFO("refresh_tokens: outputs assigned");
    return true;
}

bool cache_login_tokens() {
    std::string sf, dev, music;
    if (!refresh_tokens(sf, dev, music)) {
        return false;
    }
    g_tokens.storefront_id = sf;
    g_tokens.dev_token = dev;
    g_tokens.music_token = music;
    LOG_INFO("account info cached successfully");
    return true;
}

std::string fetch_dev_token() {
    std::lock_guard<std::mutex> lock(g_token_mutex);
    char* token = get_dev_token_impl();
    if (!token) return "";
    std::string result(token);
    free(token);
    return result;
}

std::string get_dev_token() { return g_tokens.dev_token; }
