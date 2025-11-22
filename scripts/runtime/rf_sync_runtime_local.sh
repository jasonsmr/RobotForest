#!/data/data/com.termux/files/usr/bin/env bash
set -euo pipefail

RUNTIME_REPO="jasonsmr/robotforest-wow64-runtime"
WORKFLOW_FILE="rf-release.yml"
ARTIFACT_NAME="rf-runtime-dev"

TMP_DIR="$TMP/rf_sync_local"
rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

# If the user exports RUNTIME_BRANCH, override workflow branch
BRANCH="${RUNTIME_BRANCH:-ci/termux-safe-runtime}"

echo "[rf-sync-local] Using branch: $BRANCH"

RUN_ID="$(
  gh run list \
    --repo "$RUNTIME_REPO" \
    --workflow "$WORKFLOW_FILE" \
    --branch "$BRANCH" \
    --json databaseId,conclusion,status \
  | jq 'map(select(.status=="completed" and .conclusion=="success")) | .[0].databaseId' \
  | tr -d '\n'
)"

if [ -z "$RUN_ID" ] || [ "$RUN_ID" = "null" ]; then
  echo "[rf-sync-local] ERROR: No successful run found for $BRANCH"
  exit 1
fi

echo "[rf-sync-local] Latest green run: $RUN_ID"

echo "[rf-sync-local] Downloading..."
gh run download "$RUN_ID" \
  --repo "$RUNTIME_REPO" \
  -n "$ARTIFACT_NAME" \
  -D "$TMP_DIR"

ARCHIVE="$TMP_DIR/${ARTIFACT_NAME}.tar.zst"

if [ ! -f "$ARCHIVE" ]; then
  echo "[rf-sync-local] ERROR: Missing archive: $ARCHIVE"
  exit 1
fi

echo "[rf-sync-local] Copy → scripts/runtime/"
cp "$ARCHIVE" "scripts/runtime/${ARTIFACT_NAME}.tar.zst"

echo "[rf-sync-local] Done:"
ls -lh "scripts/runtime/${ARTIFACT_NAME}.tar.zst"
