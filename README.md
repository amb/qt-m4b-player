# qt-m4b-player

A local audiobook player for Linux, built with C++20 and Qt Quick. It uses
libmpv for playback, FFmpeg for metadata and embedded captions, and SQLite for
the library. Playback never modifies audiobook files.

The Linux app is a prototype. Android support and further macOS feature parity
are planned. The macOS app serves as a product and behavior reference; this
project has its own source, database and build setup.

[Install](#install) · [Using the player](#using-the-player) ·
[Known limitations](#known-limitations) · [Build from source](#build-from-source) ·
[Development](#development) · [Documentation](#documentation)

## Features

- **Chapter-aware playback:** chapter-scoped scrubbing, ten-second skips,
  pause-and-remember chapter navigation, and Stop After Chapter.
- **Listening progress:** paused resume, recent locations and saved per-book
  caption and Stop After Chapter choices.
- **Speed and volume:** pitch-preserving playback speed, volume control and
  a speech EQ with protected Boost and automatic chapter loudness correction.
- **Notes and ratings:** per-book plain-text notes and half-star ratings,
  available from the star popup.
- **Embedded captions:** timed `mov_text` captions with automatic overflow
  scrolling and adjustable text size.
- **Desktop interface:** system colour scheme, saved window size, cover art
  and a blurred transport background in Fill mode, including with software rendering.

## Install

### Flatpak (Linux x86_64)

Download `io.github.amb.QtM4bPlayer.flatpak` from
[release v0.2.0](https://github.com/amb/qt-m4b-player/releases/tag/v0.2.0).
With Flatpak installed, run these commands from the download directory:

```bash
flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.kde.Platform//6.10
flatpak install --user ./io.github.amb.QtM4bPlayer.flatpak
flatpak run io.github.amb.QtM4bPlayer
```

The app is distributed through GitHub releases; Flathub supplies the KDE runtime.
Source changes on `main` may be newer than the published release.

### Update an existing installation

Close the player, download the new bundle, then run:

```bash
flatpak install --user --reinstall ./io.github.amb.QtM4bPlayer.flatpak
```

GitHub releases do not provide automatic Flatpak updates.

### Access books outside your home directory

Books in your home directory are accessible by default. Grant access to other
mounts as needed, for example:

```bash
flatpak override --user --filesystem=/mnt/nas io.github.amb.QtM4bPlayer
```

## Using the player

### Open a book and control playback

Use **File → Open Audiobook**, **File → Open Recent**, or Ctrl+O. Without a file
argument, the player opens the last available recent location paused.

The menu bar and lower transport controls provide the playback actions.
**Help → Player Help** lists the shortcuts.

| Shortcut | Action |
| --- | --- |
| Ctrl+O | Open an audiobook |
| Space | Play / pause |
| Left / Right | Skip back / forward ten seconds |
| Ctrl+Left / Ctrl+Right | Previous / next chapter |
| Ctrl+S | Stop and return to the chapter start |
| Ctrl+J | Open the chapter list |
| Ctrl+Shift+C | Toggle captions once indexed |
| Ctrl+N | Open Notes |
| Ctrl+E | Open EQ |
| Ctrl+, | Open Appearance |

Chapters, rating/notes, playback speed and volume open above their buttons with
matching dimming and active-button highlights.

- Escape or an outside click dismisses a popup.
- Up/Down and Enter select a chapter or speed; long lists scroll to fit.
- Rating and volume adjustments keep their popups open. Arrow keys adjust the
  focused control without seeking playback.

### Notes and ratings

Click the **star** for half-star ratings and **Open notes**. While the rating
control has focus, Left/Right changes the rating by half a star; Home clears it.

Open the notes editor from the star popup, **File → Notes**, or Ctrl+N.

- Ctrl+S saves the note; Escape or Cancel discards the draft.
- Saving empty text clears the note.
- Notes follow the book across renamed copies.
- A failed save leaves the editor and draft open.

### Speech EQ and Boost

Open the floating EQ panel with **Playback → Open EQ**, Ctrl+E, or **Open EQ**
below the volume slider.

| Control | Range / behavior |
| --- | --- |
| Tone | Warm/clear tilt from −12 to +12 dB |
| Frequency | Logarithmic balance point from 200 to 4,000 Hz |
| Boost | 0 to +24 dB, protected by a lookahead peak limiter |
| Reset | Neutral tone, 1,000 Hz and zero Boost; preserves bypass |

EQ starts disabled. Settings survive book changes within the current session
but are not saved across launches. Double-click a knob, or press Space while
it has focus, to reset that knob. Escape closes the panel.

Boost raises quiet passages and reduces dynamics at higher settings. It cannot
repair distortion already in a recording. Peak protection follows speed
processing, uses 4× oversampling and has a −1 dB output ceiling.

The response graph shows tone before automatic headroom, Boost and limiting.
See [architecture](docs/ARCHITECTURE.md) for the audio filter details.

### Auto loudness

**Auto loudness: −18 LKFS** measures the current chapter in the background and
sets a fixed Boost amount. It is available for chapters longer than one minute.

- Measurement accounts for the current tone and headroom settings, then enables EQ.
- Quiet chapters are raised toward −18 LKFS; louder chapters need no boost.
- Silence or a failed measurement leaves Boost unchanged.
- Changing the book, chapter or EQ settings cancels an active measurement.
- The +24 dB cap and limiter can keep very quiet or dynamic material below the
  target. The panel reports the result.

This applies a fixed correction rather than adjusting sentence-to-sentence
volume. It changes neither the audiobook file nor the main volume setting.

### Captions, artwork and appearance

- **Captions:** CC becomes available after a usable timed track has been indexed.
  Indexing runs in a cancellable background job; large books can take time.
- **Cover framing:** in Fill mode, drag the cover vertically to reframe it.
  Framing stays within the image edges and survives resizing. It resets for a new
  book and is not saved across launches. Dragging does not toggle playback.
- **Layout:** controls use one row in wide windows and two in narrow windows.
  Chapter and time information sits above the seek bar, wrapping when needed.
- **Preferences:** caption text size and Fill cover background save immediately
  when changed through View or Appearance.
- **Window:** normal size and maximized state are saved between launches.
  The desktop controls window placement, including on KDE Wayland.

The UI follows the desktop colour scheme. Transport icons use the active
palette, including with Qt's software renderer.

## Known limitations

- **Chapter cutoff:** Stop After Chapter observes the boundary, pauses and seeks
  back. Brief next-chapter audio is possible; precise engine-side cutoff is planned.
- **EQ transitions:** live adjustments use coalesced filter updates rather than
  the macOS engine's per-sample coefficient smoothing.
- **Validation:** automated playback tests use silent output. Audible quality,
  full accessibility and real-device Android behavior still need validation.

Other planned work includes:

- Additional appearance preferences, global defaults and configurable playback rates.
- Independent windows, drag/drop and Linux MPRIS integration.
- Library restore/recovery and Android packaging/media services.

See [schema and feature parity](docs/SCHEMA_AND_FEATURE_PARITY.md) for the full
comparison and remaining gaps.

## Build from source

### Dependencies

The tested host is Ubuntu 26.04.1 x86_64. See
[dependency versions](docs/DEPENDENCIES.md) for the exact baseline.

Install development dependencies yourself; repository scripts do not invoke `sudo`.
The native build uses apt libraries, with no Conan, vcpkg or dependency downloads
at configure time.

```bash
sudo apt install build-essential cmake ninja-build pkg-config python3 \
  qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
  libmpv-dev libavformat-dev libavcodec-dev libavutil-dev libavfilter-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-dialogs \
  qml6-module-qtquick-layouts qml6-module-qtqml-workerscript \
  libqt6sql6-sqlite libqt6svg6
```

For KDE Plasma styling, also install `qml6-module-org-kde-breeze` or
`qml6-module-org-kde-qqc2desktopstyle`. Otherwise, the app uses Qt's default style.

### Build and run

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
./build/debug/qt-m4b-player /path/to/book.m4b
```

Use the `linux-release` configure/build preset for an optimized native build.
The optional CMake install target installs the executable and desktop integration;
it uses system libraries. See [releasing](docs/RELEASING.md) for Flatpak bundles.

## Development

### Tests and QML checks

```bash
ctest --preset linux-debug
cmake --build build/debug --target qt-m4b-player_qmllint
```

CTest sets a UTF-8 C locale and an isolated Qt software-rendering configuration.
Tests exercise real FFmpeg extraction and libmpv playback with silent output.
The app sets `LC_NUMERIC` to `C`; see the
[startup constraints](docs/ARCHITECTURE.md) before changing initialization.

For a headless UI smoke check with an isolated test library:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/debug/qt-m4b-player --smoke-test --data-dir /tmp/qt-m4b-test \
  tests/fixtures/captions.m4b
```

### Command-line options

| Option | Purpose |
| --- | --- |
| `--no-resume` | Start with an empty player |
| `--data-dir DIRECTORY` | Isolate the database, cache and settings |
| `--dependency-versions` | Print runtime Qt, mpv engine and SQLite versions |
| `--smoke-test` | Load the UI with silent audio, then exit after one second |
| `--screenshot PATH` | Save a window image during a smoke test |

### Inspect media and test a real book

These commands read metadata without modifying the book:

```bash
./build/debug/media-inspect /path/to/book.m4b
./build/debug/media-inspect --captions /path/to/book.m4b
```

To include an additional book in the read-only integration check, use an absolute
path because CTest runs from the build directory:

```bash
QT_M4B_TEST_BOOK=/absolute/path/to/book.m4b ctest --preset linux-debug
```

Local debug copies can live in the ignored `.cache/books/` directory; reference
UI screenshots live in `.cache/reference/`. Prefer local copies for routine
development and use mounted network paths to validate share loading and seeking.

### Data and settings

The default Qt application data directory is normally
`~/.local/share/qt-m4b-player/qt-m4b-player/`. Override it with `--data-dir`.

| File / directory | Contents |
| --- | --- |
| `library.sqlite` | Books, locations, listening progress, notes, ratings and per-book preferences |
| `settings.ini` | Window size/state and global appearance preferences |
| `covers/` | Replaceable artwork cache |

Debug, release and installed builds share data when using the same directory.
Window sizes are bounded by the screen and the UI's minimum size.

Schema upgrades first write a timestamped backup beside the database.
Unrecognized database files are rejected without modification. The macOS
database is never opened or migrated; only its `quick:v1` book fingerprint
algorithm is shared. See [storage and schema](docs/SCHEMA_AND_FEATURE_PARITY.md).

## Documentation

- [Architecture](docs/ARCHITECTURE.md): state ownership, playback rules and startup constraints.
- [Schema and feature parity](docs/SCHEMA_AND_FEATURE_PARITY.md): storage, behavior comparison and planned work.
- [Dependencies](docs/DEPENDENCIES.md): native and Flatpak dependency baselines.
- [Releasing](docs/RELEASING.md): build, test and publish Flatpak bundles.
- [Application icon](packaging/icons/README.md): macOS artwork source and transparent Linux exports.
- [Transport icons](assets/icons/README.md): vendored Phosphor Icons 2.1.1 provenance.

## License

MIT; see [LICENSE](LICENSE). The vendored Phosphor icons are also MIT licensed.
No icon font or icon download is needed at build time or runtime.
