# Database schema and feature parity

Snapshot: **2026-10-16**.

This document describes the Qt/Linux library format, its robustness guarantees
and its relationship to the macOS reference app. Update it whenever storage
changes. See [architecture](ARCHITECTURE.md) for ownership and platform
milestones.

## Scope decision: the Qt schema is its own design

The Qt app stores a **single-platform, Qt-native** database. It does **not**
attempt macOS/GRDB compatibility. There is no importer, exporter, shared
database mode or synchronization, and the macOS database is never opened or
migrated by this project.

An earlier draft of this document proposed aligning the columns with the macOS
schema. That was rejected: without a defined compatibility layer, a source of
truth and a shared platform, mimicking GRDB's schema buys no interoperability
while forcing a surrogate-key model that is larger and more awkward than the
fingerprint-keyed design below. The comparison was made against commit
`7dd0c79a117bff96cd7faecf5423d50821e49d6c` of `macos-m4b-player`.

The only intentional overlap with macOS is the **identity hash** below, which is
kept because it is already useful on its own: it lets byte-identical local and
NAS copies of a book share progress.

Divergences from the macOS schema are therefore deliberate. Do not "fix" them
without a concrete interoperability requirement:

| Data | macOS reference | Qt/Linux |
| --- | --- | --- |
| Book key | Integer `books.id` surrogate | `books.fingerprint` natural key |
| Location key | Integer `book_locations.id` | `book_locations.path` |
| Location link | `book_id` | `fingerprint` |
| Listening position | `last_position` | `position` |
| CC preference | `captions_enabled` | `captions` |
| Review | `review` column | Not modelled; no review concept |
| Artwork | `cover_path` column | External `covers/` cache, keyed by fingerprint |
| File attributes | `file_size`, `file_modified_at` | Not stored; identity is the fingerprint |
| Timestamps | GRDB datetime text | Integer Unix epoch milliseconds |
| Migration tracking | GRDB named migrations | `PRAGMA user_version` + `application_id` |

## Book identity: compatible quick fingerprints

Both apps produce `quick:v1:<lowercase SHA-256 digest>` from the same ordered input:

1. UTF-8 bytes of `m4b-quick-fingerprint-v1` followed by a null byte.
2. File size encoded as an unsigned 64-bit big-endian integer.
3. The first 256 KiB of the file, or the whole file if shorter.
4. The last 256 KiB of the file, or the whole file if shorter.

Small files are intentionally sampled twice. At most 512 KiB of file contents is
read for hashing. Filenames, directories and modification times are not hashed,
so byte-identical NAS and local copies have the same identity.

This is a sampled identity, not a full-file integrity checksum. Changing only
unsampled bytes in the middle of a file without changing its length can leave the
fingerprint unchanged.

Qt implementation: [MediaData.cpp](../src/MediaData.cpp). Compatibility checks
include a known fixture fingerprint in [CoreTests.cpp](../tests/CoreTests.cpp).

## Storage locations

The default directory is normally
`~/.local/share/qt-m4b-player/qt-m4b-player/`. `--data-dir DIRECTORY` redirects all
three stores below. Debug, release and installed builds use the same directory
unless overridden.

| Store | Purpose |
| --- | --- |
| `library.sqlite` | Books, listening progress, per-book CC / Stop After Chapter preferences and known file locations. |
| `settings.ini` | Global appearance and desktop window preferences through Qt `QSettings`. |
| `covers/` | Replaceable extracted artwork cache; artwork paths are not stored in the database. |

Chapters and caption cues are read from the audiobook, not stored in SQLite.
Caption indexing runs in the background and currently repeats when a book opens.

## SQLite schema v2

[Library.cpp](../src/Library.cpp) creates these tables transactionally and stamps
`PRAGMA application_id = 0x4D344251` ("M4BQ") and `PRAGMA user_version = 2`:

```sql
CREATE TABLE books (
    fingerprint        TEXT PRIMARY KEY,
    title              TEXT,
    author             TEXT,
    position           REAL NOT NULL DEFAULT 0.0,
    rating             INTEGER NOT NULL DEFAULT 0 CHECK (rating BETWEEN 0 AND 50),
    notes              TEXT,
    captions           INTEGER CHECK (captions IN (0, 1)),
    stop_after_chapter INTEGER CHECK (stop_after_chapter IN (0, 1)),
    last_opened        INTEGER NOT NULL
) STRICT;

CREATE TABLE book_locations (
    path        TEXT PRIMARY KEY,
    fingerprint TEXT NOT NULL REFERENCES books(fingerprint) ON DELETE CASCADE,
    last_seen   INTEGER NOT NULL
) STRICT;

CREATE INDEX books_last_opened          ON books(last_opened DESC);
CREATE INDEX book_locations_fingerprint ON book_locations(fingerprint);
```

