# Releasing qt-m4b-player

Releases are **self-hosted**: a version tag on GitHub produces one Linux Flatpak
bundle attached to a GitHub release. Nothing is submitted to Flathub, a PPA or
another distribution channel.

The same source still installs with plain CMake (`cmake --install`) for people
who build it themselves; the Flatpak is the supported end-user artifact.

## Artifact and identifiers

| Item | Value |
| --- | --- |
| App id | `io.github.amb.QtM4bPlayer` |
| Command | `qt-m4b-player` |
| Bundle file | `io.github.amb.QtM4bPlayer.flatpak` |
| Runtime | `org.kde.Platform` / `org.kde.Sdk` `6.10` |

The app id is used for the sandbox, the desktop entry (`io.github.amb.QtM4bPlayer.desktop`),
the icon name and the Wayland `app_id`. Change it in one place only if you fork:
`CMakeLists.txt` install rules, `src/main.cpp` (`setDesktopFileName`), the desktop
file name and `Icon=`, the metainfo `<id>`/`<launchable>`, and the Flatpak
`app-id`.

## Versioning

`project(... VERSION x.y.z)` in `CMakeLists.txt`, `QCoreApplication::setApplicationVersion`
in `src/main.cpp`, and the `<releases>` entry in the AppStream metainfo must
agree. Tag the release `vx.y.z`.

## Prerequisites

```bash
sudo apt install flatpak flatpak-builder gh
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install flathub org.kde.Sdk//6.10 org.kde.Platform//6.10
```

## Build the bundle

```bash
scripts/build-flatpak.sh                 # -> build/io.github.amb.QtM4bPlayer.flatpak
scripts/build-flatpak.sh /tmp/out.flatpak
```

The manifest is `packaging/flatpak/io.github.amb.QtM4bPlayer.yml`. It builds three
modules the KDE runtime does not provide, then the app:

1. **libass** – subtitle/text shaping (mpv hard dependency).
2. **libplacebo** – GPU helper library (mpv 0.41 hard dependency). Built against
   the SDK's Vulkan and shaderc; the app itself renders no video.
3. **mpv** – a trimmed audio-only `libmpv` (`-Dcplayer=false`, no video outputs,
   PulseAudio only).
4. **qt-m4b-player** – the app, from the current checkout (`type: dir`).

The app links the runtime's FFmpeg. That base FFmpeg already includes the AAC
decoder, the `mov,mp4` demuxer and the `mov_text` decoder used for metadata,
chapters and captions; the `org.freedesktop.Platform.ffmpeg-full` extension is
not needed (and has no Flathub branch newer than 24.08).

## Test the bundle

```bash
flatpak install --user --reinstall build/io.github.amb.QtM4bPlayer.flatpak
flatpak run io.github.amb.QtM4bPlayer --dependency-versions
flatpak run io.github.amb.QtM4bPlayer /path/to/book.m4b
```

Check audio output, chapter navigation, cover art and captions. Confirm the UI
follows the Plasma light/dark colour scheme, and that the window icon and
task-manager entry resolve (no generic "?" icon).

## Publish

Releases are published with the [GitHub CLI](https://cli.github.com) rather than
CI. Commit your changes, then run:

```bash
scripts/release.sh            # tag defaults to v<project VERSION>
scripts/release.sh v0.1.0     # or pass an explicit tag
```

The script builds the Flatpak bundle and runs `gh release create` with
auto-generated notes. If the release already exists it re-uploads the bundle
with `--clobber`, so a rebuild can be published the same way. `gh` creates the
tag on the remote from the default branch if it does not exist yet; to tag the
source locally as well:

```bash
git tag v0.1.0
git push origin v0.1.0
```

`gh` must be authenticated (`gh auth login`) with `repo` access to the
destination repository.

## Sandbox access

`finish-args` grants `--filesystem=home` because the player opens arbitrary local
paths and remembers them in its library. Books on other mounts (for example
`/mnt/nas`) need an explicit override:

```bash
flatpak override --user --filesystem=/mnt/nas io.github.amb.QtM4bPlayer
```

## Theming inside the sandbox

The app does not ship a theme. Inside the KDE runtime it selects the
`org.kde.breeze` Quick Controls style when present (falling back to
`org.kde.desktop`, then Qt's default), reads the host colour scheme through the
KDE portal integration, and recolours its icons from the resulting palette.
If a system theme does not apply, first check that
`xdg-desktop-portal-kde` is running on the host.

## Open items

- The Flatpak manifest has not yet been built end to end. Run
  `scripts/build-flatpak.sh` once and fix any module drift before the first tag.
- AppStream screenshots are not included yet, so software-center listings show
  no preview images. Add a `<screenshots>` section to the metainfo later.
