#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "cmdline.h"

pid_t child_proc = -1;
struct gengetopt_args_info args_info;

static void intHan(int signum) {
    if (child_proc != -1) {
        kill(child_proc, SIGKILL);
    }
}

static int write_file(const char *path, const char *line) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t len = strlen(line);
    ssize_t ret = write(fd, line, len);
    close(fd);
    return (ret == len) ? 0 : -1;
}

static int setup_unprivileged_namespaces() {
    uid_t uid = getuid();
    gid_t gid = getgid();
    char buf[128];

    if (unshare(CLONE_NEWUSER | CLONE_NEWNS | CLONE_NEWPID) == -1) {
        perror("unshare");
        return -1;
    }

    snprintf(buf, sizeof(buf), "0 %u 1\n", uid);
    if (write_file("/proc/self/uid_map", buf) == -1) {
        perror("uid_map");
        return -1;
    }

    if (write_file("/proc/self/setgroups", "deny\n") == -1) {
        perror("setgroups");
        return -1;
    }

    snprintf(buf, sizeof(buf), "0 %u 1\n", gid);
    if (write_file("/proc/self/gid_map", buf) == -1) {
        perror("gid_map");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[], char *envp[]) {
    cmdline_parser(argc, argv, &args_info);
    if (signal(SIGINT, intHan) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    if (setup_unprivileged_namespaces() != 0) {
        return 1;
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

    if (mkdir("./rootfs/proc", 0755) != 0 && errno != EEXIST) {
        perror("mkdir ./rootfs/proc failed");
        return 1;
    }

    if (mount("proc", "./rootfs/proc", "proc", 0, NULL) != 0) {
        perror("mount proc failed");
        return 1;
    }

    // 5. 切换目录并 chroot
    if (chdir("./rootfs") != 0) {
        perror("chdir ./rootfs failed");
        return 1;
    }
    if (chroot(".") != 0) {
        perror("chroot . failed");
        return 1;
    }

    chmod("/system/bin/linker64", 0755);
    chmod("/system/bin/main", 0755);

    if (mkdir(args_info.base_dir_arg, 0777) != 0 && errno != EEXIST) {
        perror("mkdir base_dir_arg failed");
    } 
    
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/mpl_db", args_info.base_dir_arg);
    if (mkdir(db_path, 0777) != 0 && errno != EEXIST) {
        perror("mkdir mpl_db failed");
    }

    execve("/system/bin/main", argv, envp);
    
    perror("execve");
    return 1;
}