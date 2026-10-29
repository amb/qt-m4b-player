# Architecture and port milestones

## Owners

| Owner | Responsibility |
| --- | --- |
| `MpvEngine` | One audio player; local load, exact seek requests, pause, rate, volume and engine observations. No app-specific chapter policies. |
| `PlayerController` | One listening session, transport policies, load generation, identity, saved resume, chapters, caption timeline and current cue. |
| `MetadataReader` | FFmpeg container inspection and caption indexing on QtConcurrent workers. No live session or database access. |
| `Library` | Qt SQL / SQLite connection, prepared field-specific writes, atomic book/location progress commits and schema version. |
| `WindowPreferences` | Restore desktop window size/maximized state before showing it; save normal size separately from fullscreen/maximized dimensions in QSettings. |
| `AppPreferences` | Global caption text size and cover fill mode, exposed to QML and saved on change in the same QSettings file as window preferences. |
| QML | Responsive presentation, chapter/recent lists, controls, chapter-anchored scrubbing and caption overflow measurement. |
| `FrostedPanel` | Cached CPU Gaussian blur of the cover under the transport panel; crop/framing coordinates match the visible artwork. |

All session and library access occurs on the Qt owner thread. libmpv events are
drained without blocking every 20 ms; there is no native callback holding a Qt
object across its destruction. Metadata jobs receive a shared cancellation flag;
the FFmpeg interrupt callback checks it. Their completions also check the load
generation before changing the current session. A rejected completion cannot
replace a newer book's captions, artwork or identity.

Load, seek and playback property changes use libmpv's asynchronous APIs so NAS
I/O does not run inside a synchronous UI command. Replies are tracked by request
and load generation. The filesystem-mounted NAS needs no custom downloader;
ordinary local paths go through the same engine and reader. Future work includes
stalled/disconnected share handling and indexed-caption caching.

Book opening starts libmpv paused and separately reads the lightweight metadata
and sampled identity. Saved progress is restored only after both results are
ready, unless the user has already performed a transport action. Artwork is
cached separately. Caption indexing begins afterward and does not block playback.
Chapter text exposed by FFmpeg as chapter metadata/data is not treated as CC.
The first preferred-language `mov_text` track is selected, with a first-track
fallback. Parsing preserves sample PTS/duration, UTF-8/UTF-16 text and cue gaps.
Broader multi-language and edit-list compatibility needs real-book validation.

The notes editor captures a fingerprint, title and text when opened. Its explicit
Save updates only that book's `notes` field, even if playback has switched books.
Cancel/window close discards the draft; a failed save leaves the draft open.
Transport shortcuts are disabled while the notes window is visible so typing
spaces/arrows and Ctrl+S cannot control playback.
The transport star opens the shared rating/notes popup. Rating writes update
only the active book's rating in half-star steps; the popup closes on book
replacement and suppresses transport shortcuts while its star slider has focus.
Chapters, rating/notes, speed and volume share `TransportPopup`: the native theme
surface, identical padding, dimming and transitions, and an 8 px gap
above the invoking button. Popup height is bounded by the space above that
button; chapter/speed lists scroll instead of covering it. Qt's popup positioner
clamps horizontal placement, and resizing/reflow updates the available space.
Only one utility popup stays open. Escape/outside press dismisses all four in
the same way, consumes the outside click, and restores trigger focus unless a
separate Notes/EQ window is active. Transport shortcuts are suppressed for every
utility popup, so list/slider keyboard actions cannot seek or pause the book.

EQ settings belong to the playback session and survive book replacement. The
reference first-order tilt coefficients are applied through libmpv's lavfi
wrapper with an explicit 48 kHz resample and double-precision `biquad` section.
The coefficients include output trim of −abs(tone) dB. Boost spans 0–24 dB and
defaults to zero. When Boost is positive, an explicit `scaletempo2` stage follows
the tone section, then a 4×-oversampled lookahead limiter adds gain and protects
peaks. mpv routes speed to that explicit stage instead of its automatic one, so
speed processing cannot create peaks after the limiter. The limiter has a 10 ms
lookahead, 100 ms release with adaptive release enabled, a −1 dB ceiling, automatic
normalization disabled, and latency compensation/EOF flushing enabled. It links
gain reduction across channels to preserve stereo balance. High boost reduces
dynamics; already distorted source audio cannot be repaired. Boost zero retains
the existing tone-only pipeline; neutral tone with zero Boost and bypass remove
all EQ filters. Reset clears Boost and tone, restores 1,000 Hz, and retains bypass.
Volume stays independent and limited to 100 percent.
`LoudnessReader` decodes only the current chapter on a cancellable worker, trims
decoded frames to its exact boundaries, applies the current tone/headroom at
48 kHz, and measures gated BS.1770/EBU R128 integrated loudness through libavfilter.
The chapter must be longer than 60 seconds, and more than 60 seconds of decoded
audio must be available. Silence/unmeasurable input and errors never change gain.
Auto loudness enables EQ and sets `clamp(-18 - measuredLKFS, 0, 24)` dB of Boost;
it leaves volume unchanged. Completion checks both load and measurement generations.
Book/chapter replacement, EQ edits, explicit cancellation and shutdown invalidate
the worker, so an old completion cannot overwrite a newer listening session.
The measured result is cleared when its chapter or EQ settings change. This is
a one-shot correction, not continuous loudness riding or a change to the source file.
Changes are coalesced over 60 ms before replacing the graph, rather than using
the reference's per-sample 10 ms smoothing. Filter command errors disable EQ and
are shown to the user. Audible adjustment quality needs manual validation.

