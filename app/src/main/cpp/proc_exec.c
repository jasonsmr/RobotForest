// Minimal exec helper for NativeActivity apps.
// Provides: make_executable(path), spawn_process(path, argv, envp)

#define _GNU_SOURCE
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "proc_exec"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

extern char **environ;

int make_executable(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        LOGE("stat failed for %s: %s", path, strerror(errno));
        return -errno;
    }
    mode_t mode = st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH;
    if (chmod(path, mode) != 0) {
        LOGE("chmod +x failed for %s: %s", path, strerror(errno));
        return -errno;
    }
    LOGI("chmod +x ok: %s", path);
    return 0;
}

// Returns child pid on success, <0 on error. Does not wait.
pid_t spawn_process(const char* path, char* const argv[], char* const envp[]) {
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    // Example: inherit stdio; customize here if you want pipes/log capture
    pid_t pid = 0;
    int rc = posix_spawn(&pid, path, &actions, /*attrp*/NULL,
                         (char* const*)argv,
                         envp ? (char* const*)envp : environ);
    posix_spawn_file_actions_destroy(&actions);
    if (rc != 0) {
        LOGE("posix_spawn failed for %s: %s", path, strerror(rc));
        return -rc;
    }
    LOGI("spawned pid=%d for %s", (int)pid, path);
    return pid;
}

// Convenience: spawn and wait; returns exit status or negative errno
int spawn_and_wait(const char* path, char* const argv[], char* const envp[]) {
    pid_t pid = spawn_process(path, argv, envp);
    if (pid < 0) return (int)pid;

    int status = 0;
    pid_t w = waitpid(pid, &status, 0);
    if (w < 0) {
        LOGE("waitpid failed: %s", strerror(errno));
        return -errno;
    }
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        LOGI("pid %d exited with %d", (int)pid, code);
        return code;
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        LOGE("pid %d killed by signal %d", (int)pid, sig);
        return -sig;
    }
    return -1;
}
