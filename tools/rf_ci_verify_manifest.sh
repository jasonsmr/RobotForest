#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-$PWD}"
MAN="$ROOT/scripts/runtime/runtime-manifest.json"
TMP="${TMP:-$HOME/tmp}"; mkdir -p "$TMP"

URL="$(jq -r .url "$MAN")"
SHA_EXPECT="$(jq -r .sha256 "$MAN")"

echo "[download] $URL"
curl -fL --retry 3 -o "$TMP/_rt_ci.zip" "$URL"

echo -n "sha256(zip): "
SHA_ACTUAL="$(sha256sum "$TMP/_rt_ci.zip" | awk '{print $1}')"
echo "$SHA_ACTUAL"

if [ "$SHA_EXPECT" != "$SHA_ACTUAL" ]; then
  echo "[ERROR] sha256 mismatch"
  echo "  expect: $SHA_EXPECT"
  echo "  actual: $SHA_ACTUAL"
  exit 1
fi

echo "[list]"
unzip -l "$TMP/_rt_ci.zip" | sed -n '1,20p'

echo -n "[ELF magic] "
unzip -p "$TMP/_rt_ci.zip" bin/box64 | head -c 4 | od -An -tx1 | tr -d ' \n'
echo
