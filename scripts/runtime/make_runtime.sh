#!/usr/bin/env bash
# Simple & tolerant (no -euo/pipefail): we want a best-effort packer

# ---- Config
set +e
ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="$ROOT_DIR/out/runtime"
STAGING_A="$ROOT_DIR/scripts/runtime/runtime_staging"
STAGING_B="$ROOT_DIR/app/src/main/assets/runtime"
TS="$(date -u +%Y%m%d-%H%M%S)"
COMMIT_SHA="$(git -C "$ROOT_DIR" rev-parse --short=12 HEAD 2>/dev/null)"
RUNTIME_NAME="runtime-${TS}-${COMMIT_SHA}.zip"
MANIFEST_JSON="$OUT_DIR/runtime-manifest.json"

mkdir -p "$OUT_DIR"

# ---- Pick staging source
SRC=""
if [ -d "$STAGING_A" ] && [ -n "$(ls -A "$STAGING_A" 2>/dev/null)" ]; then
  SRC="$STAGING_A"
elif [ -d "$STAGING_B" ] && [ -n "$(ls -A "$STAGING_B" 2>/dev/null)" ]; then
  SRC="$STAGING_B"
fi

if [ -z "$SRC" ]; then
  echo "[runtime] No staging content found."
  echo "Create at least one:"
  echo "  - $STAGING_A"
  echo "  - $STAGING_B"
  exit 0
fi

echo "[runtime] Using staging: $SRC"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

# Copy into temp to normalize perms without touching source
cp -a "$SRC" "$TMPDIR/runtime" 2>/dev/null

# Normalize executable bits for common bins (best effort)
find "$TMPDIR/runtime" -type f -name 'box64' -o -name 'wine*' 2>/dev/null | while read -r f; do
  chmod 0755 "$f" 2>/dev/null
done

# Strip extended attrs if any (best effort)
( cd "$TMPDIR" && zip -rq9 "$OUT_DIR/$RUNTIME_NAME" runtime ) || {
  echo "[runtime] zip failed"
  exit 1
}

# Checksums
( cd "$OUT_DIR" && sha256sum "$RUNTIME_NAME" > "$RUNTIME_NAME.sha256" ) 2>/dev/null || \
( cd "$OUT_DIR" && shasum -a 256 "$RUNTIME_NAME" > "$RUNTIME_NAME.sha256" )

# Manifest
cat > "$MANIFEST_JSON" <<JSON
{
  "name": "$RUNTIME_NAME",
  "commit": "${COMMIT_SHA:-unknown}",
  "timestamp_utc": "$TS",
  "sha256_file": "$RUNTIME_NAME.sha256",
  "notes": "Zipped from ${SRC#"$ROOT_DIR"/}"
}
JSON

# Copy to Downloads (local convenience)
DL="/sdcard/Download"
if [ -d "$DL" ]; then
  cp -f "$OUT_DIR/$RUNTIME_NAME" "$DL/" 2>/dev/null
  cp -f "$OUT_DIR/$RUNTIME_NAME.sha256" "$DL/" 2>/dev/null
  echo "[runtime] Copied to $DL/$RUNTIME_NAME"
fi

echo "[runtime] Done:"
echo "  $OUT_DIR/$RUNTIME_NAME"
echo "  $OUT_DIR/$RUNTIME_NAME.sha256"
echo "  $MANIFEST_JSON"
