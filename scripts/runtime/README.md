# RobotForest Runtime

## TL;DR
- `rf-runtime-pack` → builds ZIP, writes `*.sha256`, updates manifest(s).
- `GH_PUBLISH=1 rf-runtime-pack` → also uploads to GitHub Release tag `auto` and commits manifest URL.

## Where the app looks
1. Remote: `scripts/runtime/runtime-manifest.json` via raw.githubusercontent.com
2. Fallback asset: `app/src/main/assets/runtime/manifest.json`

## Manifest shape
```json
{ "url": "<https asset>", "sha256": "auto", "subdir": "runtime" }
