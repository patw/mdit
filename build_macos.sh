#!/bin/bash
# Build mdit for macOS: a .app bundle (Qt bundled via macdeployqt, with the mdit
# icon converted to .icns) and a .dmg around it.
#
# Adapted from PengyCPP's build_macos.sh. mdit sets MACOSX_BUNDLE in CMake, so
# CMake produces build/mdit.app; this script adds the .icns (generated on the fly
# with sips/iconutil from the committed PNGs - no binary artwork in git), runs
# macdeployqt, and wraps the bundle in a dmg.
#
# Prerequisites:  brew install qt@6 cmake ninja
# Usage:  ./build_macos.sh [arm64|x86_64]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

ARCH="${1:-$(uname -m)}"
export CMAKE_PREFIX_PATH="$(brew --prefix qt@6 2>/dev/null || echo /opt/homebrew/opt/qt@6)"
export PATH="$CMAKE_PREFIX_PATH/bin:$PATH"

VERSION="${VERSION:-$(grep -A5 -m1 '^project(' CMakeLists.txt | tr '\n' ' ' | sed -n 's/.*VERSION[[:space:]]\+\([0-9][^ )]*\).*/\1/p')}"
VERSION="${VERSION#v}"
[[ -n "$VERSION" ]] || { echo "ERROR: could not determine the version" >&2; exit 1; }

MACDEPLOYQT="$(command -v macdeployqt || echo "$CMAKE_PREFIX_PATH/bin/macdeployqt")"
[ -x "$MACDEPLOYQT" ] || { echo "ERROR: macdeployqt not found (is Qt6 installed?)" >&2; exit 1; }

# ── 1. Build ────────────────────────────────────────────────────────────────
echo "==> Building mdit $VERSION for macOS ($ARCH)..."
cmake -S "$ROOT" -B "$ROOT/build_macos" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
    -DBUILD_TESTS=OFF
cmake --build "$ROOT/build_macos" --parallel "$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

APP="$ROOT/build_macos/mdit.app"
[ -d "$APP" ] || { echo "ERROR: $APP was not produced" >&2; exit 1; }

# ── 2. Smoke test (the bundle's binary runs and reports its version) ────────
echo "==> Smoke testing the bundle..."
"$APP/Contents/MacOS/mdit" --version

# ── 3. Icon: PNG -> .iconset -> .icns (generated, never committed) ──────────
echo "==> Generating the .icns from assets/icons/..."
ICONSET="$ROOT/build_macos/mdit.iconset"
rm -rf "$ICONSET"
mkdir -p "$ICONSET" "$APP/Contents/Resources"
# macOS wants specific names/sizes; generate each from the matching PNG.
for size in 16 32 48 64 128 256 512; do
    src="$ROOT/assets/icons/mdit-${size}.png"
    [ -f "$src" ] || continue
    sips -z "$size" "$size" "$src" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
done
# The @2x variants macOS expects (copy the next size up).
for size in 16 32 128 256; do
    double=$((size * 2))
    src="$ICONSET/icon_${double}x${double}.png"
    [ -f "$src" ] && cp "$src" "$ICONSET/icon_${size}x${size}@2x.png"
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/mdit.icns"
/usr/libexec/PlistBuddy -c "Set :CFBundleIconFile mdit.icns" "$APP/Contents/Info.plist" 2>/dev/null \
    || /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string mdit.icns" "$APP/Contents/Info.plist"
# Also declare the file types the app can open.
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes array" "$APP/Contents/Info.plist" 2>/dev/null || true

# ── 4. Bundle Qt into the app ───────────────────────────────────────────────
echo "==> Bundling Qt with macdeployqt..."
"$MACDEPLOYQT" "$APP" -always-overwrite -qmldir="$ROOT" >/dev/null

# ── 5. Wrap it in a .dmg ────────────────────────────────────────────────────
DMG="$ROOT/mdit-${VERSION}-macOS.dmg"
echo "==> Building $DMG..."
rm -f "$DMG"
STAGE="$ROOT/build_macos/dmg"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -a "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
hdiutil create -volname "mdit $VERSION" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
rm -rf "$STAGE"

echo
echo "==> Done: $(basename "$DMG") ($(du -h "$DMG" | cut -f1))"
