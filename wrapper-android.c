#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#include "cmdline.h"

pid_t child_proc = -1;
struct gengetopt_args_info args_info;

static void intHan(int signum) {
    if (child_proc != -1) {
        kill(child_proc, SIGKILL);
    }
}

const char* get_best_temp_dir() {
    char *env_tmp = getenv("TMPDIR");
    if (env_tmp && access(env_tmp, W_OK) == 0) {
        return env_tmp;
    }

    const char *termux_tmp = "/data/data/com.termux/files/usr/tmp";
    if (access(termux_tmp, W_OK) == 0) {
        return termux_tmp;
    }

    const char *android_tmp = "/data/local/tmp";
    if (access(android_tmp, W_OK) == 0) {
        return android_tmp;
    }

    return "/tmp";
}

static int safe_mkdir(const char *path, mode_t mode) {
    if (mkdir(path, mode) != 0 && errno != EEXIST) {
        fprintf(stderr, "[-] mkdir %s failed: %s\n", path, strerror(errno));
        return -1;
    }
    return 0;
}

void run_proot_encapsulated(int argc, char *argv[]) {
    char *proot_path = "./android/proot";

    if (access(proot_path, X_OK) != 0) {
        fprintf(stderr, "[-] PRoot binary not found or not executable at %s\n", proot_path);
        exit(EXIT_FAILURE);
    }

    if (access("android/libnetd_client.so", R_OK) != 0) {
        fprintf(stderr, "[-] Warning: Source file android/libnetd_client.so not found or not readable!\n");
    }

    // 创建 proot 挂载所需的目录
    if (safe_mkdir("rootfs", 0755) != 0) exit(EXIT_FAILURE);
    if (safe_mkdir("rootfs/dev", 0755) != 0) exit(EXIT_FAILURE);
    if (safe_mkdir("rootfs/proc", 0755) != 0) exit(EXIT_FAILURE);
    if (safe_mkdir("rootfs/sys", 0755) != 0) exit(EXIT_FAILURE);
    if (safe_mkdir("rootfs/system", 0755) != 0) exit(EXIT_FAILURE);
    if (safe_mkdir("rootfs/system/lib64", 0755) != 0) exit(EXIT_FAILURE);

    // 创建数据目录（与 wrapper.c 行为一致）
    if (safe_mkdir(args_info.base_dir_arg, 0777) != 0) exit(EXIT_FAILURE);

    char db_dir[1024];
    snprintf(db_dir, sizeof(db_dir), "%s/mpl_db", args_info.base_dir_arg);
    if (safe_mkdir(db_dir, 0777) != 0) exit(EXIT_FAILURE);

    // 动态构建 proot argv：固定选项 + 目标二进制 + 用户参数
    // 固定参数共 14 个 (proot, -r, rootfs/, -b, /dev:/dev, -b, /proc:/proc, -b, /sys:/sys,
    //                    -b, android/libnetd_client.so:/system/lib64/libnetd_client.so, -w, /, /system/bin/main)
    int fixed_args = 14;
    int total_args = fixed_args + (argc - 1) + 1;  // + 用户参数 + NULL
    char **proot_argv = malloc(total_args * sizeof(char *));
    if (proot_argv == NULL) {
        perror("malloc proot_argv");
        exit(EXIT_FAILURE);
    }

    int idx = 0;
    proot_argv[idx++] = "proot";
    proot_argv[idx++] = "-r";
    proot_argv[idx++] = "rootfs/";
    proot_argv[idx++] = "-b";
    proot_argv[idx++] = "/dev:/dev";
    proot_argv[idx++] = "-b";
    proot_argv[idx++] = "/proc:/proc";
    proot_argv[idx++] = "-b";
    proot_argv[idx++] = "/sys:/sys";
    proot_argv[idx++] = "-b";
    proot_argv[idx++] = "android/libnetd_client.so:/system/lib64/libnetd_client.so";
    proot_argv[idx++] = "-w";
    proot_argv[idx++] = "/";
    proot_argv[idx++] = "/system/bin/main";

    // 转发用户参数（跳过 argv[0]，即 wrapper-android 自身）
    for (int i = 1; i < argc; i++) {
        proot_argv[idx++] = argv[i];
    }
    proot_argv[idx] = NULL;

    const char *tmp_dir = get_best_temp_dir();
    printf("[*] Auto-setting PROOT_TMP_DIR=%s\n", tmp_dir);

    pid_t pid = fork();

    if (pid == 0) {
        // 子进程中设置环境变量，不影响父进程
        setenv("PROOT_TMP_DIR", tmp_dir, 1);
        if (execvp(proot_path, proot_argv) == -1) {
            perror("execvp proot failed");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {
        child_proc = pid;
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("[+] PRoot exited with status %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[-] PRoot killed by signal %d\n", WTERMSIG(status));
        }
    } else {
        perror("fork failed");
    }

    free(proot_argv);
}

int main(int argc, char *argv[]) {
    cmdline_parser(argc, argv, &args_info);

    if (signal(SIGINT, intHan) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    run_proot_encapsulated(argc, argv);
    return 0;
}
