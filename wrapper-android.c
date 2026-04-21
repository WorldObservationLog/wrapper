#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

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

void run_proot_encapsulated(char *target_binary, char **extra_args) {
    char *proot_path = "./android/proot";

    char proot_tmp_env[512];
    const char *tmp_dir = get_best_temp_dir();
    snprintf(proot_tmp_env, sizeof(proot_tmp_env), "PROOT_TMP_DIR=%s", tmp_dir);
    printf("[*] Auto-setting %s\n", proot_tmp_env);

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
        proot_tmp_env,
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