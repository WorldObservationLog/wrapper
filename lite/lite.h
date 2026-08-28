#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>

/* ---- JSON response helpers ---- */
struct LiteResponse {
    int code;
    std::string msg;
    std::string body; /* JSON body for data field, or empty */
};

/* ---- Token cache (persisted to disk) ---- */
struct TokenCache {
    std::string dev_token;
    std::string music_token;
    std::string storefront_id;
    std::string base_dir;
    bool loaded = false;

    void load();
    void save();
};

extern TokenCache g_tokens;

/* ---- Minimal libcurl declarations ---- */
/* We dlopen libcurl.so at runtime to avoid compile-time header dependency. */
struct CurlHandle;
using CurlWriteCallback = size_t (*)(char*, size_t, size_t, void*);

struct CurlEasy {
    void* handle;
    bool ok;
    CurlEasy();
    ~CurlEasy();
    bool setOptInt(int opt, int val);
    bool setOptPtr(int opt, void* ptr);
    bool setOptStr(int opt, const char* str);
    bool perform();
    long getResponseCode();
};

/* CURL option constants */
constexpr int CURLOPT_URL         = 10002;
constexpr int CURLOPT_WRITEFUNCTION = 20011;
constexpr int CURLOPT_WRITEDATA   = 10001;
constexpr int CURLOPT_HTTPHEADER  = 10023;
constexpr int CURLOPT_POSTFIELDS  = 10015;
constexpr int CURLOPT_POSTFIELDSIZE = 20015;
constexpr int CURLOPT_USERAGENT   = 10018;
constexpr int CURLOPT_POST        = 47;
constexpr int CURLOPT_TIMEOUT_MS  = 155;
constexpr int CURLOPT_SSL_VERIFYPEER = 64;
constexpr int CURLOPT_SSL_VERIFYHOST = 81;
constexpr int CURLOPT_FOLLOWLOCATION = 52;
constexpr int CURLOPT_ACCEPT_ENCODING = 10102;
constexpr int CURLINFO_RESPONSE_CODE = 0x2000002;

struct CurlSlist {
    void* list;
    CurlSlist() : list(nullptr) {}
    ~CurlSlist();
    void append(const char* str);
};

/* ---- Apple API helpers ---- */
struct AppleApi {
    static std::string getDevToken();
    static std::string getLyrics(const std::string& adamId,
                                  const std::string& region,
                                  const std::string& language,
                                  bool syllable,
                                  const std::string& devToken,
                                  const std::string& musicToken);
    static std::string getWebPlayback(const std::string& adamId,
                                       const std::string& devToken,
                                       const std::string& musicToken);
    static bool getLicense(const std::string& adamId,
                            const std::string& challenge,
                            const std::string& uri,
                            const std::string& devToken,
                            const std::string& musicToken,
                            std::string& outLicense,
                            int& outRenew);
};

/* ---- sprintf-style safe string builder ---- */
std::string strfmt(const char* fmt, ...);

/* ---- Android library interaction (lite_android.cpp) ---- */
struct shared_ptr;
extern struct shared_ptr g_reqCtx;
extern bool g_ssl_verify_disabled;
extern bool g_code_from_file;

void install_hooks();
struct shared_ptr init_ctx();
bool login(struct shared_ptr ctx);
void set_device_info(const char* device_info);
void set_base_dir(const char* dir);
void set_credentials(const char* user, const char* pass);
/* Align original wrapper: offline accounts use download path, online use play path. */
std::string get_m3u8(const std::string& adamId);
std::string get_key(const std::string& adamId, const std::string& uri,
                             uint8_t* ctx, uint8_t* state,
                             uint64_t* rcx, uint64_t* rax, uint64_t* rdx,
                             uint64_t* r9, uint64_t* rbp);
std::string get_storefront();
std::string get_music_token();
std::string get_dev_token();
std::string fetch_dev_token();
void setup_services();
/* Login-only flow: fetch STOREFRONT_ID + MUSIC_TOKEN and persist them, then exit. */
bool cache_login_tokens();
/* Fetch STOREFRONT_ID/dev token/music token and persist STOREFRONT_ID + MUSIC_TOKEN,
 * without touching the shared g_tokens state (background refresh helper). */
bool refresh_tokens(std::string& out_storefront, std::string& out_dev_token, std::string& out_music_token);
extern void* FHinstance;
