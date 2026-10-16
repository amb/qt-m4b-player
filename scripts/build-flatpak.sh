#!/usr/bin/env bash
# Build a self-contained Flatpak bundle for qt-m4b-player.
#
# Prerequisites:
#   sudo apt install flatpak flatpak-builder
#   flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
#   flatpak install flathub org.kde.Sdk//6.10 org.kde.Platform//6.10 \
#     org.freedesktop.Platform.ffmpeg-full//25.08
#
# Usage:
#   scripts/build-flatpak.sh [output.flatpak]
#
# The bundle is a single file that can be attached to a GitHub release and
# installed with: flatpak install --user --reinstall <output.flatpak>
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app_id="io.github.amb.QtM4bPlayer"
manifest="$root/packaging/flatpak/$app_id.yml"
build_dir="$root/build/flatpak"
repo_dir="$root/build/flatpak-repo"
output="${1:-$root/build/$app_id.flatpak}"

if ! command -v flatpak-builder >/dev/null 2>&1; then
    echo "flatpak-builder is required: sudo apt install flatpak-builder" >&2
    exit 1
fi

mkdir -p "$(dirname "$output")"

# --disable-rofiles-fuse is only needed in restricted containers; expose it as
# an opt-in so normal desktops keep the faster FUSE path.
extra=()
if [[ "${FLATPAK_BUILDER_DISABLE_ROFILES_FUSE:-0}" == "1" ]]; then
    extra+=(--disable-rofiles-fuse)
fi

flatpak-builder --force-clean "${extra[@]}" --repo="$repo_dir" "$build_dir" "$manifest"
flatpak build-bundle "$repo_dir" "$output" "$app_id"

echo "Wrote $output"
echo "Install locally with: flatpak install --user --reinstall $output"
