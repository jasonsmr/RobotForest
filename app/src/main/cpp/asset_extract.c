#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define LOG_TAG "SkyrimLauncher"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static int mkdir_p(const char* path, mode_t mode) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(tmp, mode); *p = '/'; }
    }
    return mkdir(tmp, mode) == 0 || errno == EEXIST ? 0 : -1;
}

static int write_file(const char* dst, const void* data, size_t len, mode_t mode) {
    mkdir_p(dst, 0700);
    FILE* f = fopen(dst, "wb");
    if (!f) return -1;
    fwrite(data, 1, len, f);
    fclose(f);
    chmod(dst, mode);
    return 0;
}

static int is_exec_candidate(const char* rel) {
    // Mark known executables
    return strstr(rel, "runtime/box64/bin/box64") != NULL;
}

int extract_all_assets(AAssetManager* mgr, const char* filesDir) {
    // Read index
    AAsset* idx = AAssetManager_open(mgr, "assets_index.txt", AASSET_MODE_BUFFER);
    if (!idx) { LOGE("assets_index.txt missing"); return -1; }
    const char* buf = (const char*)AAsset_getBuffer(idx);
    size_t len = AAsset_getLength(idx);

    int extracted = 0;
    const char* line = buf;
    const char* end = buf + len;
    char rel[1024], out[2048];

    while (line < end) {
        const char* nl = memchr(line, '\n', (size_t)(end - line));
        size_t n = nl ? (size_t)(nl - line) : (size_t)(end - line);
        if (n > 0 && n < sizeof(rel)) {
            memcpy(rel, line, n); rel[n] = 0;

            AAsset* a = AAssetManager_open(mgr, rel, AASSET_MODE_BUFFER);
            if (!a) { LOGE("Asset open failed: %s", rel); goto next; }
            const void* data = AAsset_getBuffer(a);
            size_t alen = AAsset_getLength(a);

            snprintf(out, sizeof(out), "%s/%s", filesDir, rel);
            if (write_file(out, data, alen, is_exec_candidate(rel) ? 0700 : 0600) != 0) {
                LOGE("Write failed: %s (%s)", out, strerror(errno));
            } else {
                extracted++;
            }
            AAsset_close(a);
        }
    next:
        line = nl ? nl + 1 : end;
    }
    AAsset_close(idx);
    LOGI("Assets extracted: %d", extracted);
    return extracted;
}
