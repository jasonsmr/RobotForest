#pragma once
#include <android/asset_manager.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

// Create parent directories for a path (like `mkdir -p`).
int mkpath(const char* path, mode_t mode);

// Extract a single asset file from AAssetManager to dst_path with `mode`.
int extract_asset_file(AAssetManager* mgr, const char* asset_rel_path,
                       const char* dst_path, mode_t mode);

#ifdef __cplusplus
}
#endif