`position` is absolute book time in seconds. `last_opened` and `last_seen` are
Unix epoch milliseconds. Multiple paths can point to the same book fingerprint;
a reused path can be reassigned to a different fingerprint.

`captions` and `stop_after_chapter` use nullable integer booleans: `NULL` means
unset, `0` disabled and `1` enabled. The current player falls back to disabled
when either preference is unset, and the NULL state is retained so a future
global default can be distinguished from an explicit per-book "off".

`title`, `author` and `notes` are nullable. Missing metadata is stored as `NULL`
and read back as an empty string. Absent metadata never overwrites metadata
recorded earlier: re-opening a book whose tags cannot be read keeps the known
title and author. Progress saves preserve the preference fields.

**A column does not imply a working feature:** `rating` and `notes` are reserved.
Qt has no rating or notes read/write API, no editor and no enforced rating scale.
Values inserted through another tool survive normal progress and preference
saves, but the app does not display them.

### Deliberate omissions

- **`position` has no `CHECK` constraint.** It is taken from the playback clock,
  and a transient negative reading must not turn into a failed save.
- **No `file_size` or `file_modified_at`.** The sampled fingerprint already
  answers "is this the same book", and the app re-reads metadata on open anyway.
- **`WITHOUT ROWID` is not used.** It would slightly shrink the TEXT-keyed
  tables, but the row counts are tiny and the cost in clarity is not worth it.

### Robustness properties

| Property | How it is provided |
| --- | --- |
| Type safety | `STRICT` tables reject a value of the wrong storage class |
| Range safety | `CHECK` on `rating`, `captions` and `stop_after_chapter` |
| Referential integrity | `PRAGMA foreign_keys = ON` on every connection, `ON DELETE CASCADE` from locations to books |
| Query performance | Indexes behind the recents ordering and the location lookup |
| Concurrency tolerance | `PRAGMA busy_timeout = 5000`; transactional progress writes |
| Crash resilience | `journal_mode = WAL` with `synchronous = NORMAL`, which is corruption-safe; only the most recent transaction can be lost on a hard reset |
| File identification | `application_id` rejects unrelated SQLite files before any write |
| Upgrade safety | Timestamped backup before any migration; one transaction per step |

`WAL` creates `library.sqlite-wal` and `library.sqlite-shm` while the app runs.
Copying only `library.sqlite` while the app is running can miss recent writes;
`WAL` is also unsuitable if the data directory is placed on a network share.

### Schema versions and migrations

| Version | Changes |
| --- | --- |
| 1 | Original prototype: non-`STRICT` tables, required `title`/`author`, `position`/`captions` column names, no cascading delete, no indexes. |
| 2 | Current: `STRICT`, nullable metadata, `ON DELETE CASCADE`, indexes, `application_id`, WAL. |

Opening a version 1 database upgrades it in place: `STRICT` and relaxed
constraints cannot be expressed with `ALTER TABLE`, so both tables are rebuilt
inside one transaction, preserving position, rating, notes, captions,
stop-after-chapter and all locations. Empty strings in `title`, `author` and
`notes` become `NULL`. The migration:

1. Copies `library.sqlite` to
   `library.sqlite.backup-before-migration-<yyyyMMdd-HHmmss>` and aborts if the
   copy fails.
2. Rebuilds both tables with `PRAGMA foreign_keys = OFF`, which is required —
   dropping `books` with foreign keys enabled would cascade the locations away.
3. Runs `PRAGMA foreign_key_check` before committing, and rolls back on any
   violation.
4. Restores `foreign_keys = ON` afterwards.

A database reporting a `user_version` newer than the supported version is
rejected. A database with a foreign `application_id` is rejected without being
modified. A version 1 file is only accepted as legacy if it actually contains
both expected tables.

There is no automated recovery beyond the pre-migration backup, and no
downgrade path.

## Global preferences outside SQLite

