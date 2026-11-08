#!/bin/bash
# Build the iOS app, install it on a connected iPhone/iPad and copy the game data across.
#   GTA2_ROOT=<folder with data/> port/install-ios.sh [device-name-or-udid]
# Turkish: GTA2_TR_GXT=<e.gxt built by port/tools/build_tr.py> also installs it as e.gxt, with the
# style files' fonts extended by port/tools/sty_tr_font.py (SKIP_DATA=1 then copies only these).
# Other Apple developer accounts: PORT_TEAM_ID=<team> PORT_BUNDLE_ID=<unique id> port/install-ios.sh
# The device must be connected (USB or same Wi-Fi), unlocked, trusted and in Developer Mode.
set -e
cd "$(dirname "$0")/.."
: "${GTA2_ROOT:=$HOME/Code/gta2-ios/download/gta2_pc/App_Executables}"
DEVICE="${1:-$(xcrun devicectl list devices 2>/dev/null | grep physical | grep -v unavailable | grep -oE '[0-9A-F]{8}-[0-9A-F]{16}|[0-9A-F]{8}-([0-9A-F]{4}-){3}[0-9A-F]{12}' | head -1)}"
if [ -z "$DEVICE" ]; then
    echo "No available iOS device. Connect the iPhone, unlock it and enable Developer Mode."
    xcrun devicectl list devices
    exit 1
fi
echo "Device: $DEVICE"
# The intro movie is Bink; the port plays an H.264 copy of it (needs ffmpeg once)
BIK="$(find "$GTA2_ROOT/data" -maxdepth 2 -iname intro.bik | head -1)"
if [ -n "$BIK" ] && [ ! -f "${BIK%.*}.mp4" ] && command -v ffmpeg >/dev/null; then
    echo "Converting intro movie..."
    ffmpeg -hide_banner -loglevel error -i "$BIK" -c:v h264_videotoolbox -b:v 2500k -pix_fmt yuv420p \
        -c:a aac -b:a 160k -movflags +faststart "${BIK%.*}.mp4" || echo "(movie conversion failed, the intro will be skipped)"
fi

: "${PORT_BUNDLE_ID:=com.erkut.gta2port}"
CMAKE_ARGS=(-DPORT_BUNDLE_ID="$PORT_BUNDLE_ID")
[ -n "$PORT_TEAM_ID" ] && CMAKE_ARGS+=(-DPORT_TEAM_ID="$PORT_TEAM_ID")
cmake -S port -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 "${CMAKE_ARGS[@]}" >/dev/null
xcodebuild -project build-ios/gta2_native.xcodeproj -scheme gta2 -configuration Release \
    -sdk iphoneos -arch arm64 -derivedDataPath build-ios/dd -allowProvisioningUpdates build | tail -1

xcrun devicectl device install app --device "$DEVICE" build-ios/Release-iphoneos/GTA2.app

if [ "$SKIP_DATA" != "1" ]; then
    echo "Copying game data (~380 MB)..."
    xcrun devicectl device copy to --device "$DEVICE" \
        --domain-type appDataContainer --domain-identifier "$PORT_BUNDLE_ID" \
        --source "$GTA2_ROOT/data" --destination Documents/GTA2/data
fi

if [ -n "$GTA2_TR_GXT" ]; then
    echo "Installing the Turkish text and fonts..."
    TR_DIR=build-ios/tr-data
    mkdir -p "$TR_DIR"
    cp "$GTA2_TR_GXT" "$TR_DIR/e.gxt"
    for STY in fstyle wil bil ste; do
        python3 -I port/tools/sty_tr_font.py "$GTA2_ROOT/data/$STY.sty" "$TR_DIR/$STY.sty"
    done
    for F in "$TR_DIR"/*; do
        xcrun devicectl device copy to --device "$DEVICE" \
            --domain-type appDataContainer --domain-identifier "$PORT_BUNDLE_ID" \
            --source "$F" --destination "Documents/GTA2/data/$(basename "$F")" >/dev/null
    done
fi

xcrun devicectl device process launch --device "$DEVICE" "$PORT_BUNDLE_ID" || true
