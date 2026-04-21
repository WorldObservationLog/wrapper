#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

void run_proot_encapsulated(char *target_binary, char **extra_args) {
    char *proot_path = "./android/proot";

    char *argv[] = {
        "proot",
        "-r", "rootfs/",
        "-b", "/dev:/dev",
        "-b", "/proc:/proc",
        "-b", "/sys:/sys",
        "-b", "android/libnetd_client.so:/system/lib64/libnetd_client.so", 
        "-w", "/",
        target_binary,
        NULL
    };

    char *envp[] = {
        NULL
    };

    pid_t pid = fork();

    if (pid == 0) {
        if (execve(proot_path, argv, envp) == -1) {
            perror("execve failed");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("[+] PRoot exited with status %d\n", WEXITSTATUS(status));
        }
    } else {
        perror("fork failed");
    }
}

int main() {
    run_proot_encapsulated("/system/bin/main", NULL);
    return 0;
}