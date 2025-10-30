#!/usr/bin/env bash
set -euo nounset
ROOT="${ROOT:-$PWD}"
APK="$ROOT/app/build/outputs/apk/debug/app-debug.apk"
DISK="$ROOT/scripts/runtime/runtime-manifest.json"
TMP="${TMP:-$HOME/tmp}"
mkdir -p "$TMP"
jq -cS . "$DISK" > "$TMP/_manifest_disk.json"
unzip -p "$APK" assets/runtime/manifest.json | jq -cS . > "$TMP/_manifest_apk.json"
echo "DISK: $(cat "$TMP/_manifest_disk.json")"
echo " APK: $(cat "$TMP/_manifest_apk.json")"
if cmp -s "$TMP/_manifest_disk.json" "$TMP/_manifest_apk.json"; then
  echo "[OK] Embedded manifest matches on-disk manifest"
else
  echo "[DIFF] Manifests differ — unified diff below:"
  diff -u "$TMP/_manifest_disk.json" "$TMP/_manifest_apk.json" || true
  exit 1
fi
