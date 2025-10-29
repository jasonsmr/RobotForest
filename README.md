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
