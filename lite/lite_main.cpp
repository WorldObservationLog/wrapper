#include "lite.h"
#include "import.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <mutex>
#include <chrono>
#include <cctype>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include "cJSON.h"
#include "logger.h"

/* ---- httplib.h (header-only HTTP server) ---- */
/* NDK API 22 does not declare getifaddrs in headers, but it is available at runtime */
/* Provide stub implementations (never called since we bind to a fixed host) */
#include <ifaddrs.h>
extern "C" int getifaddrs(struct ifaddrs** __list_ptr) { return -1; }
extern "C" void freeifaddrs(struct ifaddrs* __ptr) { }
#ifndef CPPHTTPLIB_THREAD_POOL_COUNT
#define CPPHTTPLIB_THREAD_POOL_COUNT 8
#endif
#include "httplib.h"

/* ---- Configuration ---- */
static std::string g_host = "127.0.0.1";
static int g_port = 8080;
static bool g_login_only = false;
static std::string g_login_credentials;
static std::string g_proxy;
static std::string g_device_info = "Music/5.0.2/Android/10/Pixel 10/7663314/en-US/en-US/dc28071e371c439e";
static std::string g_base_dir = "data";
static std::string g_log_level = "info";
static std::string g_log_file;
static int g_token_refresh_interval = 1800;

/* ---- Token state and background refresh ---- */
static std::mutex g_tokens_mutex;
static std::string g_web_dev_token;
static std::chrono::steady_clock::time_point g_last_web_dev_attempt;
static std::atomic<bool> g_refresh_stop{false};
static httplib::Server* g_svr = nullptr;
static sigset_t g_signal_set;

struct WebTokens {
    std::string dev_token;
    std::string music_token;
    std::string storefront_id;
    bool valid() const { return !dev_token.empty() && !music_token.empty(); }
};

/* ============================== */
/*     JSON helpers (cJSON)       */
/* ============================== */

static std::string json_dump(cJSON* root) {
    char* p = cJSON_PrintUnformatted(root);
    if (!p) return "{}";
    std::string s(p);
    cJSON_free(p);
    return s;
}

static std::string json_success(cJSON* data) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "code", 0);
    cJSON_AddStringToObject(root, "msg", "SUCCESS");
    if (data) cJSON_AddItemToObject(root, "data", data);
    else cJSON_AddNullToObject(root, "data");
    std::string s = json_dump(root);
    cJSON_Delete(root);
    return s;
}

static std::string json_error(int code, const std::string& msg) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddStringToObject(root, "msg", msg.c_str());
    std::string s = json_dump(root);
    cJSON_Delete(root);
    return s;
}

#include "storefront_ids.inc"

/* The Android lib writes storefront identifiers like "143462-1,31"; the Apple
 * Music web API expects a two-letter country code ("jp"). */
static std::string normalize_storefront_id(const std::string& raw) {
    if (raw.empty()) return "";
    if (raw.size() == 2 && std::isalpha((unsigned char)raw[0]) &&
        std::isalpha((unsigned char)raw[1])) {
        return raw;
    }
    std::string first = raw.substr(0, raw.find('-'));
    std::string code = builtin_storefront_region(first);
    return code.empty() ? "us" : code;
}

/* ============================== */
/*     Token cache helpers        */
/* ============================== */

static void load_token_cache() {
    std::lock_guard<std::mutex> lock(g_tokens_mutex);
    g_tokens.base_dir = g_base_dir;
    g_tokens.load();
    if (g_tokens.music_token.empty()) g_tokens.music_token = get_music_token();
    if (g_tokens.storefront_id.empty()) g_tokens.storefront_id = get_storefront();
    g_tokens.storefront_id = normalize_storefront_id(g_tokens.storefront_id);
    if (g_tokens.storefront_id.empty()) g_tokens.storefront_id = "us";
}

