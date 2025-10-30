#!/data/data/com.termux/files/usr/bin/bash
# Portable packer: builds runtime ZIP, writes *.sha256, updates manifests,
# and optionally publishes to a GitHub release when GH_PUBLISH=1.

set +e
umask 022

ROOT="${HOME}/android/RobotForest"
SCRIPTS="${ROOT}/scripts/runtime"
STAGING="${SCRIPTS}/runtime_staging"
OUTDIR="${ROOT}/out/runtime"
mkdir -p "$OUTDIR"

# Versioning
TS="$(date +%Y%m%d-%H%M%S)"
GIT_SHA="$(git -C "$ROOT" rev-parse --short=12 HEAD 2>/dev/null || echo nogit)"
BASENAME="runtime-${TS}-${GIT_SHA}"
ZIP="${OUTDIR}/${BASENAME}.zip"
SHAFILE="${OUTDIR}/${BASENAME}.zip.sha256"

# Defaults for URL+release
: "${GH_OWNER:=jasonsmr}"
: "${GH_REPO:=RobotForest}"
: "${GH_TAG:=auto}"
: "${RUNTIME_SUBDIR:=runtime}"

echo "[runtime] staging: $STAGING"
echo "[runtime] out zip: $ZIP"

# Sanity: we expect at least one file (box64) so app can decide validity
if [ ! -f "${STAGING}/bin/box64" ]; then
  echo "[runtime] ERROR: ${STAGING}/bin/box64 missing" >&2
  exit 1
fi

# Build ZIP
( cd "$STAGING" && zip -r -9 "$ZIP" . >/dev/null )
if [ $? -ne 0 ]; then
  echo "[runtime] ERROR: zip failed" >&2
  exit 1
fi

# Compute sha256 (sha256sum or openssl fallback)
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$ZIP" | awk '{print $1}' > "$SHAFILE"
else
  openssl dgst -sha256 "$ZIP" | awk '{print $2}' > "$SHAFILE"
fi
SHA="$(head -n1 "$SHAFILE")"
echo "[runtime] sha256: $SHA"

# The URL the app will fetch
PUBLISH_URL="https://github.com/${GH_OWNER}/${GH_REPO}/releases/download/${GH_TAG}/${BASENAME}.zip"

# Write manifests (with ACTUAL sha256)
write_manifest() {
  local path="$1"
  cat > "$path" <<JSON
{
  "url": "${PUBLISH_URL}",
  "sha256": "${SHA}",
  "subdir": "${RUNTIME_SUBDIR}"
}
JSON
}

write_manifest "${OUTDIR}/runtime-manifest.json"
write_manifest "${SCRIPTS}/runtime-manifest.json"

echo "[runtime] wrote:"
echo "  $ZIP"
echo "  $SHAFILE"
echo "  ${OUTDIR}/runtime-manifest.json"
echo "  ${SCRIPTS}/runtime-manifest.json"

# Optional: publish via GitHub CLI
if [ "${GH_PUBLISH:-0}" = "1" ]; then
  echo "[runtime] publishing to GitHub release: ${GH_TAG}"
  gh release view "${GH_TAG}" >/dev/null 2>&1 || \
    gh release create "${GH_TAG}" -t "${GH_TAG}" -n "Automated runtime ${TS}"

  gh release upload "${GH_TAG}" "${ZIP}" --clobber
  gh release upload "${GH_TAG}" "${SHAFILE}" --clobber

  # Commit updated manifest (contains exact URL + sha256)
  git -C "$ROOT" add "scripts/runtime/runtime-manifest.json"
  git -C "$ROOT" commit -m "runtime: publish ${BASENAME} and update manifest sha/url" >/dev/null 2>&1 || true
  git -C "$ROOT" push >/dev/null 2>&1 || true

  echo "[runtime] published: ${PUBLISH_URL}"
  echo "[runtime] published sha: ${PUBLISH_URL}.sha256"
else
  echo "[runtime] NOTE: GH_PUBLISH=1 to auto-upload & commit manifest URL/sha."
  echo "       Otherwise the manifest points at: ${PUBLISH_URL}"
fi

# Convenience copy for sideload/debug
DL="/sdcard/Download/${BASENAME}.zip"
cp -f "$ZIP" "$DL" 2>/dev/null && echo "[runtime] copied to ${DL}"

exit 0
