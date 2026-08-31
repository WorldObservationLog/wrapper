#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <direct.h>
#define getpid _getpid
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static std::string g_host = "127.0.0.1";
static std::string g_hostPort = "8080";
static std::string g_guestPort = "8080";
static std::string g_memory = "512";
static std::string g_smp = "2";
static std::string g_forcedAccel;
static std::string g_qemuBin;

static std::string getEnv(const char* name, const std::string& def) {
    const char* v = std::getenv(name);
    return v && *v ? v : def;
}

static bool fileExists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

static std::string executableDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n == MAX_PATH) return "";
    std::string path(buf, n);
    size_t pos = path.find_last_of("\\/");
    return pos == std::string::npos ? "" : path.substr(0, pos);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return "";
    std::string path(buf);
    size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? "" : path.substr(0, pos);
#else
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    std::string path(buf);
    size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? "" : path.substr(0, pos);
#endif
}

static std::string qemuName() {
#ifdef _WIN32
    return "qemu-system-x86_64.exe";
#else
    return "qemu-system-x86_64";
#endif
}

static bool findOnPath(const std::string& name, std::string& out) {
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return false;
    std::string path(pathEnv);
#ifdef _WIN32
    const char sep = ';';
#else
    const char sep = ':';
#endif
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find(sep, start);
        if (end == std::string::npos) end = path.size();
        std::string dir = path.substr(start, end - start);
        if (!dir.empty()) {
            std::string candidate = dir + "/" + name;
            if (fileExists(candidate)) { out = candidate; return true; }
        }
        start = end + 1;
    }
    return false;
}

static std::string locateQemu(const std::string& dir) {
    if (!g_qemuBin.empty()) return g_qemuBin;
    std::string q = qemuName();
    std::string out;
    if (findOnPath(q, out)) return out;
    if (!dir.empty()) {
        std::string candidate = dir + "/bin/" + q;
        if (fileExists(candidate)) return candidate;
    }
    return qemuName();
}

static bool canUseKvm() {
#ifdef __linux__
    return access("/dev/kvm", R_OK | W_OK) == 0;
#else
    return false;
#endif
}

static std::string autoAccel() {
#ifdef __APPLE__
    /* HVF on Apple Silicon cannot accelerate x86_64 guests; use TCG. */
    return "tcg";
#elif defined(_WIN32)
    return "whpx";
#else
    return canUseKvm() ? "kvm" : "tcg";
#endif
}

