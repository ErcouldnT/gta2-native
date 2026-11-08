#!/bin/bash
# Build and run the native macOS port.
#   GTA2_ROOT=<folder with data/> port/run-mac.sh
set -e
cd "$(dirname "$0")/.."
: "${GTA2_ROOT:=$HOME/Code/gta2-ios/download/gta2_pc/App_Executables}"
export GTA2_ROOT
# The intro movie is Bink; the port plays an H.264 copy of it (needs ffmpeg once)
BIK="$(find "$GTA2_ROOT/data" -maxdepth 2 -iname intro.bik | head -1)"
if [ -n "$BIK" ] && [ ! -f "${BIK%.*}.mp4" ] && command -v ffmpeg >/dev/null; then
    echo "Converting intro movie..."
    ffmpeg -hide_banner -loglevel error -i "$BIK" -c:v h264_videotoolbox -b:v 2500k -pix_fmt yuv420p \
        -c:a aac -b:a 160k -movflags +faststart "${BIK%.*}.mp4" || echo "(movie conversion failed, the intro will be skipped)"
fi
cmake -S port -B build-mac -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
cmake --build build-mac -j"$(sysctl -n hw.ncpu)"
exec build-mac/gta2 "$@"
