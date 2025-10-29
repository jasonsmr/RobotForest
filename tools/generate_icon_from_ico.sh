#!/system/bin/sh
set -eu

# --- Args --------------------------------------------------------------
ICO="${1:-fizban.ico}"
shift 1 || true

SCALE=90     # % of canvas side the icon should occupy (centered)
FRAME=""     # empty = auto-pick largest; or pass number
BG="#202124" # background color for adaptive icon (dark gray)

while [ $# -gt 0 ]; do
  case "$1" in
    --scale)
      SCALE="$2"; shift 2;;
    --frame)
      FRAME="$2"; shift 2;;
    --bg)
      BG="$2"; shift 2;;
    *)
      echo "Unknown arg: $1" >&2; exit 2;;
  esac
done

# clamp SCALE to sane range
case "$SCALE" in
  ''|*[!0-9]*) echo "Bad --scale '$SCALE' (expect integer 10..100)"; exit 2;;
esac
[ "$SCALE" -lt 10 ] && SCALE=10
[ "$SCALE" -gt 100 ] && SCALE=100

RES="app/src/main/res"

# --- Tooling -----------------------------------------------------------
if command -v magick >/dev/null 2>&1; then
  IM="magick"
elif command -v convert >/dev/null 2>&1; then
  IM="convert"
else
  echo "Error: ImageMagick (magick/convert) not found" >&2
  exit 1
fi

if ! command -v identify >/dev/null 2>&1; then
  echo "Warning: 'identify' not found; assuming ICO frame 0"
  IDX=0
else
  if [ -n "$FRAME" ]; then
    IDX="$FRAME"
  else
    # pick largest frame from ICO (IM6/IM7 safe)
    IDX="$(identify -format "%[fx:w*h] %[scene]\n" "$ICO" \
          | awk 'BEGIN{max=-1; idx=0} {if($1>max){max=$1; idx=$2}} END{print idx}')"
  fi
fi

[ -f "$ICO" ] || { echo "Error: $ICO not found" >&2; exit 1; }

# --- Folders -----------------------------------------------------------
mkdir -p \
  "$RES/mipmap-mdpi" "$RES/mipmap-hdpi" "$RES/mipmap-xhdpi" \
  "$RES/mipmap-xxhdpi" "$RES/mipmap-xxxhdpi" \
  "$RES/mipmap-anydpi-v26" "$RES/values"

# --- Core: composite icon centered on a fixed-size transparent canvas ---
# size param like "108x108"; out is file path; SCALE is 10..100
make_one () {
  size="$1"; out="$2"; scale_pct="$3"

  # Extract numeric base side (square densities)
  base="${size%x*}"    # "108x108" -> "108"

  # Compute content side in px (rounded)
  content_px=$(( base * scale_pct / 100 ))

  # 1) Prepare a transparent canvas of the final size
  # 2) Prepare the source layer:
  #    - take chosen ICO frame
  #    - pad to square with transparent bg (keeps aspect)
  #    - resize the icon CONTENT to $content_px
  # 3) Composite centered onto the canvas
  $IM -size "$size" canvas:none miff:- \
    | $IM - \
      \( "$ICO[$IDX]" -alpha on -gravity center -background none \
         -extent "%[fx:max(w,h)]x%[fx:max(w,h)]" -filter Lanczos \
         -resize "${content_px}x${content_px}" \) \
      -gravity center -compose over -composite "$out"
}

echo "Using ICO frame index: $IDX"
echo "Foreground scale: ${SCALE}%"

# Foregrounds for adaptive icon
make_one 108x108 "$RES/mipmap-mdpi/ic_launcher_foreground.png"   "$SCALE"
make_one 162x162 "$RES/mipmap-hdpi/ic_launcher_foreground.png"   "$SCALE"
make_one 216x216 "$RES/mipmap-xhdpi/ic_launcher_foreground.png"  "$SCALE"
make_one 324x324 "$RES/mipmap-xxhdpi/ic_launcher_foreground.png" "$SCALE"
make_one 432x432 "$RES/mipmap-xxxhdpi/ic_launcher_foreground.png" "$SCALE"

# Background color (tweak with --bg)
cat > "$RES/values/ic_launcher_background.xml" <<EOF
<resources>
    <color name="ic_launcher_background">$BG</color>
</resources>
EOF

# Adaptive icon wrappers (API 26+)
cat > "$RES/mipmap-anydpi-v26/ic_launcher.xml" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/ic_launcher_background"/>
    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>
</adaptive-icon>
EOF

cat > "$RES/mipmap-anydpi-v26/ic_launcher_round.xml" <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/ic_launcher_background"/>
    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>
</adaptive-icon>
EOF

echo "Done. Rebuild the APK to see the new icon."
