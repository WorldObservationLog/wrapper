#include "lite.h"
#include "cJSON.h"
#include "logger.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>
#include <regex>
#include <fstream>
#include <string>
#include <mutex>

/* ---- dlopen-based libcurl wrapper ---- */
static void* curl_lib = nullptr;
static std::mutex g_curl_mutex;
static void* getCurl() {
    std::lock_guard<std::mutex> lock(g_curl_mutex);
    if (!curl_lib) {
        curl_lib = dlopen("libcurl.so", RTLD_LAZY | RTLD_LOCAL);
        if (!curl_lib) {
            LOG_WARN("failed to dlopen libcurl.so: %s", dlerror());
        }
    }
    return curl_lib;
}
CurlEasy::CurlEasy() : handle(nullptr), ok(false) {
    void* lib = getCurl();
    if (!lib) return;
    typedef void* (*curl_easy_init_t)();
    curl_easy_init_t init = (curl_easy_init_t)dlsym(lib, "curl_easy_init");
    if (!init) { LOG_WARN("curl_easy_init not found"); return; }
    handle = init();
    ok = (handle != nullptr);
}

CurlEasy::~CurlEasy() {
    if (handle) {
        void* lib = getCurl();
        if (!lib) return;
        typedef void (*curl_easy_cleanup_t)(void*);
        curl_easy_cleanup_t cleanup = (curl_easy_cleanup_t)dlsym(lib, "curl_easy_cleanup");
        if (cleanup) cleanup(handle);
    }
}

bool CurlEasy::setOptInt(int opt, int val) {
    if (!handle) return false;
    void* lib = getCurl();
    if (!lib) return false;
    typedef int (*curl_easy_setopt_t)(void*, int, int);
    curl_easy_setopt_t setopt = (curl_easy_setopt_t)dlsym(lib, "curl_easy_setopt");
    return setopt && setopt(handle, opt, val) == 0;
}

bool CurlEasy::setOptStr(int opt, const char* str) {
    if (!handle) return false;
    void* lib = getCurl();
    if (!lib) return false;
    typedef int (*curl_easy_setopt_t)(void*, int, const char*);
    curl_easy_setopt_t setopt = (curl_easy_setopt_t)dlsym(lib, "curl_easy_setopt");
    return setopt && setopt(handle, opt, str) == 0;
}

bool CurlEasy::setOptPtr(int opt, void* ptr) {
    if (!handle) return false;
    void* lib = getCurl();
    if (!lib) return false;
    typedef int (*curl_easy_setopt_t)(void*, int, ...);
    curl_easy_setopt_t setopt = (curl_easy_setopt_t)dlsym(lib, "curl_easy_setopt");
    return setopt && setopt(handle, opt, ptr) == 0;
}

bool CurlEasy::perform() {
    if (!handle) return false;
    void* lib = getCurl();
    if (!lib) return false;
    typedef int (*curl_easy_perform_t)(void*);
    curl_easy_perform_t perf = (curl_easy_perform_t)dlsym(lib, "curl_easy_perform");
    return perf && perf(handle) == 0;
}

long CurlEasy::getResponseCode() {
    if (!handle) return -1;
    void* lib = getCurl();
    if (!lib) return -1;
    typedef int (*curl_easy_getinfo_t)(void*, int, long*);
    curl_easy_getinfo_t gi = (curl_easy_getinfo_t)dlsym(lib, "curl_easy_getinfo");
    if (!gi) return -1;
    long code = 0;
    gi(handle, CURLINFO_RESPONSE_CODE, &code);
    return code;
}

CurlSlist::~CurlSlist() {
    if (list) {
        void* lib = getCurl();
        if (!lib) return;
        typedef void (*curl_slist_free_all_t)(void*);
        curl_slist_free_all_t sf = (curl_slist_free_all_t)dlsym(lib, "curl_slist_free_all");
        if (sf) sf(list);
    }
}

void CurlSlist::append(const char* str) {
    void* lib = getCurl();
    if (!lib) return;
    typedef void* (*curl_slist_append_t)(void*, const char*);
    curl_slist_append_t sa = (curl_slist_append_t)dlsym(lib, "curl_slist_append");
    if (sa) list = sa(list, str);
}

/* ---- Write callback: append to string ---- */
static size_t writeCallback(char* data, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* str = (std::string*)userp;
    str->append(data, total);
    return total;
}

