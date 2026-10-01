/* canonical implementation: wrapper.c — this file may only differ where rootless operation requires it */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
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

static void intHan(int signum) {
    if (child_proc != -1) {
        kill(child_proc, SIGKILL);
    }
}

/* rootless-only: write a single line to a /proc file */
static int write_file(const char *path, const char *line) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t len = strlen(line);
    ssize_t ret = write(fd, line, len);
    close(fd);
    return (ret == len) ? 0 : -1;
}

/* rootless-only: create user+mount namespace and map current uid/gid to root */
static int setup_user_namespace() {
    uid_t uid = getuid();
    gid_t gid = getgid();
    char buf[128];

    if (unshare(CLONE_NEWUSER | CLONE_NEWNS) == -1) {
        if (errno == EPERM) {
            fprintf(stderr,
                "error: unprivileged user namespaces are not permitted on this system.\n"
                "  options:\n"
                "    1. enable:  sudo sysctl -w kernel.unprivileged_userns_clone=1\n"
                "    2. use the privileged wrapper instead\n"
                "    3. grant capabilities: sudo setcap cap_sys_admin+ep ./wrapper-rootless\n");
        } else {
            perror("unshare");
        }
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

    /* rootless-only: establish user namespace before any privileged operations */
    if (setup_user_namespace() != 0) {
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

    /* rootless-only: user namespace guarantees CLONE_NEWPID is always available */
    if (unshare(CLONE_NEWPID)) {
        perror("unshare");
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
