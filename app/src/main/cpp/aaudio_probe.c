#include <aaudio/AAudio.h>
#include <android/log.h>

#define TAG "SkyrimLauncher/AAudio"

int aaudio_probe_run(void) {
    AAudioStreamBuilder* bld = NULL;
    aaudio_result_t r = AAudio_createStreamBuilder(&bld);
    
    if (r != AAUDIO_OK || !bld) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "createStreamBuilder failed: %d", r);
        return -1;
    }

    // Basic output stream: 48kHz stereo, low latency if possible
    AAudioStreamBuilder_setDirection(bld, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setPerformanceMode(bld, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(bld, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setSampleRate(bld, 48000);
    AAudioStreamBuilder_setChannelCount(bld, 2);
    AAudioStreamBuilder_setFormat(bld, AAUDIO_FORMAT_PCM_FLOAT);

    AAudioStream* stream = NULL;
    r = AAudioStreamBuilder_openStream(bld, &stream);
    
    if (r != AAUDIO_OK || !stream) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "openStream failed: %d", r);
        AAudioStreamBuilder_delete(bld);
        return -2;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG, "AAudio open OK: SR=%d, ch=%d, fmt=%d, buf=%d frames",
                        AAudioStream_getSampleRate(stream),
                        AAudioStream_getChannelCount(stream),
                        AAudioStream_getFormat(stream),
                        AAudioStream_getBufferSizeInFrames(stream));

    // We’re not starting it yet; just proving it opens correctly.
    AAudioStream_close(stream);
    AAudioStreamBuilder_delete(bld);
    return 0;
}

