# RobotForest Runtime Integration

This document explains how the RobotForest Android app consumes the
`rf-runtime-dev` artifact built by the external repository:

- `jasonsmr/robotforest-wow64-runtime`

The goals:

1. Keep the runtime build system **decoupled** from the app.
2. Allow the app to either:
   - Embed a known-good runtime at build time, or
   - Download and install the runtime on first launch.

---

## 1. Upstream runtime artifact

Source repository:

- `https://github.com/jasonsmr/robotforest-wow64-runtime`

Canonical artifact:

- Name: `rf-runtime-dev`
- Formats:
  - `rf-runtime-dev.tar.zst`
  - `rf-runtime-dev.zip`

The artifact contains a top-level layout like:

- `bin/`
- `dxvk/`
- `vkd3d/`
- `prefix/`
- `x86_64-linux/`
- `i386-linux/`
- `rf_env.sh`
- `rf_install_runtime.sh`
- `rf_runtime_layout_check.sh`
- `runtime.version`
- `proton/` (optional)

For full details, see that repo’s `docs/RUNTIME_ARTIFACT.md`.

---

## 2. Where the app expects the runtime

The app will treat the runtime as an opaque bundle. At build time or at
install time, we want:

- A single archive file, e.g.:

  - `rf-runtime-dev.tar.zst`

- Installed into a path like:

  - **App-private storage** on device (preferred), via `rf_install_runtime.sh`.
  - Or a **local cache directory** under Termux during development.

The exact on-device target path will be configured in app code, but the
contract is:

- The installer runs:

  ```sh
  rf_install_runtime.sh /path/to/rf-runtime-dev.tar.zst /path/to/runtime_root