/* ---- Token management (base_dir-aware, atomic writes) ---- */
void TokenCache::load() {
    std::string path = (base_dir.empty() ? "data" : base_dir) + "/token_cache.json";
    std::ifstream f(path);
    if (!f) return;
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    cJSON* root = cJSON_Parse(content.c_str());
    if (!root) return;
    cJSON* dev = cJSON_GetObjectItemCaseSensitive(root, "dev_token");
    cJSON* music = cJSON_GetObjectItemCaseSensitive(root, "music_token");
    cJSON* storefront = cJSON_GetObjectItemCaseSensitive(root, "storefront_id");
    if (cJSON_IsString(dev)) dev_token = dev->valuestring;
    if (cJSON_IsString(music)) music_token = music->valuestring;
    if (cJSON_IsString(storefront)) storefront_id = storefront->valuestring;
    loaded = !dev_token.empty() || !music_token.empty();
    cJSON_Delete(root);
    if (loaded)
        LOG_INFO("token cache loaded (dev=%.16s... music=%.16s...)",
                dev_token.c_str(), music_token.c_str());
}

void TokenCache::save() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "dev_token", dev_token.c_str());
    cJSON_AddStringToObject(root, "music_token", music_token.c_str());
    cJSON_AddStringToObject(root, "storefront_id", storefront_id.c_str());
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!printed) {
        LOG_WARN("failed to serialize token cache");
        return;
    }

    std::string path = (base_dir.empty() ? "data" : base_dir) + "/token_cache.json";
    std::string tmp = path + ".tmp";
    FILE* fp = fopen(tmp.c_str(), "w");
    if (!fp) {
        LOG_WARN("failed to write token cache");
        cJSON_free(printed);
        return;
    }
    fwrite(printed, 1, strlen(printed), fp);
    fclose(fp);
    cJSON_free(printed);

    if (rename(tmp.c_str(), path.c_str()) != 0) {
        LOG_WARN("failed to replace token cache");
        remove(tmp.c_str());
        return;
    }
    LOG_INFO("token cache saved");
}

TokenCache g_tokens;

/* ---- Dev token scraping (from music.apple.com) ---- */
std::string AppleApi::getDevToken() {
    CurlEasy curl;
    if (!curl.ok) return "";

    std::string html;
    curl.setOptStr(CURLOPT_URL, "https://music.apple.com");
    curl.setOptPtr(CURLOPT_WRITEFUNCTION, (void*)&writeCallback);
    curl.setOptPtr(CURLOPT_WRITEDATA, &html);
    curl.setOptInt(CURLOPT_SSL_VERIFYPEER, 0);
    curl.setOptInt(CURLOPT_SSL_VERIFYHOST, 0);
    curl.setOptInt(CURLOPT_FOLLOWLOCATION, 1);
    curl.setOptInt(CURLOPT_TIMEOUT_MS, 15000);
    curl.setOptStr(CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    curl.setOptStr(CURLOPT_ACCEPT_ENCODING, "");
    if (!curl.perform()) return "";

    std::regex jsRe("/assets/index~[^/]+\\.js");
    std::smatch match;
    if (!std::regex_search(html, match, jsRe)) return "";
    std::string jsPath = match.str();

    std::string js;
    CurlEasy jsCurl;
    if (!jsCurl.ok) return "";
    jsCurl.setOptStr(CURLOPT_URL, ("https://music.apple.com" + jsPath).c_str());
    jsCurl.setOptPtr(CURLOPT_WRITEFUNCTION, (void*)&writeCallback);
    jsCurl.setOptPtr(CURLOPT_WRITEDATA, &js);
    jsCurl.setOptInt(CURLOPT_SSL_VERIFYPEER, 0);
    jsCurl.setOptInt(CURLOPT_SSL_VERIFYHOST, 0);
    jsCurl.setOptInt(CURLOPT_FOLLOWLOCATION, 1);
    jsCurl.setOptInt(CURLOPT_TIMEOUT_MS, 15000);
    jsCurl.setOptStr(CURLOPT_ACCEPT_ENCODING, "");
    if (!jsCurl.perform()) return "";

    std::regex tokenRe("eyJ[A-Za-z0-9-_=]+\\.[A-Za-z0-9-_=]+\\.[A-Za-z0-9-_=]+");
    std::smatch tokenMatch;
    if (!std::regex_search(js, tokenMatch, tokenRe)) return "";
    return tokenMatch.str();
}

/* ---- Lyrics ---- */
std::string AppleApi::getLyrics(const std::string& adamId,
                                  const std::string& region,
                                  const std::string& language,
                                  const std::string& devToken,
                                  const std::string& musicToken) {
    CurlEasy curl;
    if (!curl.ok) return "";

    std::string url = strfmt(
        "https://amp-api.music.apple.com/v1/catalog/%s/songs/%s/syllable-lyrics"
        "?l[lyrics]=%s&extend=ttmlLocalizations&l[script]=en-Latn",
        region.c_str(), adamId.c_str(), language.c_str());

    std::string resp;
    curl.setOptStr(CURLOPT_URL, url.c_str());
    curl.setOptPtr(CURLOPT_WRITEFUNCTION, (void*)&writeCallback);
    curl.setOptPtr(CURLOPT_WRITEDATA, &resp);
    curl.setOptInt(CURLOPT_SSL_VERIFYPEER, 0);
    curl.setOptInt(CURLOPT_SSL_VERIFYHOST, 0);
    curl.setOptInt(CURLOPT_TIMEOUT_MS, 15000);
    curl.setOptStr(CURLOPT_USERAGENT, "Music/5.7 Android/10 model/Pixel6GR1YH build/1234 (dt:66)");

    CurlSlist headers;
    headers.append(strfmt("Authorization: Bearer %s", devToken.c_str()).c_str());
    headers.append(strfmt("media-user-token: %s", musicToken.c_str()).c_str());
    headers.append("Origin: https://music.apple.com");
    curl.setOptPtr(CURLOPT_HTTPHEADER, headers.list);

    if (!curl.perform()) { LOG_WARN("lyrics curl perform failed"); return ""; }
    long lstatus = curl.getResponseCode();
    if (lstatus != 200 && lstatus != 0) {
        LOG_WARN("lyrics http status=%ld body=%.300s", lstatus, resp.c_str());
        return "";
    }

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root) { LOG_WARN("lyrics bad json: %.200s", resp.c_str()); return ""; }
    std::string lyrics;
    cJSON* errors = cJSON_GetObjectItemCaseSensitive(root, "errors");
    if (errors && cJSON_GetArraySize(errors) > 0) {
        cJSON_Delete(root);
        LOG_WARN("lyrics API errors: %.200s", resp.c_str());
        return "";
    }
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
        cJSON* first = cJSON_GetArrayItem(data, 0);
        cJSON* attrs = cJSON_GetObjectItemCaseSensitive(first, "attributes");
        cJSON* ttml = cJSON_GetObjectItemCaseSensitive(attrs, "ttmlLocalizations");
        if (cJSON_IsString(ttml)) lyrics = ttml->valuestring;
    }
    cJSON_Delete(root);
    return lyrics;
}

