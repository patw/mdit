#!/usr/bin/env bash
# Configure + build mdit with tests enabled. Run from anywhere.
set -euo pipefail
cd "$(dirname "$0")"

GEN="Ninja"
command -v ninja >/dev/null 2>&1 || GEN="Unix Makefiles"

cmake -S . -B build -G "$GEN" -DBUILD_TESTS=ON
cmake --build build --parallel
echo
echo "Build complete. Run tests with:"
echo "  ctest --test-dir build --output-on-failure"
