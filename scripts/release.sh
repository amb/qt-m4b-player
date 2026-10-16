#!/usr/bin/env bash
# Build the Flatpak bundle and publish a GitHub release with the gh CLI.
#
# Usage:
#   scripts/release.sh [tag]
#
# The tag defaults to v<project VERSION> from CMakeLists.txt. If the release
# already exists the bundle is re-uploaded with --clobber, so re-running after a
# rebuild is safe.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

app_id="io.github.amb.QtM4bPlayer"
version="$(awk '/^project\(qt-m4b-player VERSION/{print $3}' CMakeLists.txt)"
tag="${1:-v$version}"
bundle="$root/build/$app_id.flatpak"

command -v gh >/dev/null 2>&1 || { echo "gh is required: https://cli.github.com" >&2; exit 1; }
gh auth status >/dev/null 2>&1 || { echo "Run 'gh auth login' first." >&2; exit 1; }

if [[ -n "$(git status --porcelain)" ]]; then
    echo "warning: working tree has uncommitted changes" >&2
fi

"$root/scripts/build-flatpak.sh" "$bundle"

if gh release view "$tag" >/dev/null 2>&1; then
    gh release upload "$tag" "$bundle" --clobber
    echo "Uploaded $bundle to existing release $tag"
else
    gh release create "$tag" "$bundle" --title "$tag" --generate-notes
    echo "Created release $tag"
fi
