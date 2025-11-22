#!/usr/bin/env bash
set -euo pipefail

RUNTIME_REPO="jasonsmr/robotforest-wow64-runtime"
ARTIFACT_NAME="rf-runtime-dev"
WORKFLOW_FILE="rf-release.yml"
RUNTIME_BRANCH="${RUNTIME_BRANCH:-}"

echo "[rf-ci] Runtime repo: $RUNTIME_REPO"
echo "[rf-ci] Workflow: $WORKFLOW_FILE"
if [ -n "$RUNTIME_BRANCH" ]; then
  echo "[rf-ci] Filtering on branch: $RUNTIME_BRANCH"
fi

GH_ARGS=(--repo "$RUNTIME_REPO" --workflow "$WORKFLOW_FILE" --json databaseId,conclusion,status --limit 10)
if [ -n "$RUNTIME_BRANCH" ]; then
  GH_ARGS+=(--branch "$RUNTIME_BRANCH")
fi

echo "[rf-ci] Querying latest successful RF Release run..."
RUN_ID="$(
  gh run list "${GH_ARGS[@]}" \
    | jq 'map(select(.status=="completed" and .conclusion=="success")) | .[0].databaseId' \
    | tr -d '\n'
)"

if [ -z "$RUN_ID" ] || [ "$RUN_ID" = "null" ]; then
  echo "[rf-ci] ERROR: No successful RF Release run found for that workflow/branch."
  exit 1
fi

echo "[rf-ci] Latest RF Release run-id: $RUN_ID"

TMP_DIR="ci_runtime_tmp"
rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

echo "[rf-ci] Downloading artifact '$ARTIFACT_NAME' from run $RUN_ID..."
gh run download "$RUN_ID" \
  --repo "$RUNTIME_REPO" \
  -n "$ARTIFACT_NAME" \
  -D "$TMP_DIR"

ARCHIVE="$TMP_DIR/${ARTIFACT_NAME}.tar.zst"

if [ ! -f "$ARCHIVE" ]; then
  echo "[rf-ci] ERROR: Expected $ARCHIVE but it was not found."
  echo "[rf-ci] Contents of $TMP_DIR:"
  ls -R "$TMP_DIR" || true
  exit 1
fi

mkdir -p scripts/runtime

echo "[rf-ci] Moving archive into scripts/runtime/..."
mv "$ARCHIVE" "scripts/runtime/${ARTIFACT_NAME}.tar.zst"

echo "[rf-ci] Resulting file:"
ls -lh "scripts/runtime/${ARTIFACT_NAME}.tar.zst"
