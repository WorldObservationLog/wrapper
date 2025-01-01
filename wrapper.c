#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/prctl.h>

pid_t child_proc = -1;

static void intHan(int signum) {
    if (child_proc != -1) {
        kill(child_proc, SIGKILL);
        waitpid(child_proc, NULL, 0); // Clean up zombie process
    }
    exit(130); // Standard exit code for SIGINT
}

static int setup_chroot(const char *rootfs_path) {
    if (chdir(rootfs_path) != 0) {
        perror("chdir");
        return -1;
    }
    if (chroot("./") != 0) {
        perror("chroot");
        return -1;
    }
    return 0;
}

static int setup_devices() {
    // Create /dev if it doesn't exist
    mkdir("/dev", 0755);
    
    if (mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9)) != 0) {
        if (errno != EEXIST) {
            perror("mknod urandom");
            return -1;
        }
    }
    return 0;
}

static int setup_permissions() {
    const char *files[] = {
        "/system/bin/linker64",
        "/system/bin/main"
    };
    
    for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); i++) {
        if (chmod(files[i], 0755) != 0) {
            fprintf(stderr, "chmod failed for %s: %s\n", files[i], strerror(errno));
            return -1;
        }
    }
    return 0;
}

static int create_directories() {
    const char *dirs[] = {
        "/data/data/com.apple.android.music/files",
        "/data/data/com.apple.android.music/files/mpl_db"
    };
    
    for (size_t i = 0; i < sizeof(dirs)/sizeof(dirs[0]); i++) {
        if (mkdir(dirs[i], 0777) != 0 && errno != EEXIST) {
            fprintf(stderr, "mkdir failed for %s: %s\n", dirs[i], strerror(errno));
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[], char *envp[]) {
    // Set up signal handler
    struct sigaction sa = {
        .sa_handler = intHan,
        .sa_flags = SA_RESETHAND,
    };
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    // Setup chroot environment
    if (setup_chroot("./rootfs") != 0) {
        return 1;
    }

    // Setup devices and permissions
    if (setup_devices() != 0 || setup_permissions() != 0) {
        return 1;
    }

    child_proc = fork();
    if (child_proc == -1) {
        perror("fork");
        return 1;
    }

    if (child_proc > 0) {
        // Parent process
        int status;
        waitpid(child_proc, &status, 0);
        
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return 1;
    }

    // Child process
    // Set process name for easier debugging
    prctl(PR_SET_NAME, "android-main", 0, 0, 0);
    
    // Create required directories
    if (create_directories() != 0) {
        exit(1);
    }

    // Execute the main binary
    execve("/system/bin/main", argv, envp);
    perror("execve");
    return 1;
}