cmake_minimum_required(VERSION 3.22)
project(robotforest_native C)

add_library(launcher SHARED main_android.c)

# Link native_app_glue + log + android
set(ANDROID_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
add_library(native_app_glue STATIC
    ${ANDROID_GLUE_DIR}/android_native_app_glue.c)

target_include_directories(native_app_glue PUBLIC
    ${ANDROID_GLUE_DIR})

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(launcher
    native_app_glue
    ${android-lib}
    ${log-lib})
