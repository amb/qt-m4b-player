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
  must set it itself; this is why `ctest` needs `LC_ALL=C` on such a host.
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
3. **Product parity on Linux:** notes, ratings, settings/defaults,
   multi-window sessions, saved cover framing and drop/open routing;
   manual keyboard/screen-reader validation.
4. **Linux integration/distribution:** focused-session MPRIS, desktop packaging
   and a deliberate dynamic-library distribution/version policy.
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

All privileged commands (package installation and other system changes) are run
by the developer. Nothing in this repository should invoke `sudo`.
