#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t child_proc = -1;

static void intHan(int signum) {
    if (child_proc != -1) {
        kill(child_proc, SIGKILL);
    }
}

int main(int argc, char *argv[], char *envp[]) {
    if (signal(SIGINT, intHan) == SIG_ERR) {
        perror("signal");
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
    mknod("/dev/urandom", S_IFCHR | 0666, makedev(0x1, 0x9));
    chmod("/system/bin/linker64", 0755);
    chmod("/system/bin/main", 0755);

    child_proc = fork();
    if (child_proc == -1) {
        perror("fork");
        return 1;
    }

    if (child_proc > 0) {
        close(STDOUT_FILENO);
        wait(NULL);  // Parent waits for the child process to terminate
        return 0;
    }

    // Child process logic
    mkdir("/data/data/com.apple.android.music/files", 0777);
    mkdir("/data/data/com.apple.android.music/files/mpl_db", 0777);
    execve("/system/bin/main", argv, envp);
    perror("execve");
    return 1;
}