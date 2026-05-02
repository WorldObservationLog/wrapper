#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/mount.h>

#include "cmdline.h"

pid_t child_proc = -1;
struct gengetopt_args_info args_info;
#define CAP_SYS_ADMIN_IDX 21
#define CAP_SYS_ADMIN_BIT (1ULL << CAP_SYS_ADMIN_IDX)

static void intHan(int signum) {
    if (child_proc != -1) {
        kill(child_proc, SIGKILL);
    }
}

int has_cap_sys_admin() {
    FILE *fp;
    char line[256];
    unsigned long long cap_eff = 0;
    int found_cap_eff = 0;

    fp = fopen("/proc/self/status", "r");
    if (fp == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "CapEff:", 7) == 0) {
            char *value_str = line + 7;
            while (*value_str == '\t' || *value_str == ' ') {
                value_str++;
            }
            cap_eff = strtoull(value_str, NULL, 16);
            found_cap_eff = 1;
            break;
        }
    }

    fclose(fp);

    if (!found_cap_eff) {
        return 0;
    }

    if (cap_eff & CAP_SYS_ADMIN_BIT) {
        return 1;
    } else {
        return 0;
    }
}

int main(int argc, char *argv[], char *envp[]) {
    cmdline_parser(argc, argv, &args_info);
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
    chmod("/system/bin/main", 0755);

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

    if (mkdir(args_info.base_dir_arg, 0777) != 0 && errno != EEXIST) {
        perror("mkdir base_dir_arg failed");
    }
    
    char db_dir[1024];
    snprintf(db_dir, sizeof(db_dir), "%s/mpl_db", args_info.base_dir_arg);
    if (mkdir(db_dir, 0777) != 0 && errno != EEXIST) {
        perror("mkdir mpl_db failed");
    }

    execve("/system/bin/main", argv, envp);
    
    perror("execve");
    return 1;
}