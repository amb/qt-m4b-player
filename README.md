# qt-m4b-player

A separate C++20 / Qt Quick audiobook player project for Linux, with Android
planned. Playback uses libmpv; FFmpeg reads metadata, chapters, artwork and
embedded `mov_text` captions; Qt SQL manages a local SQLite library.

The macOS app is a read-only product and behavior reference. This project has
its own sources, database, build setup and copies of the synthetic test fixtures.
See [architecture](docs/ARCHITECTURE.md) for ownership, playback rules and the
startup constraints, and [schema and feature parity](docs/SCHEMA_AND_FEATURE_PARITY.md)
for storage and the remaining feature gaps.

## Status

This is the first runnable **Linux prototype**, not complete macOS feature parity.
It provides local file opening, paused resume, play/pause, ten-second skips,
chapter-scoped scrubbing, pause-and-remember chapter navigation, Stop, cover art,
pitch-preserving speed changes, volume, recent locations and embedded captions
with audio-timed automatic overflow scrolling. The star popup offers half-star
ratings and opens the per-book Notes Save/Cancel editor. A floating speech-tone EQ offers warm/clear tilt, a
logarithmic pivot, peak-limited Boost, bypass, reset and automatic output headroom. In Fill mode,
the transport panel blurs the artwork behind it, including with software rendering.
Progress and per-book CC / Stop
After Chapter choices are saved in SQLite. Playback never changes audiobook files.
The desktop window remembers its normal size and maximized state between launches.
Window placement is controlled by the desktop, including KDE on Wayland.
Caption text size and Fill cover background are global preferences saved as soon
as they change, whether adjusted in View or Appearance.

Stop After Chapter currently pauses after observing a boundary and seeks back to
the boundary. It does **not** yet provide the macOS engine-side audio cutoff;
brief next-chapter audio is possible. Caption indexing scans the file in a
cancellable background job, so large books can take time to enable CC.

Still planned: additional appearance preferences, a
configurable playback-rate list, more global defaults (colors, fonts),
independent windows, drag/drop, Linux MPRIS, full accessibility validation,
precise chapter cutoff, library restore/recovery and Android packaging/media
services.
See [database schema and feature parity](docs/SCHEMA_AND_FEATURE_PARITY.md) for
the exact Qt schema and the current feature gaps.

## Install the Flatpak