/* Caller must hold g_tokens_mutex. */
static void save_token_cache() {
    g_tokens.base_dir = g_base_dir;
    g_tokens.save();
}

/* Fetch dev token for Lyrics/License/WebPlayback.  The web endpoints must use
 * the WebPlay dev token embedded in music.apple.com's JS (iss=AMPWebPlay,
 * kid=WebPlayKid) - exactly what wrapper-manager's GetToken() returns and the
 * token the browser sends.  The Android storeservicescore token
 * (iss=UDK28SN10P) is accepted by webPlayback and lyrics but REJECTED by
 * acquireWebPlaybackLicense (Apple returns HTTP 500), so it is only a
 * fallback when the scrape is unavailable. */
static WebTokens get_web_tokens() {
    std::lock_guard<std::mutex> lock(g_tokens_mutex);
    if (g_tokens.music_token.empty()) g_tokens.music_token = get_music_token();
    if (g_tokens.storefront_id.empty()) g_tokens.storefront_id = get_storefront();
    g_tokens.storefront_id = normalize_storefront_id(g_tokens.storefront_id);
    if (g_tokens.storefront_id.empty()) g_tokens.storefront_id = "us";

    auto now = std::chrono::steady_clock::now();
    if (g_web_dev_token.empty()) {
        /* retry the scrape at most every 60 s until it succeeds */
        if (g_last_web_dev_attempt == std::chrono::steady_clock::time_point{} ||
            now - g_last_web_dev_attempt > std::chrono::seconds(60)) {
            g_last_web_dev_attempt = now;
            g_web_dev_token = AppleApi::getDevToken();
        }
    } else if (now - g_last_web_dev_attempt > std::chrono::hours(24)) {
        /* refresh the long-lived WebPlay token once a day (wrapper-manager) */
        g_last_web_dev_attempt = now;
        std::string fresh = AppleApi::getDevToken();
        if (!fresh.empty()) g_web_dev_token = fresh;
    }

    WebTokens tokens;
    tokens.dev_token = g_web_dev_token.empty() ? g_tokens.dev_token : g_web_dev_token;
    tokens.music_token = g_tokens.music_token;
    tokens.storefront_id = g_tokens.storefront_id;
    return tokens;
}

/* ---- b64 encode (for ctx/state) ---- */
static std::string b64_encode(const uint8_t* data, size_t len) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = data[i] << 16;
        if (i + 1 < len) n |= data[i + 1] << 8;
        if (i + 2 < len) n |= data[i + 2];
        out += tbl[(n >> 18) & 0x3f];
        out += tbl[(n >> 12) & 0x3f];
        out += (i + 1 < len) ? tbl[(n >> 6) & 0x3f] : '=';
        out += (i + 2 < len) ? tbl[n & 0x3f] : '=';
    }
    return out;
}


/* ============================== */
/*     HTTP Handlers              */
/* ============================== */

static void handle_m3u8(const httplib::Request& req, httplib::Response& res) {
    auto adamId = req.get_param_value("adamId");
    if (adamId.empty()) {
        res.set_content(json_error(400, "missing adamId"), "application/json");
        return;
    }

    std::string m3u8 = get_m3u8(adamId);
    if (m3u8.empty()) {
        res.set_content(json_error(404, "failed to get m3u8"), "application/json");
        return;
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "adamId", adamId.c_str());
    cJSON_AddStringToObject(data, "m3u8", m3u8.c_str());
    res.set_content(json_success(data), "application/json");
}