/* ---- WebPlayback ---- */
std::string AppleApi::getWebPlayback(const std::string& adamId,
                                      const std::string& devToken,
                                      const std::string& musicToken) {
    CurlEasy curl;
    if (!curl.ok) return "";

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "salableAdamId", adamId.c_str());
    char* printed = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    std::string postBody = printed ? printed : "";
    if (printed) cJSON_free(printed);
    std::string resp;
    curl.setOptStr(CURLOPT_URL, "https://play.music.apple.com/WebObjects/MZPlay.woa/wa/webPlayback");
    curl.setOptPtr(CURLOPT_WRITEFUNCTION, (void*)&writeCallback);
    curl.setOptPtr(CURLOPT_WRITEDATA, &resp);
    curl.setOptInt(CURLOPT_SSL_VERIFYPEER, 0);
    curl.setOptInt(CURLOPT_SSL_VERIFYHOST, 0);
    curl.setOptInt(CURLOPT_TIMEOUT_MS, 15000);
    curl.setOptStr(CURLOPT_POSTFIELDS, postBody.c_str());
    curl.setOptInt(CURLOPT_POST, 1);

    CurlSlist headers;
    /* The webPlayback endpoint requires the media dev token in the
       Authorization header plus the account's music user token. Omitting
       Authorization makes Apple reject the request with
       {"failureType":"2002","customerMessage":"Your session has ended..."}. */
    headers.append(strfmt("Authorization: Bearer %s", devToken.c_str()).c_str());
    headers.append(strfmt("X-Apple-Music-User-Token: %s", musicToken.c_str()).c_str());
    headers.append("Content-Type: application/json");
    headers.append("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36");
    curl.setOptPtr(CURLOPT_HTTPHEADER, headers.list);

    if (!curl.perform()) { LOG_WARN("webplayback curl perform failed"); return ""; }
    long wstatus = curl.getResponseCode();
    if (wstatus != 200 && wstatus != 0) {
        LOG_WARN("webplayback http status=%ld body=%.300s", wstatus, resp.c_str());
        return "";
    }

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root) { LOG_WARN("webplayback bad json: %.200s", resp.c_str()); return ""; }
    std::string m3u8;

    cJSON* failureType = cJSON_GetObjectItemCaseSensitive(root, "failureType");
    if (cJSON_IsString(failureType)) {
        LOG_WARN("webplayback rejected failureType=%s body=%.200s",
                 failureType->valuestring, resp.c_str());
        cJSON_Delete(root);
        return "";
    }

    cJSON* errors = cJSON_GetObjectItemCaseSensitive(root, "errors");
    if (errors && cJSON_GetArraySize(errors) > 0) {
        cJSON_Delete(root);
        LOG_WARN("webplayback API errors: %.200s", resp.c_str());
        return "";
    }

    cJSON* songList = cJSON_GetObjectItemCaseSensitive(root, "songList");
    if (cJSON_IsArray(songList) && cJSON_GetArraySize(songList) > 0) {
        cJSON* first = cJSON_GetArrayItem(songList, 0);
        cJSON* hls = cJSON_GetObjectItemCaseSensitive(first, "hls-playlist-url");
        if (cJSON_IsString(hls)) m3u8 = hls->valuestring;

        if (m3u8.empty()) {
            cJSON* assets = cJSON_GetObjectItemCaseSensitive(first, "assets");
            if (cJSON_IsArray(assets)) {
                int n = cJSON_GetArraySize(assets);
                for (int i = 0; i < n; i++) {
                    cJSON* asset = cJSON_GetArrayItem(assets, i);
                    cJSON* flavor = cJSON_GetObjectItemCaseSensitive(asset, "flavor");
                    if (cJSON_IsString(flavor) && strcmp(flavor->valuestring, "28:ctrp256") == 0) {
                        cJSON* url = cJSON_GetObjectItemCaseSensitive(asset, "URL");
                        if (cJSON_IsString(url)) {
                            m3u8 = url->valuestring;
                            break;
                        }
                    }
                }
            }
        }
    }
    cJSON_Delete(root);
    return m3u8;
}