The frosted transport panel samples the same cover image and framing rectangle
as `CoverArtwork`, pads the crop, downsamples to a bounded image, and performs a
separable Gaussian convolution. It caches the result until cover, framing or size
changes. Audio clock updates do not run the blur. A palette tint and rounded clip
are painted afterward. This uses QPainter with either Qt Quick renderer and adds
no shader/effects package. Fit mode uses an opaque palette background.

## Shared behavior

- Store absolute book time; show a chapter-scoped slider.
- Previous/Next pause and restore remembered in-session positions. Chapter-list
  selection seeks to a chapter's start while retaining intended playback state.
- Stop pauses and rewinds to the current chapter start.
- Paused seeks retain the requested position. Playback clock observations only
  advance that position; decoder rounding below a selected chapter's start must
  not select the preceding chapter or activate its Stop After Chapter cutoff.
- Caption intervals are half-open. Scroll from the audio clock, holding each end
  for `min(2 seconds, 20% of cue duration)`. Pauses freeze and seeks reposition it.
- Use the macOS reference's sampled identity (`quick:v1`) across byte-identical
  local copies. This is not a full transfer-integrity checksum.
- Progress writes do not overwrite notes, ratings or playback preference fields.
  Untouched preference fields remain NULL, representing an unset choice.
- Save progress at significant transport actions, replacement, shutdown and
  every 30 seconds of advancing playback.
- Keep the reference UI's narrow/wide responsive layouts and its full menu bar
  when adding or rearranging controls.

Schema v2 is initialized transactionally and stamped with `PRAGMA user_version`
and an `application_id` that identifies our files. A newer schema is rejected,
and so is an unrelated SQLite file. Version 1 databases are upgraded in place
after a timestamped backup. This project's schema is deliberately distinct from
the macOS schema and is not intended to interoperate with it; see
[database schema and feature parity](SCHEMA_AND_FEATURE_PARITY.md) for the schema
details, the robustness guarantees and the implemented/missing features.

## Startup and initialization constraints

These are ordering requirements, not preferences. Each was broken by an
otherwise plausible alternative, so they are easy to reintroduce by accident.

- `main.cpp` sets `LC_NUMERIC` to `C` **after** `QGuiApplication` is constructed
  and before libmpv or any worker exists. Qt adopts the system locale, so setting
  it earlier does not fix libmpv startup under a comma-decimal locale such as
  `fi_FI.UTF-8`. Any test or harness that constructs `PlayerController` directly
  must set it itself; CTest supplies `LC_ALL=C.UTF-8` and a generic platform
  theme/Basic controls style with software rendering for deterministic headless runs.
- `Main.qml` requires **both** the `player` and `preferences` initial properties.
  A new QML harness must supply both.
- The main window starts hidden. Restore size and state, then call `showNormal()`
  or `showMaximized()` explicitly. A plain `show()` loses the restored maximized
  state across an application restart.
- `WindowPreferences` and `AppPreferences` share one INI file but own separate
  keys, so saving window state must preserve appearance values.
- Help and About use the explicitly sized `InfoPopup.qml`. A Qt `Dialog`
  produced a sizing loop.

## Milestones

1. **Implemented Linux prototype:** apt/CMake build, single session, libmpv,
   native FFmpeg extraction, reference fixtures, caption scrolling and saved resume.
2. **Playback hardening:** native chapter audio cutoff, late-load/seek/EOF race
   validation, large-book startup/indexing, codec/chapter/edit-list compatibility
   and audible pitch/seek/cutoff tests.
3. **Product parity on Linux:** settings/defaults,
   multi-window sessions, saved cover framing and drop/open routing;
   manual keyboard/screen-reader validation.
4. **Linux integration/distribution:** self-hosted Flatpak release bundles
   (KDE runtime, mpv built in the manifest), KDE colour-scheme theming and
   focused-session MPRIS.
5. **Android:** Qt Android toolchain, pinned NDK/SDK/Qt builds, Android builds of
   libmpv/FFmpeg, managed file import or durable document access, and a native
   foreground playback service/media session. Add audio focus, output-route
   changes, lock-screen/headset controls, lifecycle tests and device validation.

Android is intentionally rejected by the current CMake configuration so an
unverified APK cannot be mistaken for a supported target. The shared model and
native libraries can carry forward; desktop paths/file opening, Linux MPRIS and
Android storage/services belong to separate platform adapters. The playback
service must outlive the Android UI and own its session. A Qt Activity alone
does not establish dependable background playback.

## Dependencies

Keep apt and CMake as the initial dependency workflow. CMake records exact
configured library versions in the build directory. The version report records
distro revisions and discovered dynamic-library packages. Adopt a separate
package manager only when cross-platform packaging needs justify it. See
[dependency policy](DEPENDENCIES.md).

Release artifacts are self-hosted Flatpak bundles built from
`packaging/flatpak/` against `org.kde.Platform`; see [releasing](RELEASING.md).
The app declares no theme: it selects the KDE Quick Controls style when present
and derives all colours from the platform palette.

All privileged commands (package installation and other system changes) are run
by the developer. Nothing in this repository should invoke `sudo`.
