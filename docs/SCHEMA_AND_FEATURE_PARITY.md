# Database schema and feature parity

Snapshot: **2026-10-12**. macOS reference commit:
`7dd0c79a117bff96cd7faecf5423d50821e49d6c`.

The Qt app is a separate Linux prototype. Its quick fingerprints match the macOS
app, but its SQLite schema is different. Database import, shared database access
and synchronization between the apps are not implemented. The macOS project is
a read-only reference and has not been modified by the port.

Update this document when storage or user-visible features change. See
[architecture](ARCHITECTURE.md) for ownership and platform milestones.

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
unsampled bytes in the middle of a file without changing its length can leave
the fingerprint unchanged.

Qt implementation: [MediaData.cpp](../src/MediaData.cpp). Compatibility checks
include a known fixture fingerprint in [CoreTests.cpp](../tests/CoreTests.cpp).
The reference implementation is `PlayerModel.quickFingerprint(of:)` in
`macos-m4b-player/Sources/m4b-player/PlayerModel.swift`.

## Current Qt storage

The default directory is normally
`~/.local/share/qt-m4b-player/qt-m4b-player/`. `--data-dir DIRECTORY` redirects all
three stores below. Debug, release and installed builds use the same directory
unless overridden.

| Store | Purpose |
| --- | --- |
| `library.sqlite` | Books, listening progress, per-book CC / Stop After Chapter preferences and known file locations. |
| `settings.ini` | Global appearance and desktop window preferences through Qt `QSettings`. |
| `covers/` | Replaceable extracted artwork cache; artwork paths are not stored in the Qt database. |

Chapters and caption cues are read from the audiobook, not stored in SQLite.
Caption indexing runs in the background and currently repeats when a book opens.

### SQLite schema v1

[Library.cpp](../src/Library.cpp) creates these tables transactionally and sets
`PRAGMA user_version = 1`:

```sql
CREATE TABLE books (
    fingerprint TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    author TEXT NOT NULL,
    position REAL NOT NULL DEFAULT 0,
    last_opened INTEGER NOT NULL,
    captions INTEGER,
    stop_after_chapter INTEGER,
    rating INTEGER NOT NULL DEFAULT 0,
    notes TEXT NOT NULL DEFAULT ''
);

CREATE TABLE book_locations (
    path TEXT PRIMARY KEY,
    fingerprint TEXT NOT NULL REFERENCES books(fingerprint),
    last_seen INTEGER NOT NULL
);
```

`position` is absolute book time in seconds. `last_opened` and `last_seen` are
Unix epoch milliseconds. Multiple paths can point to the same book fingerprint;
a reused path can be reassigned to a different fingerprint.

`captions` and `stop_after_chapter` use nullable integer booleans: `NULL` means
unset, `0` disabled and `1` enabled. The current player falls back to disabled
when either preference is unset. Progress saves preserve these preferences and
the reserved rating/notes fields.

**A column does not imply a working feature:**

- `rating` is reserved, with default `0`. Qt has no rating read/write API,
  rating controls or enforced rating scale yet.
- `notes` is reserved, with default empty text. Qt has no notes read/write API
  or editor yet.
- `review` does not exist in the Qt schema.

The Qt library API currently returns listening position and playback preferences,
not ratings or notes. Values inserted through another tool can remain in those
reserved columns, but the Qt app does not display them.

There are no upgrades beyond schema v1. A database reporting a newer
`user_version` is rejected. Automatic backups and recovery for schema upgrades
have not been implemented.

### Differences from the macOS schema

The macOS app uses GRDB over SQLite with named migrations. The reference schema
has integer book/location IDs and a unique book fingerprint that can be null for
legacy books awaiting identification. Qt uses fingerprints and paths as its keys.

