# RobotForest

Android launcher project built on-device in **Termux** (Z Fold 4), with NDK/Gradle.
This repo is configured for SSH-based Git, clean ignores, and CI builds.

## Build (on device)
```bash
cd ~/android/RobotForest
./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## CI
- GitHub Actions workflow builds Debug APK on push/PR.
- Artifacts are uploaded as workflow artifacts (not released automatically).

> Note: Signing configs remain local; CI only builds debug for safety.

## Status
[![runtime-verify](https://github.com/jasonsmr/RobotForest/actions/workflows/runtime-verify.yml/badge.svg)](https://github.com/jasonsmr/RobotForest/actions/workflows/runtime-verify.yml)

## Runtime integration

Durring on-device devalopment in Termux 
Sync rf-runtime-dev.tar.zst manually from the runtime repo:

cd ~/android/robotforest-wow64-runtime
RUN_ID=<RF_RELEASE_RUN_ID>

rm -rf "$TMP/rf-release-artifact-local"
mkdir -p "$TMP/rf-release-artifact-local"

gh run download "$RUN_ID" \
  -n rf-runtime-dev \
  -D "$TMP/rf-release-artifact-local"

cp "$TMP/rf-release-artifact-local/rf-runtime-dev.tar.zst" \
   ~/android/RobotForest/scripts/runtime/rf-runtime-dev.tar.zst


RobotForest consumes a prebuilt runtime bundle produced by the
`robotforest-wow64-runtime` repository.

For an overview of how the app and CI will consume the `rf-runtime-dev`
artifact, see:

- `docs/RF_RUNTIME_INTEGRATION.md`
