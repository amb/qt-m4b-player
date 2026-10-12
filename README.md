# qt-m4b-player

A separate C++20 / Qt Quick audiobook player project for Linux, with Android
planned. Playback uses libmpv; FFmpeg reads metadata, chapters, artwork and
embedded `mov_text` captions; Qt SQL manages a local SQLite library.

The macOS app is a read-only product and behavior reference. This project has
its own sources, database, build setup and copies of the synthetic test fixtures.
See [HANDOFF.md](HANDOFF.md) to continue development from the latest session.

## Status

This is the first runnable **Linux prototype**, not complete macOS feature parity.
It provides local file opening, paused resume, play/pause, ten-second skips,
chapter-scoped scrubbing, pause-and-remember chapter navigation, Stop, cover art,
pitch-preserving speed changes, volume, recent locations and embedded captions
with audio-timed automatic overflow scrolling. Progress and per-book CC / Stop
After Chapter choices are saved in SQLite. Playback never changes audiobook files.
The desktop window remembers its normal size and maximized state between launches.
Window placement is controlled by the desktop, including KDE on Wayland.
Caption text size and Fill cover background are global preferences saved as soon
as they change, whether adjusted in View or Appearance.

Stop After Chapter currently pauses after observing a boundary and seeks back to
the boundary. It does **not** yet provide the macOS engine-side audio cutoff;
brief next-chapter audio is possible. Caption indexing scans the file in a
cancellable background job, so large books can take time to enable CC.

Still planned: notes/rating UI, additional appearance preferences,
independent windows, drag/drop, Linux MPRIS, full accessibility validation,
precise chapter cutoff, migration backups and Android packaging/media services.
The notes/rating schema fields are reserved, without an editor yet.
See [database schema and feature parity](docs/SCHEMA_AND_FEATURE_PARITY.md) for
the exact Qt schema, macOS differences and the current feature gaps.

## Dependencies and build

Use apt for the initial Linux development environment. No Conan, vcpkg or
download-at-configure dependency system is used. The tested host is Ubuntu
26.04.1 x86_64; see [exact dependency versions](docs/DEPENDENCIES.md).

Install dependencies yourself:

```bash
sudo apt install build-essential cmake ninja-build pkg-config python3 \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  libmpv-dev libavformat-dev libavcodec-dev libavutil-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-dialogs \
  qml6-module-qtquick-layouts qml6-module-qtqml-workerscript libqt6sql6-sqlite libqt6svg6
```

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
./build/debug/qt-m4b-player /path/to/book.m4b
```

Without a file argument, the last available recent location opens paused.
Use **File → Open Audiobook**, **File → Open Recent**, or Ctrl+O to choose a book.
The menu bar provides File, Playback, View and Help menus, including speed, volume,
chapters, captions, appearance and full screen. Space toggles playback; Left/Right
skip ten seconds; Ctrl+Left/Right changes chapters. **Help → Player Help** lists
the shortcuts. The lower transport controls provide the same playback actions.
The CC command enables once a usable timed track has been indexed. The bottom
panel uses one control row in wide windows and two rows in narrow windows, with
the chapter, elapsed time and remaining-book percentage above the seek bar.
The elapsed-time readout stays exactly centered when all three labels fit on one
line, reserving only its natural text width. Otherwise, the chapter moves to its
own row. In Fill mode, drag the cover vertically to reframe its crop; framing is
clamped to the image edges, retained through resizing, and reset for a new book.
A drag does not toggle playback. Cover framing is not persisted across launches.

Transport icons are vendored from Phosphor Icons 2.1.1 under the MIT license.
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
`linux-release` is the release configure/build preset. The optional CMake install
target installs the executable and desktop entry; it is not a self-contained bundle.

## Storage and development

The standard Qt application data directory is used (normally
`~/.local/share/qt-m4b-player/qt-m4b-player/`). It contains `library.sqlite` and a
replaceable `covers/` cache, plus `settings.ini` for window size/state and appearance. Debug,
release and installed builds share these settings when using the same data directory.
Saved sizes are bounded by the current screen and the UI's minimum size.
The database keeps multiple locations for a book.
The reference app's `quick:v1` sampled fingerprint algorithm is preserved; its
macOS database is never opened or migrated by this project.

The [architecture and remaining parity work](docs/ARCHITECTURE.md) describe state
ownership, platform boundaries and the next milestones. Tests exercise real
FFmpeg extraction and libmpv playback with the silent output backend; they do
not establish audible quality or real-device Android behavior.

For an additional real book, run the optional read-only integration check:

```bash
QT_M4B_TEST_BOOK=/path/to/local/book.m4b ctest --preset linux-debug
```

Local debug copies can live in `.cache/books/`, which Git ignores. Routine
development should use those local copies; point the optional test at the NAS
path specifically when validating network-share loading/seeking. The developer
inspection command reads metadata without changing the book:

```bash
./build/debug/media-inspect /path/to/book.m4b
./build/debug/media-inspect --captions /path/to/book.m4b
```

The first real NAS book check found 56 chapters and 5,489 timed cues in a
roughly 12-hour M4B. Metadata read took about 0.2 seconds and full caption indexing
about 7.5 seconds on that run. The optional libmpv load/seek/background-index
check passed with silent output and an isolated temporary library. These timings
are observations from one share/cache state, not latency guarantees.