| Data | macOS reference | Current Qt app |
| --- | --- | --- |
| Book primary key | Integer `books.id` | `books.fingerprint` |
| Fingerprint | Unique `books.fingerprint`; nullable for legacy rows | `books.fingerprint`; supplied by the player on every saved book |
| Title / author | Nullable text | Required text |
| Listening position | `last_position` | `position` |
| CC preference | `captions_enabled` | `captions` |
| Stop After Chapter | Nullable `stop_after_chapter` | Nullable `stop_after_chapter` |
| Rating | Integer tenths of a star, `0–50`; `0` unrated | Reserved integer; no implemented semantics or UI |
| Notes | Nullable text, with editor and database API | Non-null text, default `''`; storage column only |
| Review | Nullable `review` column and database API | Absent |
| Artwork path | `cover_path` column | External cache; no column |
| Location primary key | Integer `book_locations.id` | `book_locations.path` |
| Location-to-book link | `book_id`, with cascading deletion | `fingerprint`, without cascading deletion |
| File path | Unique `file_path` | Primary-key `path` |
| File attributes | `file_size`, `file_modified_at` | Absent |
| Opened / seen timestamps | GRDB datetime fields | Integer Unix epoch milliseconds |
| Migration tracking | GRDB named migrations; backup before applicable upgrades | `PRAGMA user_version`; initial schema only |

Source of the reference schema:
`macos-m4b-player/Sources/m4b-player/AppDatabase.swift`.

The database files are **not interchangeable**. A future importer must map keys
and column names, decode timestamps, handle nullable metadata and resolve legacy
rows. Compatible fingerprints allow matching already identified books; they do
not automatically translate the remaining data. macOS file paths and artwork
paths may also need replacement on Linux.

### Global preferences outside SQLite

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
| Ratings | No star control, popover, display or persistence API. Preserve the macOS `0–50` tenths representation if rating compatibility is adopted. |
| Notes | No editor, display or persistence API. The macOS editor binds to a logical book ID so saving after switching books updates the correct book; Qt needs an equivalent ownership rule. |
| Review data | No schema field or API. This is a schema gap; the macOS reference's separate notes editor is already a user-visible feature. |
| Precise Stop After Chapter | Qt observes the audio clock, pauses and seeks back to the boundary. Brief next-chapter audio is possible. The macOS engine enforces a playback endpoint before the notification. |
| Independent player windows | Qt has one window/session. Opening another book replaces it; macOS supports independent windows and focused-player coordination. |
| Drag-and-drop opening | No audiobook drop target or drag overlay in Qt. |
| Desktop open routing | A basic desktop entry accepts a file argument, but forwarding opens to an existing process/window is not implemented. |
| More global preferences | No configurable playback-rate list, default Stop After Chapter, auto-resume preference, general player font size or custom foreground/background colors. |
| File identity bookkeeping | Qt does not store file size/modification time or implement the macOS legacy-row identity/duplicate-merge migration behavior. |
| Database interoperability | No macOS import/export, schema alignment or cross-platform synchronization. |
| Schema upgrade protection | No automatic migration backup or tested upgrade/recovery path beyond initial schema creation. |

### Additional platform and validation work

These are port milestones, not all missing macOS user features:

- Linux MPRIS/media-key/desktop playback integration is not implemented.
- Android builds, packaging, document access, background playback service,
  audio focus and headset/lock-screen integration are not implemented. CMake
  currently rejects Android builds explicitly.
- Caption indexes are not cached; indexing a large book can delay CC availability.
- Disconnected/stalled NAS handling and broader codec/edit-list/language coverage
  still need hardening and validation.
- Full keyboard/screen-reader coverage and audible seek, pitch and chapter-cutoff
  behavior still need validation. Silent-engine tests do not establish audio quality.
- Installation uses system libraries and a desktop entry; a self-contained
  distribution package and its library version policy remain future work.

## Follow-up decisions

Before implementing ratings/notes, choose whether to retain Qt's schema with an
explicit importer or align it with the macOS logical-book/location model. Existing
Qt progress and preferences must survive any change. Preserve rating units,
nullable preference semantics and notes ownership deliberately rather than
treating the reserved columns as completed features.

Schema-changing work should include a consistent SQLite backup, a transactional
migration and recovery validation. None of those follow-ups is implemented merely
by adding this document.