macOS uses SwiftUI `@AppStorage` backed by `UserDefaults`. Qt uses
[AppPreferences](../src/AppPreferences.cpp) and
[WindowPreferences](../src/WindowPreferences.cpp) with `settings.ini`:

| Qt key | Current behavior |
| --- | --- |
| `appearance/captionSize` | Caption size in pixels, `18–42`, default `26`; saved when changed. |
| `appearance/fillCover` | Fill versus Fit cover mode, default Fill; saved when changed. |
| `window/width`, `window/height` | Normal window size; saved on orderly exit and bounded by the current screen/UI minimum when restored. |
| `window/maximized` | Maximized-state persistence; minimized/fullscreen states do not become launch modes. |

Caption size changes through either View or Appearance use the same saved value.
CC enabled and Stop After Chapter remain per-book SQLite preferences.
Window placement belongs to the desktop/window manager. Cover drag position,
playback speed and volume are not persisted across app launches.

## Recent books and fallback paths

`recentBooks()` returns **one entry per logical book**, at its most recently seen
location, ordered by `last_opened` descending. A book known under two paths (for
example a local copy and a NAS copy) does not appear twice in the menu.

`lastAvailablePath()` walks every known location of the most recently opened
books and returns the first one that still exists, so a deleted copy falls back
to another path of the same book.

## Implemented Qt features

- Local file opening through the command line, file chooser and recent locations;
  filesystem-mounted NAS paths use the same playback path.
- Paused listening-position resume, play/pause, Stop, ten-second skips and
  pitch-preserving playback speed changes.
- Chapter selection and chapter-scoped scrubbing. Previous/Next pause and remember
  chapter positions within the current session.
- Embedded artwork, Fill/Fit cover mode and vertical cover dragging in Fill mode.
- Embedded `mov_text` captions, cue timing, automatic overflow scrolling and
  persistent caption text size.
- Saved per-book CC and Stop After Chapter choices, plus a working chapter-end
  pause with the cutoff limitation below.
- File/Playback/View/Help menus, shortcuts and responsive narrow/wide controls,
  including the centered elapsed-time readout.
- Saved normal window size and maximized state, verified on KDE Wayland.

## Missing or incomplete Qt features

| Feature | Current gap |
| --- | --- |
| Ratings | No star control, popover, display or persistence API. The column is reserved. |
| Notes | No editor, display or persistence API. The column is reserved; an editor must bind to a logical book, not to whatever is currently playing. |
| Precise Stop After Chapter | Qt observes the audio clock, pauses and seeks back to the boundary. Brief next-chapter audio is possible. The macOS engine enforces a playback endpoint before the notification. |
| Independent player windows | Qt has one window/session. Opening another book replaces it; macOS supports independent windows and focused-player coordination. |
| Drag-and-drop opening | No audiobook drop target or drag overlay in Qt. |
| Desktop open routing | A basic desktop entry accepts a file argument, but forwarding opens to an existing process/window is not implemented. |
| More global preferences | No configurable playback-rate list, default Stop After Chapter, auto-resume preference, general player font size or custom foreground/background colors. |
| Caption index cache | Caption indexes are not cached; indexing a large book can delay CC availability. |
| Database recovery | Only the automatic pre-migration backup exists; there is no restore UI or downgrade path. |
| macOS interoperability | Explicitly out of scope, by the scope decision above. |

### Additional platform and validation work

These are port milestones, not missing macOS user features:

- Linux MPRIS/media-key/desktop playback integration is not implemented.
- Android builds, packaging, document access, background playback service,
  audio focus and headset/lock-screen integration are not implemented. CMake
  currently rejects Android builds explicitly.
- Disconnected/stalled NAS handling and broader codec/edit-list/language coverage
  still need hardening and validation.
- Full keyboard/screen-reader coverage and audible seek, pitch and chapter-cutoff
  behavior still need validation. Silent-engine tests do not establish audio quality.
- Installation uses system libraries and a desktop entry; a self-contained
  distribution package and its library version policy remain future work.

## Follow-up decisions

The storage shape is settled: Qt-native, `STRICT`, fingerprint-keyed, with
reserved rating and notes columns. Open items are behavioural, not schema-level:

- Ratings and notes need APIs and UI that bind to a logical book, and the rating
  scale (`0–50` tenths of a star, `0` unrated) should be enforced in code if the
  reserve is ever populated.
- If a global "default Stop After Chapter" or "default CC" preference is added,
  the existing `NULL` versus `0` distinction already supports inheriting it.