static void handle_key(const httplib::Request& req, httplib::Response& res) {
    auto adamId = req.get_param_value("adamId");
    auto uri = req.get_param_value("uri");
    if (adamId.empty()) {
        res.set_content(json_error(400, "missing adamId"), "application/json");
        return;
    }
    if (uri.empty()) uri = "skd://itunes.apple.com/P000000000/s1/e1";

    uint8_t cap_ctx[0x8000], cap_state[0x2100];
    uint64_t rcx = 0, rax = 0, rdx = 0, r9 = 0, rbp = 0;
    std::string contentKey = get_key(adamId, uri, cap_ctx, cap_state,
                                              &rcx, &rax, &rdx, &r9, &rbp);
    if (contentKey.empty()) {
        res.set_content(json_error(500, "key retrieval failed"), "application/json");
        return;
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "adamId", adamId.c_str());
    cJSON_AddStringToObject(data, "keyUri", uri.c_str());
    cJSON_AddStringToObject(data, "contentKey", contentKey.c_str());
    if (rcx || rax || rdx || r9 || rbp) {
        cJSON_AddStringToObject(data, "ctx", b64_encode(cap_ctx, 0x8000).c_str());
        cJSON_AddStringToObject(data, "state", b64_encode(cap_state, 0x2100).c_str());
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "0x%llx", (unsigned long long)rcx);
        cJSON_AddStringToObject(data, "rcx", tmp);
        snprintf(tmp, sizeof(tmp), "0x%llx", (unsigned long long)rax);
        cJSON_AddStringToObject(data, "rax", tmp);
        snprintf(tmp, sizeof(tmp), "0x%llx", (unsigned long long)rdx);
        cJSON_AddStringToObject(data, "rdx", tmp);
        snprintf(tmp, sizeof(tmp), "0x%llx", (unsigned long long)r9);
        cJSON_AddStringToObject(data, "r9", tmp);
        snprintf(tmp, sizeof(tmp), "0x%llx", (unsigned long long)rbp);
        cJSON_AddStringToObject(data, "rbp", tmp);
    }
    res.set_content(json_success(data), "application/json");
}

static void handle_lyrics(const httplib::Request& req, httplib::Response& res) {
    auto adamId = req.get_param_value("adamId");
    auto language = req.get_param_value("language");
    auto syllable = req.get_param_value("syllable");
    if (adamId.empty()) {
        res.set_content(json_error(400, "missing adamId"), "application/json");
        return;
    }
    if (language.empty()) language = "en";
    /* syllable=0|false|no -> standard /lyrics; default (and syllable=1) ->
       word-timed /syllable-lyrics. */
    bool use_syllable = true;
    if (!syllable.empty()) {
        std::string s = syllable;
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        if (s == "0" || s == "false" || s == "no" || s == "off") use_syllable = false;
    }

    WebTokens tokens = get_web_tokens();
    if (!tokens.valid()) {
        res.set_content(json_error(500, "missing music/dev token, run --login first"), "application/json");
        return;
    }

    std::string lyrics = AppleApi::getLyrics(adamId, tokens.storefront_id, language,
                                              use_syllable, tokens.dev_token, tokens.music_token);
    if (lyrics.empty()) {
        res.set_content(json_error(404, "lyrics not found"), "application/json");
        return;
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "adamId", adamId.c_str());
    cJSON_AddStringToObject(data, "syllable", use_syllable ? "true" : "false");
    cJSON_AddStringToObject(data, "lyrics", lyrics.c_str());
    res.set_content(json_success(data), "application/json");
}

static void handle_webplayback(const httplib::Request& req, httplib::Response& res) {
    auto adamId = req.get_param_value("adamId");
    if (adamId.empty()) {
        res.set_content(json_error(400, "missing adamId"), "application/json");
        return;
    }

    WebTokens tokens = get_web_tokens();
    if (!tokens.valid()) {
        res.set_content(json_error(500, "missing music/dev token, run --login first"), "application/json");
        return;
    }

    std::string m3u8 = AppleApi::getWebPlayback(adamId, tokens.dev_token, tokens.music_token);
    if (m3u8.empty()) {
        res.set_content(json_error(404, "webplayback not available"), "application/json");
        return;
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "adamId", adamId.c_str());
    cJSON_AddStringToObject(data, "m3u8", m3u8.c_str());
    res.set_content(json_success(data), "application/json");
}

