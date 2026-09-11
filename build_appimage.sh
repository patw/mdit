#!/bin/bash
# Build mdit's AppImage.
#
# Adapted from PengyCPP's appimage/build.sh (linuxdeploy + the Qt plugin), with
# the hard-won parts kept:
#   * the icon handed to linuxdeploy is exactly 256x256 (it validates that the
#     file name matches the dimensions);
#   * the **Qt Wayland platform plugin is bundled and then VERIFIED** — a
#     linuxdeploy-plugin-qt bundle only ships xcb by default, which makes the
#     AppImage fall back to XWayland on a Wayland desktop (blurry on HiDPI) or
#     fail to start at all on a Wayland-only compositor.
#
# Prerequisites:
#   sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-base-dev-tools \
#                    qt6-wayland libgl-dev wget file imagemagick
#   mkdir -p appimage/tools && wget -P appimage/tools \
#     https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
#     https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
#   chmod +x appimage/tools/*.AppImage
#
# Usage:  ./build_appimage.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

BUILD_JOBS="$(command -v nproc >/dev/null 2>&1 && nproc || echo 4)"
TOOLS="$ROOT/appimage/tools"
APPDIR="$ROOT/appimage/mdit.AppDir"
ICON="$ROOT/assets/icons/mdit-256.png"
DESKTOP="$ROOT/packaging/mdit.desktop"

command -v convert >/dev/null 2>&1 || { echo "ERROR: imagemagick (convert) is required" >&2; exit 1; }
[ -x "$TOOLS/linuxdeploy-x86_64.AppImage" ] || { echo "ERROR: linuxdeploy not in $TOOLS" >&2; exit 1; }
[ -x "$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage" ] || { echo "ERROR: linuxdeploy-plugin-qt not in $TOOLS" >&2; exit 1; }

# ── 1. Build ────────────────────────────────────────────────────────────────
echo "==> Building mdit..."
BUILD_DIR="$ROOT/build_package"   # shared with build_deb.sh (never ./build)
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --parallel "$BUILD_JOBS"
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mdit" --version

# ── 2. Assemble the AppDir from `cmake --install` (same payload as the .deb) ─
echo "==> Assembling the AppDir..."
rm -rf "$APPDIR"
mkdir -p "$APPDIR"
cmake --install "$BUILD_DIR" --prefix "$APPDIR/usr" --strip >/dev/null
# linuxdeploy wants the icon at the AppDir root too, named after the app.
cp "$ICON" "$APPDIR/mdit.png"
QT_QPA_PLATFORM=offscreen "$APPDIR/usr/bin/mdit" --version >/dev/null

# ── 3. Bundle the Wayland platform plugin (+ its runtime libs) ──────────────
echo "==> Bundling the Wayland platform plugin..."
QT6_PLUGINS="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || echo /usr/lib/x86_64-linux-gnu/qt6/plugins)"
mapfile -t WAYLAND_PLUGINS < <(find "$QT6_PLUGINS/platforms" -maxdepth 1 -name 'libqwayland*.so' 2>/dev/null)
if [ "${#WAYLAND_PLUGINS[@]}" -eq 0 ]; then
    echo "ERROR: no Qt6 Wayland platform plugin (libqwayland*.so) under $QT6_PLUGINS/platforms." >&2
    echo "       Install qt6-wayland + libqt6waylandclient6, then rebuild." >&2
    exit 1
fi
mkdir -p "$APPDIR/usr/plugins/platforms" "$APPDIR/usr/lib"
cp "${WAYLAND_PLUGINS[@]}" "$APPDIR/usr/plugins/platforms/"
for dir in wayland-shell-integration wayland-graphics-integration-client wayland-decoration-client; do
    [ -d "$QT6_PLUGINS/$dir" ] || continue
    mkdir -p "$APPDIR/usr/plugins/$dir"
    cp -a "$QT6_PLUGINS/$dir/." "$APPDIR/usr/plugins/$dir/"
done
for pattern in libQt6WaylandClient.so.6* libwayland-client.so.0* libwayland-cursor.so.0* \
               libxkbcommon.so.0* libQt6WlShellIntegration.so.6*; do
    while IFS= read -r f; do cp "$f" "$APPDIR/usr/lib/"; done \
        < <(find /usr/lib/x86_64-linux-gnu -maxdepth 1 -name "$pattern" 2>/dev/null)
done

# ── 4. linuxdeploy (Qt plugin) -> .AppImage ─────────────────────────────────
echo "==> Bundling with linuxdeploy..."
export LDAI_OUTPUT="$ROOT/mdit-x86_64.AppImage"
"$TOOLS/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --plugin qt \
    --desktop-file "$APPDIR/usr/share/applications/mdit.desktop" \
    --icon-file "$APPDIR/mdit.png" \
    --output appimage

# ── 5. Verify the published AppImage really carries Wayland ─────────────────
echo "==> Verifying the Wayland plugin made it into the AppImage..."
ls "$APPDIR"/usr/plugins/platforms/libqwayland*.so >/dev/null 2>&1 \
    || { echo "ERROR: no libqwayland*.so in the AppDir - the AppImage would be xcb-only." >&2; exit 1; }
ls "$APPDIR"/usr/lib/libQt6WaylandClient.so.6* >/dev/null 2>&1 \
    || { echo "ERROR: libQt6WaylandClient.so.6 missing from the AppDir." >&2; exit 1; }

echo
echo "==> Done: mdit-x86_64.AppImage"
ls -lh "$ROOT/mdit-x86_64.AppImage"
echo "==> Run it:  chmod +x mdit-x86_64.AppImage && ./mdit-x86_64.AppImage somefile.md"
