#!/bin/bash
# Build a mdit .deb package.
#
# The payload comes from mdit's OWN `cmake --install` rules, so the package can
# never drift from what a source install produces:
#   /usr/bin/mdit
#   /usr/share/applications/mdit.desktop              (Icon=mdit, StartupWMClass=mdit)
#   /usr/share/icons/hicolor/<32..512>/apps/mdit.png  (the mark, every size)
# plus the Debian control/copyright/postinst scaffolding.
#
# The postinst refreshes the desktop database and the icon cache so the entry and
# its icon show up for every user (GNOME/Ubuntu Dock included) without a logout.
#
# Prerequisites (Ubuntu/Debian):
#   sudo apt install build-essential cmake ninja-build qt6-base-dev libgl-dev dpkg-dev
#
# Usage:  ./build_deb.sh            # build + package
#         VERSION=v0.2.0 ./build_deb.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

if [[ -n "${JOBS:-}" ]]; then
    BUILD_JOBS="$JOBS"
elif command -v nproc >/dev/null 2>&1; then
    BUILD_JOBS="$(nproc)"
else
    BUILD_JOBS=4
fi

# One source of truth for the version: CMakeLists.txt (project(... VERSION ...)).
# A tag/CI-supplied VERSION wins, with the leading "v" stripped — dpkg requires a
# digit-first version, and forgetting this is exactly how the release pipeline
# broke once before (see AGENTS.md / BotTalk).
VERSION="${VERSION:-$(grep -A5 -m1 '^project(' CMakeLists.txt | tr '\n' ' ' | sed -n 's/.*VERSION[[:space:]]\+\([0-9][^ )]*\).*/\1/p')}"
VERSION="${VERSION#v}"
[[ -n "$VERSION" ]] || { echo "ERROR: could not determine the version" >&2; exit 1; }

ARCH="$(dpkg --print-architecture)"
PKG="mdit_${VERSION}_${ARCH}"
STAGING="$ROOT/.deb_staging/$PKG"

# ── 1. Build ────────────────────────────────────────────────────────────────
echo "==> Building mdit $VERSION..."
# A dedicated build dir: the packaging scripts must never clobber the
# developer's ./build (which is configured with -DBUILD_TESTS=ON).
BUILD_DIR="$ROOT/build_package"
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --parallel "$BUILD_JOBS"

# ── 2. Smoke test the binary before packaging it ────────────────────────────
echo "==> Smoke testing the binary..."
if ! "$BUILD_DIR/mdit" --version >/dev/null 2>&1; then
    echo "ERROR: build/mdit --version failed (stale or broken build?)" >&2
    exit 1
fi
"$BUILD_DIR/mdit" --version

# ── 3. Assemble the staging tree from `cmake --install` ─────────────────────
echo "==> Assembling the package staging tree..."
rm -rf "$ROOT/.deb_staging"
mkdir -p "$STAGING/DEBIAN" "$STAGING/usr/share/doc/mdit"
cmake --install "$BUILD_DIR" --prefix "$STAGING/usr" --strip >/dev/null

# Every icon size the .desktop's Icon=mdit may be resolved at must be present.
for size in 32 48 64 128 256 512; do
    test -f "$STAGING/usr/share/icons/hicolor/${size}x${size}/apps/mdit.png" \
        || { echo "ERROR: missing ${size}x${size} icon in the package" >&2; exit 1; }
done
test -f "$STAGING/usr/share/applications/mdit.desktop" \
    || { echo "ERROR: the .desktop file did not install" >&2; exit 1; }

INSTALLED_KB=$(du -sk "$STAGING/usr" | cut -f1)

# ── 4. DEBIAN/control ──────────────────────────────────────────────────────
# Qt6 runtime deps, with the pre-/post-time_t64 alternations Ubuntu needs.
cat > "$STAGING/DEBIAN/control" <<EOF
Package: mdit
Version: $VERSION
Architecture: $ARCH
Maintainer: Pat Wendorf <dungeons@gmail.com>
Installed-Size: $INSTALLED_KB
Depends: libc6 (>= 2.35), libgcc-s1, libstdc++6,
 libqt6core6t64 | libqt6core6, libqt6gui6t64 | libqt6gui6,
 libqt6widgets6t64 | libqt6widgets6, libqt6printsupport6t64 | libqt6printsupport6,
 libgl1
Section: editors
Priority: optional
Homepage: https://catbee.ca
Description: mdit - a fast, pure-Qt6 specialty markdown editor
 mdit opens .md/.markdown files in a 50/50 split view: a syntax-highlighted
 editor on the left, a live-rendered preview on the right (Qt's built-in
 CommonMark/GFM renderer - no QtWebEngine, no external markdown library).
 .
 Features: undo/redo, find & replace, find-in-document, HTML/PDF export,
 recent files, an unsaved-changes guard, light/dark themes with 8 accents and a
 UI scale, and a themable markdown highlighter. mdit is a Catbee project.
EOF

# ── 5. copyright (a well-formed .deb requires it) ──────────────────────────
cat > "$STAGING/usr/share/doc/mdit/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: mdit
Source: https://catbee.ca

Files: *
Copyright: 2026 Pat Wendorf
License: MIT
EOF
# Debian policy: the changelog must exist and be gzipped.
{
    echo "mdit ($VERSION) unstable; urgency=medium"
    echo
    echo "  * Release $VERSION."
    echo
    echo " -- Pat Wendorf <dungeons@gmail.com>  $(date -R)"
} > "$STAGING/usr/share/doc/mdit/changelog.Debian"
gzip -9n "$STAGING/usr/share/doc/mdit/changelog.Debian"

# ── 6. postinst: make the entry + icon visible to every user right away ────
cat > "$STAGING/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true
fi
if command -v xdg-desktop-menu >/dev/null 2>&1; then
    xdg-desktop-menu forceupdate >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 755 "$STAGING/DEBIAN/postinst"

# ── 7. Build the .deb ──────────────────────────────────────────────────────
echo "==> Building the .deb..."
dpkg-deb --build --root-owner-group "$STAGING" "$ROOT/${PKG}.deb" >/dev/null
rm -rf "$ROOT/.deb_staging"

echo
echo "==> Done: ${PKG}.deb"
dpkg-deb --contents "$ROOT/${PKG}.deb" | sed 's/^/    /'
echo
echo "==> Install with:  sudo apt install ./${PKG}.deb"
echo "    (then mdit shows up in the app grid / dock with its own icon)"
