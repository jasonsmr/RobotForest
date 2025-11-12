#!/usr/bin/env bash
set -euo pipefail

OWNER="jasonsmr"
REPO="robotforest-wow64-runtime"
TAG="${1:-latest}"   # use 'latest' or an explicit tag like v0.1.1

OUT="$HOME/android/RobotForest/app/src/main/assets/runtime/rf-runtime.zip"
mkdir -p "$(dirname "$OUT")"

have_cmd() { command -v "$1" >/dev/null 2>&1; }

# 1) Try the deterministic direct download URL if TAG != latest
if [[ "$TAG" != "latest" ]]; then
  DIRECT_URL="https://github.com/${OWNER}/${REPO}/releases/download/${TAG}/robotforest-wow64-runtime-${TAG}.zip"
  echo "[fetch] probing direct: $DIRECT_URL"
  if curl -fsI "$DIRECT_URL" >/dev/null 2>&1; then
    echo "[fetch] downloading direct asset -> $OUT"
    curl -fL "$DIRECT_URL" -o "$OUT"
    touch "$HOME/android/RobotForest/app/src/main/assets/runtime/manifest.json" || true
    echo "[fetch] Done."
    exit 0
  else
    echo "[fetch] direct URL not ready yet (404 or similar). Will query the API…"
  fi
fi

# 2) Query Releases API (requires jq; if not present we fall back to first .zip found via grep)
API_BASE="https://api.github.com/repos/$OWNER/$REPO/releases"
if [[ "$TAG" == "latest" ]]; then
  API_URL="$API_BASE/latest"
else
  API_URL="$API_BASE/tags/$TAG"
fi

echo "[fetch] Querying API: $API_URL"
set +e
json=$(curl -fsSL -H 'Accept: application/vnd.github+json' "$API_URL")
st=$?
set -e

if (( st != 0 )); then
  echo "[fetch] API returned $st (likely the release object doesn't exist yet)."
  if [[ "$TAG" != "latest" ]]; then
    echo "        The tag may be pushed, but the release workflow hasn't created the Release yet."
  fi
  exit 22
fi

# Extract assets list
if have_cmd jq; then
  assets=$(printf '%s' "$json" | jq -r '.assets[]?.browser_download_url' || true)
else
  # crude fallback: extract URLs ending with .zip
  assets=$(printf '%s\n' "$json" | grep -oE 'https://[^"]+\.zip' | tr -d '\r' | sort -u)
fi

if [[ -z "${assets// }" ]]; then
  echo "[fetch] No zip assets found on release '$TAG'."
  echo "        If you just pushed the tag, wait for the GitHub Actions release job to finish."
  exit 22
fi

# Prefer the deterministic naming from your workflow
url=$(printf '%s\n' "$assets" | grep -E "robotforest-wow64-runtime-.*\.zip$" | head -n1 || true)
if [[ -z "$url" ]]; then
  url=$(printf '%s\n' "$assets" | head -n1 || true)
fi
[[ -n "$url" ]] || { echo "[fetch] No .zip asset URL found."; exit 22; }

echo "[fetch] $url -> $OUT"
curl -fL "$url" -o "$OUT"
touch "$HOME/android/RobotForest/app/src/main/assets/runtime/manifest.json" || true
echo "[fetch] Done."