/* ---- License ---- */
bool AppleApi::getLicense(const std::string& adamId,
                           const std::string& challenge,
                           const std::string& uri,
                           const std::string& devToken,
                           const std::string& musicToken,
                           std::string& outLicense,
                           int& outRenew) {
    CurlEasy curl;
    if (!curl.ok) return false;

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "challenge", challenge.c_str());
    cJSON_AddStringToObject(req, "uri", uri.c_str());
    cJSON_AddStringToObject(req, "key-system", "com.widevine.alpha");
    cJSON_AddStringToObject(req, "adamId", adamId.c_str());
    cJSON_AddBoolToObject(req, "isLibrary", 0);
    cJSON_AddBoolToObject(req, "user-initiated", 1);
    char* printed = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    std::string postBody = printed ? printed : "";
    if (printed) cJSON_free(printed);

    std::string resp;
    curl.setOptStr(CURLOPT_URL, "https://play.itunes.apple.com/WebObjects/MZPlay.woa/wa/acquireWebPlaybackLicense");
    curl.setOptPtr(CURLOPT_WRITEFUNCTION, (void*)&writeCallback);
    curl.setOptPtr(CURLOPT_WRITEDATA, &resp);
    curl.setOptInt(CURLOPT_SSL_VERIFYPEER, 0);
    curl.setOptInt(CURLOPT_SSL_VERIFYHOST, 0);
    curl.setOptInt(CURLOPT_TIMEOUT_MS, 15000);
    curl.setOptStr(CURLOPT_POSTFIELDS, postBody.c_str());
    curl.setOptInt(CURLOPT_POST, 1);
    curl.setOptInt(CURLOPT_POSTFIELDSIZE, (int)postBody.size());

    CurlSlist headers;
    headers.append(strfmt("Authorization: Bearer %s", devToken.c_str()).c_str());
    headers.append(strfmt("X-Apple-Music-User-Token: %s", musicToken.c_str()).c_str());
    headers.append("Content-Type: application/json");
    curl.setOptPtr(CURLOPT_HTTPHEADER, headers.list);

    if (!curl.perform()) return false;
    if (curl.getResponseCode() != 200) return false;

    cJSON* root = cJSON_Parse(resp.c_str());
    if (!root) return false;

    bool ok = false;
    cJSON* errors = cJSON_GetObjectItemCaseSensitive(root, "errors");
    cJSON* license = cJSON_GetObjectItemCaseSensitive(root, "license");
    cJSON* renewAfter = cJSON_GetObjectItemCaseSensitive(root, "renew-after");
    if ((!errors || cJSON_GetArraySize(errors) == 0) && cJSON_IsString(license)) {
        outLicense = license->valuestring;
        outRenew = renewAfter ? (int)cJSON_GetNumberValue(renewAfter) : 0;
        ok = true;
    }
    cJSON_Delete(root);
    return ok;
}

/* ---- strfmt ---- */
std::string strfmt(const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return std::string(buf);
}
