#include <android_native_app_glue.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "RF", __VA_ARGS__)

static void handle_cmd(struct android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("NativeActivity window init (black screen as stub).");
            break;
        default:
            break;
    }
}

void android_main(struct android_app* app) {
    app->onAppCmd = handle_cmd;
    int events;
    struct android_poll_source* source;

    while (1) {
        while (ALooper_pollOnce(0, NULL, &events, (void**)&source) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) return;
        }
    }
}