static void handle_license(const httplib::Request& req, httplib::Response& res) {
    cJSON* req_json = cJSON_Parse(req.body.c_str());
    if (!req_json) {
        res.set_content(json_error(400, "invalid JSON body"), "application/json");
        return;
    }

    const char* adamId_s = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(req_json, "adamId"));
    const char* challenge_s = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(req_json, "challenge"));
    const char* uri_s = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(req_json, "uri"));
    std::string adamId = adamId_s ? adamId_s : "";
    std::string challenge = challenge_s ? challenge_s : "";
    std::string uri = uri_s ? uri_s : "";
    cJSON_Delete(req_json);

    if (adamId.empty() || challenge.empty() || uri.empty()) {
        res.set_content(json_error(400, "missing adamId, challenge, or uri"), "application/json");
        return;
    }

    WebTokens tokens = get_web_tokens();
    if (!tokens.valid()) {
        res.set_content(json_error(500, "missing music/dev token, run --login first"), "application/json");
        return;
    }

    std::string license;
    int renew = 0;
    if (!AppleApi::getLicense(adamId, challenge, uri, tokens.dev_token, tokens.music_token, license, renew)) {
        res.set_content(json_error(500, "license acquisition failed"), "application/json");
        return;
    }

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "adamId", adamId.c_str());
    cJSON_AddStringToObject(data, "license", license.c_str());
    cJSON_AddNumberToObject(data, "renew", renew);
    res.set_content(json_success(data), "application/json");
}


/* ============================== */
/*     Background tasks           */
/* ============================== */

