#!/usr/bin/env bash
set -euo pipefail
# Requires: curl, jq (or pass TAG explicitly)
OWNER="jasonsmr"
REPO="robotforest-wow64-runtime"
TAG="${1:-latest}"  # or pass v0.1.0

if [[ "$TAG" == "latest" ]]; then
  api="https://api.github.com/repos/$OWNER/$REPO/releases/latest"
else
  api="https://api.github.com/repos/$OWNER/$REPO/releases/tags/$TAG"
fi

# Get the first .zip asset URL
url=$(curl -fsSL "$api" | jq -r '.assets[] | select(.name|endswith(".zip")) | .browser_download_url' | head -n1)
[[ -n "$url" ]] || { echo "No zip asset found."; exit 1; }

OUT="$HOME/android/RobotForest/app/src/main/assets/runtime/rf-runtime.zip"
mkdir -p "$(dirname "$OUT")"
echo "[fetch] $url -> $OUT"
curl -fL "$url" -o "$OUT"
touch "$HOME/android/RobotForest/app/src/main/assets/runtime/manifest.json" || true
