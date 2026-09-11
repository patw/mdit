#!/usr/bin/env python3
"""pytest shim for mdit.

This is what the ralph harness invokes. It configures the CMake build with
tests enabled, builds it, and runs the full CTest suite. Any configure, build,
or test failure raises an assertion so the run is marked red.

Run directly:
    pytest -q tests/test_build.py
or from the repo root:
    pytest -q
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(REPO_ROOT, "build")


def _generator() -> str:
    return "Ninja" if shutil.which("ninja") else "Unix Makefiles"


def _run(cmd: list[str], cwd: str = REPO_ROOT) -> int:
    print("\n+ " + " ".join(cmd) + f"\n  (cwd={cwd})", flush=True)
    proc = subprocess.run(cmd, cwd=cwd)
    return proc.returncode


def test_configure() -> None:
    """Configure with tests enabled (Ninja preferred, Make as fallback)."""
    rc = _run([
        "cmake",
        "-S", REPO_ROOT,
        "-B", BUILD_DIR,
        "-G", _generator(),
        "-DBUILD_TESTS=ON",
    ])
    assert rc == 0, "cmake configure failed"


def test_build() -> None:
    """Build all targets (the mdit executable + the test binaries)."""
    rc = _run([
        "cmake", "--build", BUILD_DIR,
        "--parallel",
    ])
    assert rc == 0, "cmake build failed"


def test_ctest() -> None:
    """Run the full CTest/QtTest suite; fail on any failure."""
    rc = _run([
        "ctest", "--test-dir", BUILD_DIR,
        "--output-on-failure",
    ])
    assert rc == 0, "ctest failed"