static void token_refresh_worker() {
    while (!g_refresh_stop.load()) {
        for (int i = 0; i < g_token_refresh_interval && !g_refresh_stop.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (g_refresh_stop.load()) break;

        LOG_INFO("background token refresh starting");
        std::string sf, dev, music;
        if (refresh_tokens(sf, dev, music)) {
            std::lock_guard<std::mutex> lock(g_tokens_mutex);
            if (!sf.empty()) {
                g_tokens.storefront_id = normalize_storefront_id(sf);
            }
            if (g_tokens.storefront_id.empty()) g_tokens.storefront_id = "us";
            if (!dev.empty()) g_tokens.dev_token = dev;
            if (!music.empty()) g_tokens.music_token = music;
            save_token_cache();
            LOG_INFO("background token refresh completed");
        } else {
            LOG_WARN("background token refresh failed");
        }
    }
}

static void signal_worker() {
    while (!g_refresh_stop.load()) {
        int sig = 0;
        if (sigwait(&g_signal_set, &sig) == 0) {
            if (sig == SIGINT || sig == SIGTERM) {
                LOG_INFO("received signal %d, stopping service", sig);
                g_refresh_stop.store(true);
                if (g_svr) g_svr->stop();
                break;
            }
        }
    }
}

/* ============================== */
/*     Main entry point           */
/* ============================== */

static void print_usage() {
    LOG_INFO("usage: lite [--login user:pass] [--host 127.0.0.1] [--port 8080]");
    LOG_INFO("            [--device-info STR] [--base-dir data] [--proxy URL] [--debug]");
    LOG_INFO("            [--log-level debug|info|warn|error] [--log-file PATH]");
    LOG_INFO("            [--token-refresh-interval SECONDS]");
}

static LogLevel parse_log_level(const std::string& s) {
    if (s == "debug") return LogLevel::Debug;
    if (s == "warn") return LogLevel::Warn;
    if (s == "error") return LogLevel::Error;
    return LogLevel::Info;
}

int main(int argc, char* argv[]) {
    setenv("ANDROID_DATA", "/data", 1);
    setenv("ANDROID_ROOT", "/system", 1);
    setenv("TZ", "UTC", 1);

    std::vector<std::string> cmdline_args;
    cmdline_args.push_back(argv[0]);
    const char* args_file = getenv("LITE_ARGS_FILE");
    if (args_file && *args_file) {
        std::ifstream af(args_file);
        std::string line;
        while (std::getline(af, line)) {
            if (!line.empty() && (line.back() == '\r')) line.pop_back();
                        line.erase(std::remove_if(line.begin(), line.end(), [](char c){ return c == 0; }), line.end());
            if (line.empty()) continue;
            size_t b = line.find_first_not_of(" 	");
            if (b == std::string::npos) continue;
            size_t e = line.find_last_not_of(" 	");
            line = line.substr(b, e - b + 1);
            if (!line.empty()) cmdline_args.push_back(line);
        }
    } else {
        for (int i = 1; i < argc; i++) cmdline_args.push_back(argv[i]);
    }
    for (size_t i = 1; i < cmdline_args.size(); i++) {
        std::string arg = cmdline_args[i];
        if (arg == "--host" && i + 1 < cmdline_args.size()) g_host = cmdline_args[++i];
        else if (arg == "--port" && i + 1 < cmdline_args.size()) g_port = atoi(cmdline_args[++i].c_str());
        else if (arg == "--debug") { g_log_level = "debug"; g_ssl_verify_disabled = true; }
        else if (arg == "--code-from-file") g_code_from_file = true;
        else if (arg == "--log-level" && i + 1 < cmdline_args.size()) g_log_level = cmdline_args[++i];
        else if (arg == "--log-file" && i + 1 < cmdline_args.size()) g_log_file = cmdline_args[++i];
        else if (arg == "--token-refresh-interval" && i + 1 < cmdline_args.size()) g_token_refresh_interval = atoi(cmdline_args[++i].c_str());
        else if (arg == "--proxy" && i + 1 < cmdline_args.size()) {
            g_proxy = cmdline_args[++i];
            setenv("all_proxy", g_proxy.c_str(), 1);
        }
        else if (arg == "--login" && i + 1 < cmdline_args.size()) {
            g_login_only = true;
            g_login_credentials = cmdline_args[++i];
        }
        else if (arg == "--device-info" && i + 1 < cmdline_args.size()) g_device_info = cmdline_args[++i];
        else if (arg == "--base-dir" && i + 1 < cmdline_args.size()) g_base_dir = cmdline_args[++i];
        else if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }
        else {
            LOG_ERROR("unknown argument: %s", arg.c_str());
            print_usage();
            return 1;
        }
    }

    if (g_token_refresh_interval <= 0) g_token_refresh_interval = 1800;
    Logger::get().configure(parse_log_level(g_log_level), g_log_file, 5 * 1024 * 1024);
    set_base_dir(g_base_dir.c_str());
    g_tokens.base_dir = g_base_dir;

    if (g_login_only) {
        auto colon = g_login_credentials.find(':');
        if (colon == std::string::npos) {
            LOG_ERROR("invalid login format, expected user:pass");
            return 1;
        }
        std::string username = g_login_credentials.substr(0, colon);
        std::string password = g_login_credentials.substr(colon + 1);
        set_credentials(username.c_str(), password.c_str());

        LOG_INFO("wrapper-lite login mode");

        /* Run the whole login + native token refresh in a child process.
         * On Termux + QEMU TCG the Android URLRequest destructors corrupt the
         * heap; isolating login in a child keeps this process's heap intact. */
        pid_t pid = fork();
        if (pid < 0) {
            LOG_ERROR("fork failed");
            return 1;
        }
        if (pid == 0) {
            install_hooks();
            set_device_info(g_device_info.c_str());
            g_reqCtx = init_ctx();
            if (!login(g_reqCtx)) {
                LOG_ERROR("login failed");
                _exit(1);
            }
            LOG_INFO("login successful");
            if (!cache_login_tokens()) {
                LOG_ERROR("failed to cache account info");
                _exit(1);
            }
            _exit(0);
        }

        int status = 0;
        waitpid(pid, &status, 0);

        /* Read token files (written by the child) into this clean process. */
        auto readFile = [](const std::string& path) {
            std::ifstream f(path);
            if (!f) return std::string();
            std::string c((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            while (!c.empty() && (c.back() == ' ' || c.back() == 10 || c.back() == 13)) c.pop_back();
            return c;
        };
        g_tokens.base_dir = g_base_dir;
        g_tokens.storefront_id = readFile(std::string(g_base_dir) + "/STOREFRONT_ID");
        g_tokens.dev_token = readFile(std::string(g_base_dir) + "/DEV_TOKEN");
        g_tokens.music_token = readFile(std::string(g_base_dir) + "/MUSIC_TOKEN");
        if (g_tokens.storefront_id.empty()) g_tokens.storefront_id = "us";
        if (g_tokens.dev_token.empty() || g_tokens.music_token.empty()) {
            LOG_ERROR("failed to cache account info");
            return 1;
        }
        {
            std::lock_guard<std::mutex> lock(g_tokens_mutex);
            save_token_cache();
        }
        LOG_INFO("login complete, exiting");
        return 0;
    }


    /* Service mode */
    /* Block termination signals before Android/Apple libraries spawn their
       internal threads; new threads inherit this mask.  Our signal_worker
       consumes SIGINT/SIGTERM via sigwait and stops the HTTP server. */
    sigemptyset(&g_signal_set);
    sigaddset(&g_signal_set, SIGINT);
    sigaddset(&g_signal_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &g_signal_set, nullptr);

    install_hooks();
    set_device_info(g_device_info.c_str());
    g_reqCtx = init_ctx();
    setup_services();
    load_token_cache();
    WebTokens initial_tokens = get_web_tokens();
    if (!initial_tokens.valid()) {
        LOG_WARN("missing music/dev token, run --login first");
    }

    httplib::Server svr;

    svr.set_read_timeout(10, 0);
    svr.set_write_timeout(30, 0);
    svr.set_keep_alive_max_count(256);
    svr.set_keep_alive_timeout(30);
    svr.set_payload_max_length(1 << 20);
    svr.set_tcp_nodelay(true);

    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        (void)res;
        LOG_INFO("request: %s %s", req.method.c_str(), req.target.c_str());
        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.Get("/m3u8", handle_m3u8);
    svr.Get("/key", handle_key);
    svr.Get("/lyrics", handle_lyrics);
    svr.Get("/webplayback", handle_webplayback);
    svr.Post("/license", handle_license);

    svr.Get("/status", [](const httplib::Request&, httplib::Response& res) {
        std::string storefront;
        {
            std::lock_guard<std::mutex> lock(g_tokens_mutex);
            storefront = g_tokens.storefront_id;
        }
        cJSON* data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "storefront", storefront.c_str());
        res.set_content(json_success(data), "application/json");
    });

    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404)
            res.set_content(json_error(404, "not found"), "application/json");
    });
    svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr ep) {
        try {
            if (ep) std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            LOG_ERROR("handler exception: %s", e.what());
        } catch (...) {
            LOG_ERROR("handler exception: unknown");
        }
        res.set_content(json_error(500, "internal error"), "application/json");
    });

    g_refresh_stop.store(false);
    std::thread refresh_thread(token_refresh_worker);
    std::thread sig_thread(signal_worker);
    g_svr = &svr;

    LOG_INFO("wrapper-lite listening on %s:%d", g_host.c_str(), g_port);
    svr.listen(g_host.c_str(), g_port);

    g_refresh_stop.store(true);
    pthread_kill(sig_thread.native_handle(), SIGTERM);
    if (refresh_thread.joinable()) refresh_thread.join();
    if (sig_thread.joinable()) sig_thread.join();
    {
        std::lock_guard<std::mutex> lock(g_tokens_mutex);
        save_token_cache();
    }
    g_svr = nullptr;
    LOG_INFO("wrapper-lite stopped");

    return 0;
}
