#!/bin/bash
# Build mdit for macOS: a .app bundle (Qt bundled via macdeployqt, with the mdit
# icon converted to .icns) and a .dmg around it.
#
# Adapted from PengyCPP's build_macos.sh. mdit sets MACOSX_BUNDLE in CMake, so
# CMake produces build/mdit.app; this script adds the .icns (generated on the fly
# with sips/iconutil from the committed PNGs - no binary artwork in git), runs
# macdeployqt, and wraps the bundle in a dmg.
#
# Prerequisites: brew install qt cmake ninja (qt@6 also works when available)
# Usage: ./build_macos.sh [arm64|x86_64]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

ARCH="${1:-$(uname -m)}"

# Homebrew currently ships Qt 6 as `qt`; older installations may still provide
# `qt@6`. Prefer an installed formula rather than assuming either layout.
QT_PREFIX=""
for formula in qt qt@6; do
    if brew list --versions "$formula" >/dev/null 2>&1; then
        QT_PREFIX="$(brew --prefix "$formula")"
        break
    fi
done
QT_PREFIX="${QT_PREFIX:-/opt/homebrew/opt/qt}"
[ -d "$QT_PREFIX" ] || { echo "ERROR: Qt 6 was not found; run: brew install qt" >&2; exit 1; }
export CMAKE_PREFIX_PATH="$QT_PREFIX"
export PATH="$QT_PREFIX/bin:$PATH"

VERSION="${VERSION:-$(grep -A5 -m1 '^project(' CMakeLists.txt | tr '\n' ' ' | sed -n 's/.*VERSION[[:space:]]*\([0-9][^ )]*\).*/\1/p')}"
VERSION="${VERSION#v}"
[[ -n "$VERSION" ]] || { echo "ERROR: could not determine the version" >&2; exit 1; }

MACDEPLOYQT="$(command -v macdeployqt || echo "$CMAKE_PREFIX_PATH/bin/macdeployqt")"
[ -x "$MACDEPLOYQT" ] || { echo "ERROR: macdeployqt not found (is Qt6 installed?)" >&2; exit 1; }

# Ninja is preferred, but a source checkout should still package successfully
# on a Mac that only has the Xcode Command Line Tools' Unix Makefiles generator.
if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
else
    GENERATOR="Unix Makefiles"
fi

# ── 1. Build ────────────────────────────────────────────────────────────────
echo "==> Building mdit $VERSION for macOS ($ARCH, $GENERATOR)..."
rm -rf "$ROOT/build_macos"
cmake -S "$ROOT" -B "$ROOT/build_macos" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
    -DBUILD_TESTS=OFF
cmake --build "$ROOT/build_macos" --parallel "$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

APP="$ROOT/build_macos/mdit.app"
[ -d "$APP" ] || { echo "ERROR: $APP was not produced" >&2; exit 1; }

# ── 2. Smoke test (the bundle's binary runs and reports its version) ────────
echo "==> Smoke testing the bundle..."
QT_QPA_PLATFORM=offscreen "$APP/Contents/MacOS/mdit" --version

# ── 3. Icon: PNG -> .iconset -> .icns (generated, never committed) ──────────
echo "==> Generating the .icns from assets/icons/..."
ICONSET="$ROOT/build_macos/mdit.iconset"
rm -rf "$ICONSET"
mkdir -p "$ICONSET" "$APP/Contents/Resources"
# iconutil accepts only this canonical iconset naming scheme. Generate every
# required 1x/2x variant from committed artwork; the 1024px 512@2x variant is
# an intentional upscale of the 512px source (macOS otherwise rejects the set).
for size in 16 32 128 256 512; do
    src="$ROOT/assets/icons/mdit-${size}.png"
    [ -f "$src" ] || { echo "ERROR: missing $src" >&2; exit 1; }
    sips -z "$size" "$size" "$src" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null

    double=$((size * 2))
    sips -z "$double" "$double" "$src" \
        --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/mdit.icns"
/usr/libexec/PlistBuddy -c "Set :CFBundleIconFile mdit.icns" "$APP/Contents/Info.plist" 2>/dev/null \
    || /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string mdit.icns" "$APP/Contents/Info.plist"
# Also declare the file types the app can open.
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes array" "$APP/Contents/Info.plist" 2>/dev/null || true

# ── 4. Bundle Qt into the app ───────────────────────────────────────────────
# mdit is a Widgets application; it has no QML imports. Homebrew Qt 6.11's
# default macdeployqt plugin sweep pulls unrelated QML/image plugins (and their
# optional QtPdf/QtSvg/VirtualKeyboard dependencies), which can leave an invalid
# bundle. Deploy only the platform plugin mdit actually needs, and ask
# macdeployqt to fix its framework paths as an explicit executable.
echo "==> Bundling Qt with macdeployqt..."
"$MACDEPLOYQT" "$APP" -always-overwrite -no-plugins >/dev/null
PLATFORM_DIR="$APP/Contents/PlugIns/platforms"
mkdir -p "$PLATFORM_DIR"
QT_PLUGIN_DIR="$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
COCOA_PLUGIN="$QT_PLUGIN_DIR/platforms/libqcocoa.dylib"
[ -f "$COCOA_PLUGIN" ] || { echo "ERROR: missing Qt Cocoa platform plugin: $COCOA_PLUGIN" >&2; exit 1; }
cp "$COCOA_PLUGIN" "$PLATFORM_DIR/"
"$MACDEPLOYQT" "$APP" -always-overwrite -no-plugins \
    -executable="$PLATFORM_DIR/libqcocoa.dylib" >/dev/null

# macdeployqt ad-hoc signs while rewriting dependencies, but re-sign once after
# every copy/rewrite so modern macOS accepts the final nested-code layout.
codesign --force --deep --sign - "$APP" >/dev/null
codesign --verify --deep --strict "$APP"

# The packaged app deliberately contains Cocoa, not the test-only offscreen
# plugin. A normal --version launch verifies the isolated bundle can start.
env -i PATH="/usr/bin:/bin" HOME="$HOME" "$APP/Contents/MacOS/mdit" --version \
    | grep -Fx "mdit $VERSION" >/dev/null \
    || { echo "ERROR: bundled app failed its isolated version smoke test" >&2; exit 1; }

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
