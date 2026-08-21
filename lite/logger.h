#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <chrono>
#include <mutex>
#include <string>

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3
};

/* Minimal thread-safe logger with optional file output and size-based
 * rotation.  All wrapper-lite components share this single logger. */
class Logger {
public:
    static Logger& get() {
        static Logger instance;
        return instance;
    }

    void configure(LogLevel level, const std::string& file, size_t maxSize) {
        std::lock_guard<std::mutex> lock(mutex_);
        level_ = level;
        if (!file.empty()) {
            if (file_ && file_path_ != file) {
                fclose(file_);
                file_ = nullptr;
            }
            file_path_ = file;
            file_ = fopen(file_path_.c_str(), "a");
            if (!file_) {
                fprintf(stderr, "[LOGGER] failed to open log file %s\n", file_path_.c_str());
            }
        }
        maxSize_ = maxSize;
    }

    bool enabled(LogLevel level) const {
        return (int)level >= (int)level_;
    }

    void log(LogLevel level, const char* fmt, ...) {
        char buffer[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        std::lock_guard<std::mutex> lock(mutex_);
        rotate_if_needed_locked();

        std::string line = format_line(level, buffer);
        fprintf(stderr, "%s", line.c_str());
        if (file_) {
            fprintf(file_, "%s", line.c_str());
            fflush(file_);
        }
    }

private:
    Logger() : level_(LogLevel::Info), maxSize_(5 * 1024 * 1024), file_(nullptr) {}

    static const char* level_name(LogLevel level) {
        switch (level) {
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO ";
            case LogLevel::Warn:  return "WARN ";
            case LogLevel::Error: return "ERROR";
        }
        return "INFO ";
    }

    static int days_from_civil(int y, unsigned m, unsigned d) {
        y -= m <= 2;
        int era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (int)doe - 719468;
    }

    static void civil_from_days(int z, int& y, unsigned& m, unsigned& d) {
        z += 719468;
        int era = (z >= 0 ? z : z - 146096) / 146097;
        unsigned doe = (unsigned)(z - era * 146097);
        unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        y = (int)yoe + era * 400;
        unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        unsigned mp = (5 * doy + 2) / 153;
        d = doy - (153 * mp + 2) / 5 + 1;
        m = mp + (mp < 10 ? 3 : -9);
        y += (m <= 2);
    }

    static std::string timestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        long long secs = (long long)duration_cast<seconds>(now.time_since_epoch()).count();
        long long days = secs / 86400;
        long long rem = secs % 86400;
        if (rem < 0) { rem += 86400; --days; }
        int y; unsigned m, d;
        civil_from_days((int)days, y, m, d);
        int hh = (int)(rem / 3600); rem %= 3600;
        int mm = (int)(rem / 60);
        int ss = (int)(rem % 60);
        char out[40];
        snprintf(out, sizeof(out), "%04d-%02d-%02d %02d:%02d:%02d.%03lld",
                 y, m, d, hh, mm, ss, (long long)ms.count());
        return out;
    }

    std::string format_line(LogLevel level, const char* msg) {
        std::string out;
        out += timestamp();
        out += " [";
        out += level_name(level);
        out += "] ";
        out += msg;
        if (out.empty() || out.back() != '\n') out += '\n';
        return out;
    }

    void rotate_if_needed_locked() {
        if (!file_ || maxSize_ == 0) return;
        long size = ftell(file_);
        if (size < 0) size = 0;
        if ((size_t)size >= maxSize_) {
            fclose(file_);
            file_ = nullptr;
            std::string old = file_path_ + ".1";
            remove(old.c_str());
            rename(file_path_.c_str(), old.c_str());
            file_ = fopen(file_path_.c_str(), "a");
            if (!file_) {
                fprintf(stderr, "[LOGGER] failed to reopen log file %s\n", file_path_.c_str());
            }
        }
    }

    std::mutex mutex_;
    LogLevel level_;
    std::string file_path_;
    size_t maxSize_;
    FILE* file_;
};

#define LOG_DEBUG(...) do { Logger& _logger = Logger::get(); if (_logger.enabled(LogLevel::Debug)) _logger.log(LogLevel::Debug, __VA_ARGS__); } while (0)
#define LOG_INFO(...)  do { Logger& _logger = Logger::get(); if (_logger.enabled(LogLevel::Info))  _logger.log(LogLevel::Info,  __VA_ARGS__); } while (0)
#define LOG_WARN(...)  do { Logger& _logger = Logger::get(); if (_logger.enabled(LogLevel::Warn))  _logger.log(LogLevel::Warn,  __VA_ARGS__); } while (0)
#define LOG_ERROR(...) do { Logger& _logger = Logger::get(); if (_logger.enabled(LogLevel::Error)) _logger.log(LogLevel::Error, __VA_ARGS__); } while (0)
