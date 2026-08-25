/*
 * wrapper-lite.c — Apple Music 解密 wrapper-lite 的宿主层 (root 特权版)
 * 流程: 解析 --base-dir → bind-mount rootfs/dev/urandom → chdir+chroot ./rootfs
 *   → 若有 CAP_SYS_ADMIN 则 unshare(CLONE_NEWPID) → fork 子进程
 *   → mount proc, 建 base_dir/mpl_db → execve("/system/bin/lite")。
 * 参数 (--host/--port/--login/--debug/...) 原样传给 lite。
 */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t child_proc = -1;
#define CAP_SYS_ADMIN_IDX 21
#define CAP_SYS_ADMIN_BIT (1ULL << CAP_SYS_ADMIN_IDX)

static void intHan(int signum) {
    if (child_proc != -1) {
        kill(child_proc, SIGKILL);
    }
}

static int has_cap_sys_admin(void) {
    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) return 0;
    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "CapEff:", 7) == 0) {
            char *v = line + 7;
            while (*v == ' ' || *v == '\t') v++;
            if (strtoull(v, NULL, 16) & CAP_SYS_ADMIN_BIT) found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}

int main(int argc, char *argv[], char *envp[]) {
    const char *exec_path = "/system/bin/lite";
    const char *base_dir_for_mkdir = "data";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--base-dir") == 0 && i + 1 < argc) {
            base_dir_for_mkdir = argv[++i];
        }
    }

    if (signal(SIGINT, intHan) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    if (mkdir("./rootfs/dev", 0755) != 0 && errno != EEXIST) {
        perror("mkdir ./rootfs/dev failed");
        return 1;
    }

    int fd = open("./rootfs/dev/urandom", O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("open ./rootfs/dev/urandom failed");
        return 1;
    }
    close(fd);

    if (mount("/dev/urandom", "./rootfs/dev/urandom", NULL, MS_BIND, NULL) != 0) {
        perror("mount /dev/urandom failed");
        return 1;
    }

    if (chdir("./rootfs") != 0) {
        perror("chdir");
        return 1;
    }
    if (chroot("./") != 0) {
        perror("chroot");
        return 1;
    }

    if (mkdir("/proc", 0755) != 0 && errno != EEXIST) {
        perror("mkdir /proc failed");
        return 1;
    }

    chmod("/system/bin/linker64", 0755);
    chmod(exec_path, 0755);

    if (has_cap_sys_admin()) {
        if (unshare(CLONE_NEWPID)) {
            perror("unshare");
            return 1;
        }
    }

    child_proc = fork();
    if (child_proc == -1) {
        perror("fork");
        return 1;
    }

    if (child_proc > 0) {
        wait(NULL);
        return 0;
    }

    if (mount("proc", "/proc", "proc", 0, NULL) != 0) {
        perror("mount proc failed");
        return 1;
    }

    if (mkdir(base_dir_for_mkdir, 0777) != 0 && errno != EEXIST) {
        perror("mkdir base_dir failed");
    }

    char db_path[1024];
    snprintf(db_path, sizeof(db_path), "%s/mpl_db", base_dir_for_mkdir);
    if (mkdir(db_path, 0777) != 0 && errno != EEXIST) {
        perror("mkdir mpl_db failed");
    }

    execve(exec_path, argv, envp);
    perror("execve");
    return 1;
}
