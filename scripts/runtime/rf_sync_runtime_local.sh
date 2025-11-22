#!/usr/bin/env bash
set -euo pipefail

# rf_sync_runtime_local.sh
# ------------------------
# Local helper (Termux/dev) to pull the latest rf-runtime-dev artifact
# from the robotforest-wow64-runtime repo and drop it into:
#   scripts/runtime/rf-runtime-dev.tar.zst
#
# Usage:
#   scripts/runtime/rf_sync_runtime_local.sh           # auto-detect latest
#   scripts/runtime/rf_sync_runtime_local.sh <RUN_ID>  # force specific run id
#
# Notes:
# - Requires: gh CLI, jq (for auto-discovery mode).
# - We currently target the ci/termux-safe-runtime branch in the runtime repo.

RUNTIME_REPO="jasonsmr/robotforest-wow64-runtime"
WORKFLOW_FILE="rf-release.yml"
BRANCH="ci/termux-safe-runtime"
ARTIFACT_NAME="rf-runtime-dev"

# Where to put the archive inside RobotForest repo
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
OUT_ARCHIVE="$SCRIPT_DIR/rf-runtime-dev.tar.zst"

# Temp dir for downloads
TMP_DIR="${TMPDIR:-/data/data/com.termux/files/home/tmp}/rf-runtime-sync-local"
rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

RUN_ID="${1:-}"

if [ -z "$RUN_ID" ]; then
  echo "[rf-sync] No RUN_ID provided, trying to auto-detect latest green RF Release..."
  if ! command -v jq >/dev/null 2>&1; then
    echo "[rf-sync] ERROR: jq is required for auto-discovery mode."
    echo "[rf-sync] Either install jq or call this script with an explicit RUN_ID."
    exit 1
  fi

  # Query latest successful runs for rf-release.yml on the target branch
  RUN_ID="$(
    gh run list \
      --repo "$RUNTIME_REPO" \
      --workflow "$WORKFLOW_FILE" \
      --branch "$BRANCH" \
      --json databaseId,conclusion,status \
      --limit 10 \
    | jq 'map(select(.status=="completed" and .conclusion=="success")) | .[0].databaseId' \
    | tr -d '\n'
  )"

  if [ -z "$RUN_ID" ] || [ "$RUN_ID" = "null" ]; then
    echo "[rf-sync] ERROR: Could not find a successful RF Release run on $BRANCH."
    exit 1
  fi

  echo "[rf-sync] Using latest green RF Release run id: $RUN_ID"
else
  echo "[rf-sync] Using explicit run id: $RUN_ID"
fi

echo "[rf-sync] Downloading artifact '$ARTIFACT_NAME' from $RUNTIME_REPO (run $RUN_ID)..."

gh run download "$RUN_ID" \
  --repo "$RUNTIME_REPO" \
  -n "$ARTIFACT_NAME" \
  -D "$TMP_DIR"

if [ ! -f "$TMP_DIR/$ARTIFACT_NAME.tar.zst" ]; then
  echo "[rf-sync] ERROR: Expected $TMP_DIR/$ARTIFACT_NAME.tar.zst not found."
  echo "[rf-sync] Contents of temp dir:"
  ls -R "$TMP_DIR" || true
  exit 1
fi

echo "[rf-sync] Copying $ARTIFACT_NAME.tar.zst into RobotForest scripts/runtime/..."
cp "$TMP_DIR/$ARTIFACT_NAME.tar.zst" "$OUT_ARCHIVE"

echo "[rf-sync] Done."
echo "[rf-sync] Local archive:"
ls -lh "$OUT_ARCHIVE"
