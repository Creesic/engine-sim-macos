#!/bin/bash
#
# Double-click launcher for the native macOS build of engine-sim.
#
# engine-sim resolves its assets and its main.mr engine script with paths
# relative to the current directory (e.g. "../assets/main.mr"), so this
# launcher runs the binary from build-macos/ where those relative paths line
# up. It builds the app first if it hasn't been built yet.
#
set -euo pipefail

# Directory this script lives in (the repository root) -- works no matter
# where Finder or the shell invokes it from.
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$REPO_DIR/build-macos"
APP="$BUILD_DIR/engine-sim-app"

# Homebrew (Apple Silicon or Intel) may not be on a double-clicked script's
# PATH; add both prefixes so cmake is found if a build is needed.
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

echo "engine-sim - native macOS launcher"
echo "==================================="

if [ ! -x "$APP" ]; then
    echo "First run: building the app (this can take a few minutes)..."
    if ! command -v cmake >/dev/null 2>&1; then
        echo
        echo "ERROR: cmake was not found. Install the build tools first:"
        echo "    brew install cmake sdl2 sdl2_image boost"
        echo
        read -r -p "Press Return to close this window."
        exit 1
    fi
    cmake -S "$REPO_DIR" -B "$BUILD_DIR"
    cmake --build "$BUILD_DIR" --target engine-sim-app -j"$(sysctl -n hw.ncpu)"
fi

echo "Launching Engine Sim..."
echo "(Controls: A ignition, hold S to start, Q/W/E/R throttle, Up/Down gear,"
echo " Shift declutch, D dyno, H rpm-hold, F fullscreen, Esc quit)"
echo

# Run from build-macos so the app's relative asset/script paths resolve.
cd "$BUILD_DIR"
# `|| status=$?` keeps `set -e` from aborting before we can inspect a
# non-zero exit (so a crash shows a message instead of the window vanishing).
status=0
./engine-sim-app "$@" || status=$?

if [ "$status" -ne 0 ]; then
    echo
    echo "Engine Sim exited with status $status."
    read -r -p "Press Return to close this window."
fi