static int spawnAndWait(const std::vector<std::string>& args) {
#ifdef _WIN32
    std::ostringstream cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) cmd << " ";
        std::string a = args[i];
        bool needQuote = a.empty() || a.find_first_of(" \\\"") != std::string::npos;
        if (needQuote) {
            cmd << "\"";
            for (char c : a) {
                if (c == '\"') cmd << "\\\"";
                else cmd << c;
            }
            cmd << "\"";
        } else {
            cmd << a;
        }
    }
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    std::string cmdline = cmd.str();
    if (!CreateProcessA(nullptr, &cmdline[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        std::fprintf(stderr, "[run] failed to start qemu\n");
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
#else
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return 1;
    }
    if (pid == 0) {
        execvp(argv[0], argv.data());
        std::perror("execvp");
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

static void writeArgsFile(const std::string& path, const std::vector<std::string>& liteArgs) {
    std::ofstream f(path, std::ios::out | std::ios::binary);
    for (const auto& a : liteArgs) f << a << "\n";
}


static std::string base64Encode(const std::string& data) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        unsigned n = ((unsigned char)data[i] << 16) | ((unsigned char)data[i+1] << 8) | (unsigned char)data[i+2];
        out += table[(n >> 18) & 63];
        out += table[(n >> 12) & 63];
        out += table[(n >> 6) & 63];
        out += table[n & 63];
    }
    if (i < data.size()) {
        unsigned n = (unsigned char)data[i] << 16;
        if (i + 1 < data.size()) n |= (unsigned char)data[i+1] << 8;
        out += table[(n >> 18) & 63];
        out += table[(n >> 12) & 63];
        out += (i + 1 < data.size()) ? table[(n >> 6) & 63] : '=';
        out += (i + 1 < data.size()) ? table[n & 63] : '=';
    }
    return out;
}

static std::vector<std::string> buildQemuArgs(const std::string& qemuBin,
                                              const std::string& accel,
                                              const std::string& dir,
                                              const std::string& argsFile) {
    std::vector<std::string> args;
    args.push_back(qemuBin);
    /* Point QEMU at the bundled firmware (SeaBIOS bios-256k.bin, vgabios,
       option ROMs, ...) shipped in qemu/bin for every platform, including
       Android (bundled from the Termux qemu packages). */
    args.push_back("-L");
    args.push_back(dir + "/bin");
    args.push_back("-accel");
    if (accel == "whpx") {
        args.push_back("whpx,kernel-irqchip=off");
    } else {
        args.push_back(accel);
    }
    if (accel == "kvm") {
        args.push_back("-cpu");
        args.push_back("host");
    } else if (accel == "whpx") {
        args.push_back("-cpu");
        args.push_back("qemu64-v1");
    } else {
        args.push_back("-cpu");
        args.push_back("max");
    }
    args.push_back("-m");
    args.push_back(g_memory);
    args.push_back("-smp");
    args.push_back(g_smp);
    args.push_back("-kernel");
    args.push_back(dir + "/vmlinuz-lite-qemu");
    args.push_back("-initrd");
    args.push_back(dir + "/lite-initramfs.cpio.gz");
    std::string appendStr = "console=ttyS0 quiet net.ifnames=0 biosdevname=0";
    {
        std::ifstream af(argsFile, std::ios::binary);
        std::ostringstream oss;
        oss << af.rdbuf();
        std::string content = oss.str();
        if (!content.empty()) {
            appendStr += " lite_args_b64=" + base64Encode(content);
        }
    }
    args.push_back("-append");
    args.push_back(appendStr);
    args.push_back("-display");
    args.push_back("none");
    args.push_back("-serial");
    args.push_back("stdio");
    args.push_back("-no-reboot");
    args.push_back("-nic");
    args.push_back("user,model=e1000,hostfwd=tcp:" + g_host + ":" + g_hostPort + "-:" + g_guestPort);
    args.push_back("-drive");
    args.push_back("file=" + dir + "/data.img,format=raw,if=virtio");
    std::ifstream af(argsFile);
    if (af.peek() != std::ifstream::traits_type::eof()) {
        args.push_back("-fw_cfg");
        args.push_back("name=lite_args,file=" + argsFile);
    }
    return args;
}

static int runQemu(const std::string& qemuBin, const std::string& accel,
                   const std::string& dir, const std::string& argsFile) {
    std::vector<std::string> qargs = buildQemuArgs(qemuBin, accel, dir, argsFile);
    std::fprintf(stderr, "[run] accel=%s qemu=%s\n", accel.c_str(), qemuBin.c_str());
    return spawnAndWait(qargs);
}

int main(int argc, char** argv) {
    /* Environment variables remain as fallbacks; command-line flags win. */
    g_host = getEnv("LITE_QEMU_HOST", "127.0.0.1");
    g_hostPort = getEnv("HOST_PORT", "8080");
    g_guestPort = getEnv("GUEST_PORT", "8080");
    g_memory = getEnv("MEMORY", "512");
    g_smp = getEnv("SMP", "2");
    g_forcedAccel = getEnv("LITE_QEMU_ACCEL", "");
    g_qemuBin = getEnv("QEMU_BIN", "");

    std::vector<std::string> liteArgs;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        bool wantsValue = (a == "--host" || a == "--host-port" || a == "--guest-port" ||
                           a == "--memory" || a == "--smp" || a == "--accel" || a == "--qemu-bin");
        if (wantsValue && i + 1 < argc) {
            std::string v = argv[++i];
            if (a == "--host") g_host = v;
            else if (a == "--host-port") g_hostPort = v;
            else if (a == "--guest-port") g_guestPort = v;
            else if (a == "--memory") g_memory = v;
            else if (a == "--smp") g_smp = v;
            else if (a == "--accel") g_forcedAccel = v;
            else if (a == "--qemu-bin") g_qemuBin = v;
        } else {
            liteArgs.push_back(a);
        }
    }

    std::string dir = executableDir();
    if (dir.empty()) dir = ".";
    std::string assetDir = dir + "/qemu";

    std::string qemuBin = locateQemu(assetDir);

    std::string argsFile = assetDir + "/.lite-qemu-args";
    writeArgsFile(argsFile, liteArgs);

    std::string accel = g_forcedAccel.empty() ? autoAccel() : g_forcedAccel;
    bool forced = !g_forcedAccel.empty();

    std::fprintf(stderr, "[run] starting wrapper-lite guest (host %s, port %s -> %s, mem %sMB)\n",
                 g_host.c_str(), g_hostPort.c_str(), g_guestPort.c_str(), g_memory.c_str());
    if (!liteArgs.empty()) std::fprintf(stderr, "[run] forwarding %zu argument(s) to wrapper-lite\n", liteArgs.size());

    int rc = runQemu(qemuBin, accel, assetDir, argsFile);
    if (rc != 0 && !forced && accel != "tcg") {
        std::fprintf(stderr, "[run] %s acceleration unavailable, falling back to tcg\n", accel.c_str());
        rc = runQemu(qemuBin, "tcg", assetDir, argsFile);
    }

    std::remove(argsFile.c_str());
    return rc;
}
