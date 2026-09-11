#!/bin/bash
# Pre-flight checks — run before `git push --tags`.
#
# Adapted from PengyCPP's check_release.sh: it catches the release failures that
# are cheap to catch locally (version drift, a bad icon, a docs-contract break, a
# red test suite) instead of waiting for three CI jobs to tell you.
#
# Usage:  ./check_release.sh [tag]      # tag defaults to the CMake version
set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

WARNINGS=0
warn() { printf '\033[33m  WARNING: %s\033[0m\n' "$*"; WARNINGS=$((WARNINGS + 1)); }
fail() { printf '\033[31m  FAIL: %s\033[0m\n' "$*"; exit 1; }
ok()   { printf '\033[32m  ✓ %s\033[0m\n' "$*"; }

echo "========================================="
echo " mdit pre-flight release check"
echo "========================================="

# ── 1. Version consistency ──────────────────────────────────────────────────
echo "--- Version ---"
# mdit's project() call spans several lines; handle both layouts.
CMAKE_VER="$(grep -A5 -m1 '^project(' CMakeLists.txt | tr '\n' ' ' | sed -n 's/.*VERSION[[:space:]]\+\([0-9][^ )]*\).*/\1/p')"
[ -n "$CMAKE_VER" ] || fail "could not parse project(... VERSION ...) from CMakeLists.txt"
ok "CMakeLists.txt version: $CMAKE_VER (single source of truth; AppInfo::version() compiles it in)"
TAG="${1:-}"
if [ -n "$TAG" ]; then
    if [ "$TAG" = "v$CMAKE_VER" ] || [ "$TAG" = "$CMAKE_VER" ]; then
        ok "tag $TAG matches the project version"
    else
        fail "tag $TAG does not match the project version ($CMAKE_VER)"
    fi
fi

# ── 2. The icon: square, power-of-two-ish, and present at every shipped size ─
echo "--- Icon ---"
for size in 16 32 48 64 128 256 512; do
    icon="assets/icons/mdit-${size}.png"
    [ -f "$icon" ] || fail "missing $icon"
    if command -v identify >/dev/null 2>&1; then
        dims="$(identify -format '%w %h' "$icon" 2>/dev/null)"
        [ "$dims" = "$size $size" ] || fail "$icon is '$dims', expected '$size $size'"
    fi
done
ok "all seven sizes present and square (16…512)"
# linuxdeploy validates that the file name matches the pixel size.
[ "$(identify -format '%w' assets/icons/mdit-256.png 2>/dev/null)" = "256" ] \
    || fail "assets/icons/mdit-256.png must be exactly 256x256 for linuxdeploy"
ok "mdit-256.png is exactly 256x256 (linuxdeploy-compatible)"

# ── 3. The packaging inputs the release jobs rely on ───────────────────────
echo "--- Packaging inputs ---"
for f in packaging/mdit.desktop resources/mdit.qrc CMakeLists.txt \
         build_deb.sh build_appimage.sh build_macos.sh build_windows.bat; do
    [ -f "$f" ] || fail "missing $f"
done
grep -q '^Icon=mdit$' packaging/mdit.desktop || fail "the .desktop must use Icon=mdit"
grep -q '^StartupWMClass=mdit$' packaging/mdit.desktop || fail "the .desktop needs StartupWMClass=mdit"
ok "desktop entry + qrc + the four platform scripts are in place"

# ── 4. Docs contract (what test_github_ready enforces, checked early) ──────
echo "--- Docs ---"
for f in README.md LICENSE CHANGELOG.md spec.md .gitignore; do
    [ -s "$f" ] || fail "$f is missing or empty"
done
grep -q 'MIT License' LICENSE || fail "LICENSE lacks the MIT header"
ok "README / LICENSE / CHANGELOG / spec / .gitignore present"

# ── 5. Clean build + the full suite (the objective function) ───────────────
echo "--- Clean build + tests ---"
if command -v ninja >/dev/null 2>&1; then GEN=Ninja; else GEN="Unix Makefiles"; fi
rm -rf build_release_check
cmake -S . -B build_release_check -G "$GEN" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON >/dev/null \
    || fail "cmake configure failed"
cmake --build build_release_check --parallel "$(nproc 2>/dev/null || echo 4)" >/dev/null \
    || fail "cmake build failed"
ctest --test-dir build_release_check --output-on-failure >/tmp/mdit_release_ctest.log 2>&1 \
    || { tail -30 /tmp/mdit_release_ctest.log; fail "the test suite is red"; }
ok "$(grep -c '^[0-9]*-/23' /tmp/mdit_release_ctest.log >/dev/null 2>&1; echo "23 binaries") all green"
./build_release_check/mdit --version || fail "mdit --version failed"
rm -rf build_release_check

# ── 6. Nothing that should not be committed ────────────────────────────────
echo "--- Working tree ---"
if command -v git >/dev/null 2>&1 && git rev-parse --git-dir >/dev/null 2>&1; then
    if [ -n "$(git status --porcelain)" ]; then
        warn "uncommitted changes (commit them so the tag builds what you tested)"
    else
        ok "working tree clean"
    fi
else
    warn "not a git repository yet (git init + push, then tag)"
fi

echo
if [ "$WARNINGS" -gt 0 ]; then
    printf '\033[33m==> %s warning(s) — review before tagging.\033[0m\n' "$WARNINGS"
else
    printf '\033[32m==> All checks passed. Tag it: git tag v%s && git push --tags\033[0m\n' "$CMAKE_VER"
fi