[v0.2.0](https://github.com/amb/qt-m4b-player/releases/tag/v0.2.0) is the latest
published Linux x86_64 release. Download `io.github.amb.QtM4bPlayer.flatpak`
from that release, then run the following from the download directory with
Flatpak installed:

```bash
flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.kde.Platform//6.10
flatpak install --user ./io.github.amb.QtM4bPlayer.flatpak
flatpak run io.github.amb.QtM4bPlayer
```

The app is distributed through GitHub releases; Flathub supplies the KDE runtime.
To update an existing installation, close the player, download the new bundle and run:

```bash
flatpak install --user --reinstall ./io.github.amb.QtM4bPlayer.flatpak
```

GitHub releases do not provide automatic Flatpak updates.
Books in your home directory are accessible by default. For books on another
mount, grant access to that directory, for example:

```bash
flatpak override --user --filesystem=/mnt/nas io.github.amb.QtM4bPlayer
```

See [releasing](docs/RELEASING.md) for building and publishing bundles, sandbox
access and KDE theming. Newer source changes on `main` may follow the published
release.

## Dependencies and build

Use apt for the initial Linux development environment. No Conan, vcpkg or
download-at-configure dependency system is used. The tested host is Ubuntu
26.04.1 x86_64; see [exact dependency versions](docs/DEPENDENCIES.md).

Install dependencies yourself. All privileged commands are run by the developer;
nothing in this repository invokes `sudo`:

```bash
sudo apt install build-essential cmake ninja-build pkg-config python3 \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  libmpv-dev libavformat-dev libavcodec-dev libavutil-dev libavfilter-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-dialogs \
  qml6-module-qtquick-layouts qml6-module-qtqml-workerscript \
  libqt6sql6-sqlite libqt6svg6
```

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
cmake --build build/debug --target qt-m4b-player_qmllint
./build/debug/qt-m4b-player /path/to/book.m4b
```

CTest sets a UTF-8 C locale and an isolated Qt software-rendering configuration,
so the tests also work on hosts with a comma-decimal locale or KDE styling.
The application sets `LC_NUMERIC` to `C` itself. See the startup constraints in
[architecture](docs/ARCHITECTURE.md).

Without a file argument, the last available recent location opens paused.
Use **File → Open Audiobook**, **File → Open Recent**, or Ctrl+O to choose a book.
The menu bar provides File, Playback, View and Help menus, including speed, volume,
chapters, captions, appearance and full screen. Space toggles playback; Left/Right
skip ten seconds; Ctrl+Left/Right changes chapters. **Help → Player Help** lists
the shortcuts. The lower transport controls provide the same playback actions.
Chapters, rating/notes, playback speed and volume use matching popups above their
buttons, with the same background dimming and active-button highlight. Escape or
an outside click dismisses a popup; long lists scroll to stay within the window.
Use Up/Down and Enter to choose a chapter or speed. Rating and volume adjustments
stay open, and their arrow keys do not seek playback.
Click the **star** in the transport controls for half-star ratings and
**Open notes**. Left/Right changes the rating by half a star; Home clears it.
**File → Notes** or Ctrl+N also opens the current book's plain-text notes; Ctrl+S saves
and Escape/Cancel discards the draft. Empty text clears the note. Notes follow
the book across renamed copies, and a failed save keeps the editor and draft open.
**Playback → Open EQ**, Ctrl+E, or **Open EQ** below the volume slider opens a
reusable floating panel. EQ starts disabled. Tone spans −12 to +12 dB, and the
balance point spans 200–4,000 Hz logarithmically. Reset restores neutral tone at
1,000 Hz and zero Boost without changing bypass. Boost spans 0 to +24 dB and
lifts quiet passages through a lookahead limiter, rather than hard-clipping loud
peaks. Higher boost reduces dynamics; it cannot repair distortion already in a
recording. Peak protection runs after pitch-preserving speed processing and uses
4× oversampling with a −1 dB output ceiling. Double-click a knob or press Space while it
has focus to reset that knob. Settings survive book changes for the current
session and are not saved across launches. Output headroom is −abs(tone) dB;
the graph displays the tone response before this trim, Boost and limiting. The Qt implementation
coalesces live adjustments into filter updates; it does not reproduce the macOS
engine's per-sample coefficient smoothing.

**Auto loudness: −18 LKFS** measures the current chapter in a cancellable
background job, available only for chapters longer than one minute. It accounts
for the current tone/headroom settings, then sets a fixed Boost amount and enables
EQ. Quiet chapters are raised toward −18 LKFS; louder chapters need no boost.
It does not continually adjust sentence-to-sentence volume. Silence and failed
measurements leave Boost unchanged. Changing books, chapters or EQ settings
cancels an active measurement. The +24 dB cap and peak limiter may keep very quiet
or highly dynamic material below the target; the result is shown in the panel.
Measurement never changes the audiobook file or the main volume setting.
The CC command enables once a usable timed track has been indexed. The bottom
panel uses one control row in wide windows and two rows in narrow windows, with
the chapter, elapsed time and remaining-book percentage above the seek bar.
The elapsed-time readout stays exactly centered when all three labels fit on one
line, reserving only its natural text width. Otherwise, the chapter moves to its
own row. In Fill mode, drag the cover vertically to reframe its crop; framing is
clamped to the image edges, retained through resizing, and reset for a new book.
A drag does not toggle playback. Cover framing is not persisted across launches.

Transport icons are vendored from Phosphor Icons 2.1.1 under the MIT license.
Prefer vendored, freely licensed icons over platform-specific symbol
dependencies.
See [icon provenance and license](assets/icons/README.md); no icon font or
network download is needed at build time or runtime.

For an isolated test library and a headless UI smoke check:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/debug/qt-m4b-player --smoke-test --data-dir /tmp/qt-m4b-test \
  tests/fixtures/captions.m4b
```

`--no-resume` starts empty. `--data-dir DIRECTORY` isolates the database/cache/settings.
`--dependency-versions` prints the runtime Qt, mpv engine and SQLite versions.
`--screenshot PATH` writes a window image while `--smoke-test` runs.
`linux-release` is the release configure/build preset. The optional CMake install
target installs the executable, desktop entry, AppStream metadata and icons; it
is not a self-contained bundle. Self-hosted Flatpak bundles built from
`packaging/flatpak/` are attached to GitHub releases; see
[releasing](docs/RELEASING.md).

The UI follows the desktop colour scheme and ships no palette of its own. On KDE
Plasma install `qml6-module-org-kde-breeze` (or
`qml6-module-org-kde-qqc2desktopstyle`) so the KDE Quick Controls style is used;
otherwise the app falls back to Qt's default style. Transport icons are
recoloured on the CPU from the active palette, including with Qt's software renderer.

## Storage and development

The standard Qt application data directory is used (normally
`~/.local/share/qt-m4b-player/qt-m4b-player/`). It contains `library.sqlite` and a
replaceable `covers/` cache, plus `settings.ini` for window size/state and appearance. Debug,
release and installed builds share these settings when using the same data directory.
Saved sizes are bounded by the current screen and the UI's minimum size.
The database keeps multiple locations for a book and uses a `STRICT` schema with
write-ahead logging, a foreign key that cascades location cleanup and an
`application_id` stamp. Upgrading an older schema writes a timestamped backup
next to the database first, and a file that is not recognised as this app's
library is rejected without being modified.
The reference app's `quick:v1` sampled fingerprint algorithm is preserved; its
macOS database is never opened or migrated by this project.

The [architecture and remaining parity work](docs/ARCHITECTURE.md) describe state
ownership, platform boundaries and the next milestones. Tests exercise real
FFmpeg extraction and libmpv playback with the silent output backend; they do
not establish audible quality or real-device Android behavior.

For an additional real book, run the optional read-only integration check. Use an
**absolute path** because CTest runs from the build directory:

```bash
QT_M4B_TEST_BOOK=/absolute/path/to/book.m4b ctest --preset linux-debug
```

The check selects Chapter Twelve when the book provides one.

Local debug copies can live in `.cache/books/`, which Git ignores, and the
reference UI screenshots are in `.cache/reference/`. Use a local copy for routine
development and the network-share path only when validating share loading,
streaming and seeking. The developer inspection command reads metadata without
changing the book:

```bash
./build/debug/media-inspect /path/to/book.m4b
./build/debug/media-inspect --captions /path/to/book.m4b
```

The first real NAS book check found 56 chapters and 5,489 timed cues in a
roughly 12-hour M4B. Metadata read took about 0.2 seconds and full caption indexing
about 7.5 seconds on that run. The optional libmpv load/seek/background-index
check passed with silent output and an isolated temporary library. These timings
are observations from one share/cache state, not latency guarantees.

## License

MIT; see [LICENSE](LICENSE). The vendored Phosphor icons are MIT as well
([provenance](assets/icons/README.md)).
