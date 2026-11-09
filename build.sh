#!/usr/bin/env bash
# Configure and build the native Linux app using the repository's CMake presets.
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./build.sh [--dev]

Build a native release in build/release, or a debug build with tests in
build/debug with --dev. Dependencies must already be installed (see README.md).
Flatpak bundles use scripts/build-flatpak.sh instead.

Set CMAKE_BUILD_PARALLEL_LEVEL to control build concurrency (default: 16).
ccache is used automatically when installed; configure with
-DQT_M4B_USE_CCACHE=OFF to disable it.
EOF
}

preset=linux-release
for arg in "$@"; do
    case "$arg" in
        --dev) preset=linux-debug ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'Unknown argument: %s\n' "$arg" >&2; usage >&2; exit 2 ;;
    esac
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$root"

cmake --preset "$preset"
cmake --build --preset "$preset" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-16}"

if [[ "$preset" == linux-debug ]]; then
    printf 'Built %s/build/debug/qt-m4b-player\nRun tests with: ctest --preset linux-debug\n' "$root"
else
    printf 'Built %s/build/release/qt-m4b-player\n' "$root"
fi
