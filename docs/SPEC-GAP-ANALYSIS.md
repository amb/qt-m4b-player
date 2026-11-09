# Gap analysis: qt-m4b-player vs. the macOS UI specification

Snapshot: **2026-11-09**. Updated the same day: Audiobook Info, WAV export and the menu reorganization are implemented. Compared:

- **Spec:** `../m4b-player-ui-spec/` (macOS UI spec, source revision `9c1ca04`,
  reviewed 2026-11-07): `01-player.md` … `05-states-and-interactions.md`.
- **Qt app:** `qt-m4b-player` at commit `dcda72b` (`src/`, `qml/`, `docs/`).

This is a read-only review of the source code, plus two offscreen smoke-test screenshots
(`--smoke-test --screenshot`, software renderer, isolated `--data-dir`) of the
empty player and `tests/fixtures/captions.m4b`. No code was changed.
The screenshots confirm: "0h 0m remaining" formatting, Previous disabled at the
first chapter, a blank cover area for a book without artwork, a single volume icon,
the "Your next chapter" empty state, and the fixed one-/two-row control split. Spec element IDs
(P01, W04, S06, M02c, K02 …) are used so each gap can be traced back to the spec.
Where the Qt app already documents a gap in
[ARCHITECTURE.md § Known gaps](ARCHITECTURE.md#known-gaps-and-milestones), this
file says so. Items marked **NEW** are not listed there.

Legend:

- **Missing**: there is no Qt equivalent.
- **Partial**: the feature exists but is missing part of the spec's behavior.
- **Diverges**: the feature exists but behaves differently from the spec. Some
  differences may be deliberate Linux adaptations. Each one is listed so a
  decision can be recorded.
- **Qt-only**: the Qt app has it and the spec does not. Listed for completeness.

---

## 1. Summary: largest missing features

| # | Feature | Spec | Status | Already tracked? |
| --- | --- | --- | --- | --- |
| 1 | Audiobook Info sheet (File → Info…, Ctrl+I) | W05, M02 | ✅ **Implemented** (see 2.16) | — |
| 2 | WAV export: current chapter and current caption chunk | M02c–M02e, I02 | ✅ **Implemented** (see 2.21) | — |
| 3 | Full Settings window: resume-at-launch, Stop After Chapter default, player text size, subtitle transparency, custom colors, preview, Reset Appearance | S01–S10 | ✅ **Implemented** except custom colors and Reset (not wanted) | — |
| 4 | Multiple independent player windows | P01, I06, M06 | Missing | Yes |
| 5 | Drag-and-drop opening with drop overlay | P20 | Missing | Yes |
| 6 | System media controls / Now Playing (MPRIS on Linux) | K02 | Missing | Yes (milestone 4) |
| 7 | Per-book persistence of speed and EQ (switch, tone, Hz) | P16, W06e, I05 | Missing | **NEW** |
| 8 | Editable speed field: typed value, 0.01× scroll steps, ±0.1× shortcuts | P16, M05 | Diverges (preset list) | Partly ("configurable playback-rate list") |
| 9 | Play at book end must not restart. Saved exact endpoint must not be restored | P08, P19, I01 | Diverges | **NEW** |
| 10 | Open Recent: fall back to other known locations; Clear Menu; 10-entry cap | M02b | Partial | **NEW** |
| 11 | Engine-precise Stop After Chapter cutoff | P13 | Partial | Yes |
| 12 | Desktop "Open With" routing to a running instance | P01, M02a | Missing | Yes |

Section 2 lists each spec area in detail.

---

## 2. Detailed comparison by spec area

### 2.1 Player window (P01)

| Spec behavior | Qt implementation | Status |
| --- | --- | --- |
| Each window owns an independent book and session. Finder/Open With opens a new window per file. Focus routes menu and media commands | One `PlayerController`, one `ApplicationWindow` (`main.cpp`) | **Missing** (tracked) |
| Initial 440 × 652, minimum 400 × 620 | 520 × 800, minimum 390 × 600 (`Main.qml:19-22`). Size and maximized state restored from `settings.ini` | Diverges (minor) |
| Additional windows cascade from the previous one | N/A (single window) | Missing |
| Title is "Title — Author". Before metadata it can show the filename | Title is "Title — Author" only once `loaded`. Otherwise "qt-m4b-player" (`Main.qml:25`) | Partial: the filename does not appear in the title while loading |
| Closing the last window quits. Closing a player flushes progress | `aboutToQuit` → `shutdown()` → `save()` | OK |

### 2.2 Cover and background surface (P02)

| Spec | Qt | Status |
| --- | --- | --- |
| Click on cover toggles Play/Pause. Disabled without a usable book | `CoverArtwork.qml` MouseArea, `playbackEnabled: player.loaded` | OK |
| Accessible name changes "Play audiobook" / "Pause audiobook" | Static "Artwork: play or pause" | Partial |
| Fit / Fill. Fill uses translucent blurred controls | Implemented (`FrostedPanel`) | OK |
| **Default is Fit** | Default is **Fill** (`AppPreferences.h:25`) | **Diverges NEW** |
| Fill: an 8 pt drag reframes vertical excess, clamped. Resets on new load | Implemented (8 px threshold, normalized position, resets on source change) | OK |
| Book-specific play behavior at the finished state (see 2.12) | Cover click calls `togglePlayback()`, which restarts at the end | Diverges (see 2.12) |

### 2.3 Captions area (P03)

| Spec | Qt | Status |
| --- | --- | --- |
| Whole caption bubble is a Play/Pause button. Accessibility value is the cue text | MouseArea on bubble toggles playback. Text has `Accessible.name` | OK / Partial (not exposed as a button with a value) |
| With CC on, the region **reserves 224 pt** even for short or absent cues, so layout does not jump | Bubble height = `min(text + 36, 43% of window)`. It collapses to 0 when there is no cue (`Main.qml:378-379`) | **Missing NEW** |
| Taller windows give taller cues more space. Captions never overlap controls | 43% cap. Captions are in the same column as the controls, so there is no overlap | Partial |
| Overflow auto-scrolls on audio time, with holds of `min(2 s, 20%)` | Implemented (`CaptionCue::scrollFraction`) | OK |
| Text size **12–108 pt, default 18** | **18–42 px, default 26**. The View menu offers 7 presets and Appearance has a slider | **Diverges NEW** |
| Subtitle background transparency 0–100% (100% removes panel and shadow) | Fixed opaque black `#000000` | **Missing** |
| Text always has a black outline | No outline | **Missing NEW** |
| Custom caption colors with a 4.5:1 contrast check, falling back to white on black | Fixed white on black | **Missing** |
| No manual caption scrolling | No manual scrolling | OK |
| Only `mov_text`, preferred-language order, no track chooser | Same (`ARCHITECTURE.md`) | OK |

### 2.4 Progress header (P04)

| Spec | Qt | Status |
| --- | --- | --- |
| Chapter title, elapsed/total, book remaining. Single row centered, or two rows | `ProgressHeader.qml` implements both layouts | OK |
| Remaining: "Xh Ym". **Minutes alone below an hour; seconds below a minute.** Percentage **rounded** | Always `Hh Mm` (shows "0h 12m"). Percentage uses `Math.ceil` (`Main.qml:431-432`) | **Diverges NEW** |
| Monospaced digits for elapsed/total | Proportional font | Partial NEW |
| Rounded seconds | `Math.floor` (`formatTime`) | Minor divergence |
| Player text size preference scales these readouts | Fixed pixel sizes 16/14 | Missing (see S04) |

### 2.5 Chapter slider (P05)

| Spec | Qt | Status |
| --- | --- | --- |
| Seeks within the current chapter. Range is frozen during drag. End stays one tick inside | Implemented (`capturedStart/End`, `capturedEnd - 1e-6`) | OK |
| Accessibility value "elapsed of total" | Name only ("Chapter progress") | Partial |
| Drag suspends audio but keeps **play intent**. Pause during drag cancels resume | Pauses through `togglePlayback()`, then toggles again on release if it was playing. There is no separate intent state, so e.g. pressing Space mid-drag is not reconciled. Each toggle also writes a save | Partial |
| Approximate seeks during drag are allowed | No seeks during drag; only on release | OK (allowed) |

### 2.6 Transport controls (P06–P11) and keyboard (M05, K01)

| Spec | Qt | Status |
| --- | --- | --- |
| Previous/Next **pause**, use remembered in-session position, one step per click | Implemented (`navigateChapter`) | OK |
| Previous/Next **stay enabled** at first/last chapter and **pause without moving** | Same | ✅ |
| Remembered position at or past chapter end → go to start | Clamped to `nextafter(end, start)`, so it can return to just before the end | Diverges (minor) NEW |
| Back/Forward 10 s keep play intent, clamped | Implemented | OK |
| Play at exact book end does nothing | **Seeks to 0 and plays** (`PlayerController.cpp` `togglePlayback`) | **Diverges NEW** |
| Pause clears an active chapter cutoff | The cutoff is evaluated from the clock, so there is nothing to clear | N/A |
| Stop = chapter start + pause, falling back to book start | Implemented | OK |
| Hover tooltips after ~1 s | `ToolTip.visible: hovered` with no delay set (Qt default) | Minor |
| Sizes 40 pt / 46 pt central, 6 pt gap | 48 px / 56 px, 4 px gap | Cosmetic divergence |
| **Bare keys:** Space play, **Esc stop**, **↑/↓ chapters**, ←/→ skip | Same | ✅ |
| **Cmd+←/→ = speed ±0.1×** | Ctrl+←/→, clamped to the 0.5–3× presets | ✅ |
| Cmd+↑/↓ = volume ±5 | Ctrl+↑/↓ | OK |
| Bare keys are suppressed while popovers or editors own focus | `enabled: !notesEditor.visible && !utilityPopupOpen` | OK |

### 2.7 Chapters button and list (P12, W01)

| Spec | Qt | Status |
| --- | --- | --- |
| Opens the list; disabled without a book | Ctrl+J action; enabled when `chapters.length > 0` | OK (Qt adds a Ctrl+J shortcut) |
| Rows show **chapter duration at right**, checkmark slot, semibold current row | Rows show a "▶" prefix and title only. **No duration column** | **Partial NEW** |
| Row accessibility: title + duration + "Current chapter" | Default ItemDelegate text | Partial |
| Select → seek to start, keep play intent, close | Implemented | OK |
| "Whole book" for unchaptered books | `MetadataReader.cpp:153` synthesizes one | OK |
| 540 × 320 list | 420 wide, ≤ 440 tall, bounded by space above the button | Cosmetic |

### 2.8 Stop After Chapter (P13, S02)

| Spec | Qt | Status |
| --- | --- | --- |
| Engine stops at the next chapter start before audio continues | Observe, pause, seek back (brief overrun possible) | **Partial** (tracked) |
| Play then continues the next chapter | Seeks to `boundary` (= next chapter start), so Play continues from there | OK |
| Per-book saved choice | `stop_after_chapter` column | OK |
| **Global default for books without a saved choice** (S02) | Hard-coded `false` (`value_or(false)`). The NULL state is already preserved for this | **Missing** (tracked) |
| Turning it off right after the engine stopped resumes from the next chapter | Not applicable to the observe-based design | N/A until engine cutoff exists |
| Accessible On/Off value, filled/outlined icon | Checkable button highlight. Single icon | Partial |

### 2.9 CC button (P14)

| Spec | Qt | Status |
| --- | --- | --- |
| Disabled with no supported non-empty track | `enabled: captionsAvailable` | OK |
| Tooltip "Embedded captions: On/Off" / "No embedded captions" | "Embedded captions" / "Indexing captions" | Partial |
| Per-book choice, default Off | Implemented | OK |
| Async discovery | Implemented. Index not cached (tracked) | OK / tracked |

### 2.10 Rating and notes (P15, W02, W04)

| Spec | Qt | Status |
| --- | --- | --- |
| Star button: outline when unrated, filled when rated. Tooltip shows rating | Implemented | OK |
| Popover: 5 stars; click left/right half of a star sets a half or whole star; **click at the far left clears** | Slider with 0.5 steps drawn as stars. Home clears; End sets 5. Click-to-set happens through slider movement. There is no far-left click-to-clear | Partial |
| Filled stars turn gold on hover | Highlight color when focused | Cosmetic |
| "Open notes" disabled during identity lookup; failure alert "Unable to Open Notes" | Synchronous; failure goes to the error banner | Partial |
| Notes as a **sheet**, 520 × 460, min 520 × 380 | Separate window-modal dialog, 560 × 460, min 390 × 320 | Diverges (acceptable on Linux) |
| **Resize grip; dimensions saved globally** for future editors | Normal window resize; **size not persisted** | **Missing NEW** |
| Heading ≥ 22 pt, title up to **2 lines** | Heading 24 px; title elided to 1 line | Minor |
| Footer text "Saved with this audiobook in your library." | "Saved with this audiobook." | Cosmetic |
| **Saving… state** disables editor/Save/Cancel | Save is synchronous; there is no Saving state | Partial (fine if saves stay synchronous) |
| Failure: **red** inline error, focus returns to editor | Plain label, no red color, focus is not restored | Partial NEW |
| Esc/Cancel discard without confirmation | Implemented | OK |
| Notes and Info cannot open over each other | N/A (no Info) | — |
| Text uses player text-size preference | Fixed | Missing (S04) |

### 2.11 Speed (P16)

| Spec | Qt | Status |
| --- | --- | --- |
| **Editable text field** (type "1.25x", Enter or focus loss applies) | Button opening a **preset list** (0.5, 0.75 … 3×) plus a Playback → Speed submenu | **Diverges NEW** |
| Range **0.5–2.0×**, stored in 0.01 steps | Presets up to **3×**. `setSpeed` accepts 0.25–4 | **Diverges NEW** |
| Scroll wheel ±0.01× with trackpad accumulation, temporarily suspends audio, resumes after gesture or 200 ms | Missing | **Missing NEW** |
| Cmd+←/→ ±0.1× | Missing | **Missing NEW** |
| Invalid text reverts | N/A | — |
| **Saved per book**, default 1× | **Not persisted**; no DB column. Resets to 1× with each new mpv load | **Missing NEW** |
| Tooltip/accessibility explains range | "Playback speed" | Partial |

### 2.12 Volume (P17, W03)

| Spec | Qt | Status |
| --- | --- | --- |
| Available without a book | `volumeAction` always enabled | OK |
| Icon changes at 0 / ⅓ / ⅔ (slashed at zero) | Single `volume` icon | **Missing NEW** |
| Tooltip with percentage; accessible "Volume and EQ" + % | "Volume" | Partial |
| 0–100%, ±5 shortcuts, session-only, not system volume | Implemented | OK |
| Open EQ link closes popover, opens or raises the single panel | Implemented | OK |
| Quiet/loud speaker symbols around the slider | Not present | Cosmetic |

### 2.13 Adaptive layout and appearance (P18)

| Spec | Qt | Status |
| --- | --- | --- |
| One-row vs two-row split chosen from **measured** control widths | Fixed breakpoint `width >= 740` (`Main.qml:13`) | Diverges NEW |
| Vertical divider between transport and utilities in one-row mode | None | Cosmetic |
| Global appearance (Fit/Fill, text sizes, transparency, colors) updates live | Only Fit/Fill and caption size exist | Partial (see Settings) |
| System dark appearance | Follows the KDE/Qt palette | OK |

### 2.14 Empty, loading, no-artwork, finished states (P19, I01)

| Spec | Qt | Status |
| --- | --- | --- |
| Empty: musical-note placeholder, title "No audiobook loaded" | Text "Your next chapter" with a hint and no icon. Window title "qt-m4b-player" | Diverges (cosmetic) |
| Loading: "Loading audiobook…" **with progress indicator** | "Opening audiobook…" label, **no BusyIndicator** | Partial NEW |
| No artwork: placeholder while loaded | Nothing drawn (window background) | Partial NEW |
| **Finished**: Play, cover and caption clicks **do nothing** at book end | Play **restarts from 0** | **Diverges NEW** |
| **Reopen restores only interior positions**; a saved exact endpoint opens at 0 | `applyResume` clamps to `[0, duration]`, so it restores the exact end. Combined with the above, Play then restarts | Diverges NEW (note: the spec flags its own reopen policy as "currently", so confirm the intent) |
| Late metadata must not undo user seeks or preferences | `m_positionTouched`, `m_captionsTouched`, `m_stopTouched` | OK |

### 2.15 Drag-and-drop (P20)

No `DropArea` anywhere in QML. The overlay ("Drop audiobook to open"), the
0.15 s animation and same-window loading are all **missing** (tracked).

### 2.16 Audiobook Info sheet (W05): ✅ Implemented

Implemented as `qml/BookInfoWindow.qml` (a window-modal dialog, as Linux has no
sheets) backed by `src/AudiobookInfo.cpp`. Remaining differences: it is a
separate dialog window rather than an attached sheet. File "kind" comes from the
shared MIME database, and "Show in File Manager" uses
`org.freedesktop.FileManager1`, falling back to opening the folder. The original spec items were:

- File → **Info…**, **Ctrl+I**. Disabled without a book or while Notes is open.
- **Book:** title, author/"Unknown", duration, chapter count, live-updating.
- **File:** filename, full path, kind, size (human and exact bytes), created and
  modified dates.
- **Audio:** per track: codec, average bitrate (kb/s), sample rate (kHz),
  channels (Mono/Stereo).
- **Subtitles:** count/"None"; per track ID, language and format; selected track
  and its On/Off state; explanation when no usable track exists.
- **Copy Path**, **Show in File Manager** (Linux equivalent of Show in Finder,
  e.g. `org.freedesktop.FileManager1.ShowItems` over D-Bus or `xdg-open` on the folder),
  **Done**/Return, Esc.
- Async loading placeholders ("Loading file details…" etc.) and inline errors.
- Read-only, selectable text; does not pause playback.

`MetadataReader` already opens the container with FFmpeg and could supply the stream details.

### 2.17 EQ panel (W06)

| Spec | Qt | Status |
| --- | --- | --- |
| One reusable floating panel per player, closes with player, Esc closes | `EqualizerWindow.qml` (`Qt.Tool`), closed in `onClosing` | OK |
| Hides when the app deactivates | Not handled explicitly (window-manager dependent) | Minor |
| Heading "Speech tone" + book title or "Applies to this player window" | "Speech tone" only | Partial |
| Switch starts Off; bypass keeps knob values | Implemented | OK |
| Graph: log axis, refs 100 Hz/1 kHz/10 kHz, gain guides ±12/0, pivot dot | Log 20 Hz–20 kHz curve with −12/0/+12 dB guides. **No 100 Hz/1 kHz/10 kHz reference lines and no pivot dot** (`EqualizerWindow.qml:44-62`). Curve stays highlight-colored rather than gray in bypass (verify) | Partial NEW |
| Tone and Hz knobs: drag mapping, center snap, arrows 1%, double-click/Space reset | Dials; README says double-click/Space reset is implemented | OK (check center snap and 1% step) |
| Tone label "6.0 dB clearer" / "Neutral" within 0.05 | "Tone N.N dB" + "Warm ↔ Clear" | Partial |
| Hz label: whole Hz < 1 kHz, two-decimal kHz above; endpoint labels 200/4k | Always "N Hz" | Partial |
| Status "Bypassed · original audio" / "Auto headroom −N.N dB" | "Auto headroom −N.N dB" / **"Bypassed"** (`EqualizerWindow.qml:194`) | Minor wording |
| Reset keeps enabled state | Implemented | OK |
| **EQ (switch, tone, Hz) saved per book** and restored | **Session-only, carried across books** (deliberate per README, but contradicts the spec) | **Diverges NEW** |
| Format failure disables the switch and shows the reason under the controls | `equalizerFailed` → disable + error banner | Partial (shown in the main banner, not the panel) |
| Per-sample 10 ms coefficient smoothing | 60 ms coalesced graph replacement | Partial (tracked) |
| **Boost 0–24 dB + limiter, Auto loudness −18 LKFS** | Present | **Qt-only** (extension; keep) |

### 2.18 Help (W07, M07)

| Spec | Qt | Status |
| --- | --- | --- |
| Reusable resizable Help **window**, 560 × 680, header with icon/tagline, shortcut table + sections | `InfoPopup` with a single plain-text message | Partial |
| Shortcut ⇧⌘H | F1 (Linux convention) | Diverges (acceptable) |
| About: system panel | Custom About popup with version and build time | OK |

### 2.19 Settings (S01–S10): mostly **Missing**

Qt's **Appearance** popup (Ctrl+,) contains only *Fill cover background* and a
caption size slider. Compared with the spec's Settings:

| ID | Setting | Spec default / range | Qt |
| --- | --- | --- | --- |
| S01 | Resume most recent book at launch (loads paused) | On | ✅ |
| S02 | Stop After Chapter default for books without saved choice | Off | ✅ |
| S03 | Cover display Fit / Fill + explanation | **Fit** | ✅ picker + explanation; default stays **Fill** |
| S04 | Player text size | 12–24, step 1, default 16.5 | ✅ 12–24 px, default 16 (progress, empty state, notes) |
| S05 | Subtitle text size | 12–108, step 1, default 18 | ✅ 12–108 px, default 26 |
| S06 | Subtitle background transparency | 0–100%, step 1, default 0% | ✅ with black text outline |
| S07 | Use default system colors | On | Not wanted |
| S08 | Foreground / background color pickers (RGB, no alpha) | — | Not wanted |
| S09 | Live appearance preview "Audiobook title · 12:34 remaining" | — | ✅ |
| S10 | Reset Appearance (appearance keys only, not playback/book prefs or notes size) | — | Not wanted |

The spec also requires that settings apply immediately and have no Save/Cancel.
Qt does this for the settings it has.
`AppPreferences` needs new keys, and `STORAGE.md` must be updated with them.

### 2.20 Menus (M01–M07)

| Spec item | Qt | Status |
| --- | --- | --- |
| File → Open… (Ctrl+O), native picker, one `.m4b` | Present. Filter also allows m4a/mp4/all files | OK (broader filter) |
| File → Open Recent, max **10** entries, title fallback to path | Up to **30** (`Library.cpp:352`). Uses the title, with no path fallback if the title is empty (blank entry) | Partial NEW |
| Open Recent: **missing path → try other known locations of the same book** | `openLocalPath(path)` fails with "Choose an available local audiobook file." Only launch resume walks other locations (`lastAvailablePath`) | **Missing NEW** |
| Open Recent → **Clear Menu** | Missing | **Missing NEW** |
| File → Info… Ctrl+I | File → Audiobook Info… Ctrl+I | ✅ |
| File → Open Notes Ctrl+N | Present as "Notes…" | OK |
| File → Export Current Chapter as WAV… / Export Current Chunk as WAV… / "Exporting Audio…" | Present | ✅ |
| File → Close Ctrl+W | Missing (only Quit) | Minor NEW |
| Edit menu (Undo/Redo/Cut/Copy/Paste/Select All) | No Edit menu. Text fields handle the standard keys themselves | Partial NEW |
| View → Open EQ Ctrl+E | Under Playback with volume; Ctrl+E works | OK (placement differs) |
| No duplicated commands | Menus reorganized: File (book/files), Playback (transport, chapters, speed, sound), View (captions, full screen, Appearance), Help. Appearance and caption size no longer appear twice | ✅ |
| View → Full Screen | F11 | OK |
| Playback: Play/Pause, Stop, Prev/Next, ±10 s, **Decrease/Increase Speed**, Stop After Chapter, Volume ± | Speed ± actions missing (preset submenu instead) | Partial |
| Window menu (Minimize, window list) | None (desktop-provided) | Acceptable on Linux |
| Help → Help, About | Present | OK |

### 2.21 WAV export (M02c–M02e): ✅ Implemented

Implemented in `src/AudioExporter.cpp` (decoding through `src/AudioDecode.cpp`)
and the File menu. Differences: the command is labelled "Export Current Caption
Chunk as WAV…"; the chunk filename is `<title> - H-MM-SS.mmm.wav`; and errors are
shown as "Unable to Export Audio" in an in-window dialog. The original spec items were:

- **Export Current Chapter as WAV…**: the whole chapter containing the playhead,
  range captured when invoked, suggested filename
  `"<book title> - <NN> <chapter title>.wav"` with unsafe characters replaced.
- **Export Current Chunk as WAV…**: the active non-empty caption cue's range.
  Works with CC Off. Enabled whenever the book has supported cues. In a gap or at
  book end it shows **"No Chunk at Current Position"** / "The current playback
  position is in a gap between subtitle chunks." with OK, and no save dialog.
  Filename based on cue start time.
- Save dialog titled "Export Audio as WAV", overwrite confirmation.
- Background export; menu shows "Exporting Audio…" and disables both actions.
- Output: **16-bit PCM WAV**, source sample rate and channels, normal speed, no
  volume/EQ.
- Atomic write (never leave a partial file over the destination). Failure shows
  "Unable to Export Audio" with the error. No success dialog.

Implementation note: the FFmpeg decode path in `LoudnessReader` (exact
chapter-boundary trimming on a cancellable worker) plus `QSaveFile` is close to
what this needs.

### 2.22 Media keys and Now Playing (K02)

Missing (tracked, milestone 4). On Linux this means an **MPRIS2** D-Bus service
(`org.mpris.MediaPlayer2.Player`): Play/Pause/PlayPause, Next/Previous mapped to
**chapter navigation (which pauses)**, Seek ±10 s, and Metadata (title, author,
art URL, length, position, rate). Play at book end must not restart. The rate
should report 0 while audio is suspended.

### 2.23 Errors and dialogs (I02, I03)

| Spec | Qt | Status |
| --- | --- | --- |
| Modal alerts: "Unable to Open Audiobook", "Unable to Open Notes", "Unable to Export Audio", "No Chunk at Current Position" | All errors go to a dismissible **red inline banner** (`Main.qml:356-374`) | Diverges (banner is reasonable; chunk/export dialogs needed once export exists) |
| Export failure / gap message | "Unable to Export Audio" and "No Chunk at Current Position" in-window dialogs | ✅ |
| Library-open failure explains playback is unavailable | `main.cpp` exits with stderr on a library exception; no UI | **Partial NEW** |
| Missing Recent: "could not be found at any known location" | Generic "Choose an available local audiobook file." | Partial NEW |
| Info inline loading/error states | N/A | — |

### 2.24 Focus and accessibility (I04)

Mostly present: popups restore trigger focus on close, and shortcuts are suppressed
while popups or notes are open. Gaps:

- Spec: focus returns to the **player surface** (a non-editing target). Qt
  returns focus to the triggering button. Equivalent in practice.
- Accessible **values** (not just names) are missing on: chapter slider
  (elapsed/total), CC/Stop After Chapter (On/Off), speed, volume (%), Play/Pause
  surface, chapter rows ("Current chapter").
- Separate focus sections for transport and utilities: not set up.
- Manual screen-reader validation is outstanding (tracked).

### 2.25 Persistence matrix (I05)

| State | Spec | Qt |
| --- | --- | --- |
| Position | Per book | ✅ |
| Rating | Per book | ✅ |
| Notes | Per book | ✅ |
| Stop After Chapter | Per book + global default | ✅ per book / ❌ default |
| CC | Per book, default Off | ✅ |
| **Speed** | **Per book, default 1×** | ❌ not stored |
| **EQ switch, Tone, Hz** | **Per book** | ❌ session-only (Boost too) |
| Volume | Session only | ✅ |
| Appearance prefs | Global | Partial (only 2 keys) |
| Cover crop | Local, reset on load | ✅ |
| **Notes editor size** | **Global** | ❌ |
| Recent list | Global, 10 | Derived from DB, 30, no Clear |
| Prev/Next remembered positions | Session | ✅ |

Speed and EQ persistence need a schema v3 migration (e.g. `speed REAL`,
`eq_enabled INTEGER`, `eq_tone REAL`, `eq_frequency REAL`, all nullable). The
existing v1→v2 path (backup, transactional rebuild) is the pattern to follow.
Recent-list clearing needs either a `hidden_from_recents`/`recent_cleared_at`
marker or a separate list, because Recents are currently derived from `books.last_opened`.

### 2.26 Multi-window and async behavior (I06)

Missing (tracked): per-window engine, focus-routed commands, Open Recent bound to
the invoking window, EQ panel per window, app quit flushing all players. The
async load-generation guards already in `PlayerController` carry over per
instance.

---

## 3. Acceptance checklist (spec I07) evaluated against Qt

| Scenario | Qt result (from source) |
| --- | --- |
| Play/cover/caption while paused starts playback | ✅ |
| Finish final chapter, then Play: stays at end | ❌ restarts from 0 |
| Finish intermediate chapter with Stop After On: pauses at next start; Play starts it | ✅ (possible brief overrun) |
| Turn Stop After off after a cutoff: stays paused | ✅ |
| Prev/Next while playing: pauses, moves one chapter, remembers position | ✅ (but ❌ edge behavior) |
| Chapter-list row while playing: seeks to start and resumes; popover closes | ✅ |
| Drag slider fully right: stays in chapter; release keeps intent | ✅ |
| Scroll speed while playing, then Pause: keeps speed, no auto-resume | ❌ no scroll-speed feature |
| Malformed speed input reverts | ❌ no text entry |
| Hide captions, export chunk | ✅ |
| Chunk export in a gap shows message | ✅ |
| Cancel Notes discards | ✅ |
| Failed Notes save keeps draft | ✅ |
| Reset EQ while enabled → neutral/1 kHz, still On | ✅ (also zeroes Boost) |
| Reset Appearance restores only appearance | ❌ no reset |
| Dismiss popover after speed edit: player regains focus | N/A |
| Resize / font size change: no overlap, no duplicate controls | ✅ resize / ❌ no font-size preference |
| Late metadata after user seek keeps the seek | ✅ |
| Open Recent then focus other player | N/A (single window) |

---

## 4. Qt-only features (not in the spec)

Keep these, but they should be documented as intentional extensions:

- **Boost** 0–24 dB with 4× oversampled lookahead limiter, and **Auto loudness
  −18 LKFS** per-chapter measurement.
- Frosted (blurred) transport panel in Fill mode.
- Ctrl+J chapter list shortcut, F11 full screen, F1 help.
- Playback speeds above 2× (up to 3× in presets).
- Broader open filter (`.m4a`, `.mp4`, all files).
- NAS-friendly async libmpv I/O and an 8 MiB packet cache limit.
- Fallback MP4 `covr` box reader for artwork FFmpeg skips.
- CLI options `--data-dir`, `--no-resume`, `--smoke-test`, `--screenshot`, `--dependency-versions`.

---

## 5. Suggested implementation order

Ordered by user value and risk. This is a suggestion only; no code has been changed.

1. **Behavior corrections (small, high value):** Play at book end does nothing;
   reopen restores only interior positions; Prev/Next enabled at edges and pause;
   remaining-time formatting (minutes/seconds, rounded %); Fit as default cover
   mode (decide).
2. **Per-book speed and EQ persistence** (schema v3) and a **Stop After Chapter
   global default**.
3. **Settings window** S01–S10 (resume toggle, text sizes with spec ranges,
   caption transparency/outline/colors with contrast check, preview, Reset
   Appearance). Includes the reserved 224 pt caption area.
4. **Speed field** (typed entry 0.5–2.0, 0.01 scroll steps with audio suspend,
   Ctrl+Shift+←/→ or another Linux-safe ±0.1× shortcut; decide key mapping).
5. **Open Recent** improvements: location fallback, Clear Menu, 10-entry cap,
   path fallback label, specific "not found" message.
6. ~~Audiobook Info~~ (done).
7. ~~WAV export~~ (done).
8. **Drag-and-drop** opening with overlay.
9. **MPRIS** media controls and Now Playing metadata.
10. **Multi-window sessions** plus single-instance open forwarding (largest
    refactor: one `PlayerController`/`MpvEngine` per window, focus routing).
11. **Polish:** chapter durations in list, volume level icons, loading spinner and
    no-art placeholder, accessible values, notes size persistence and red error,
    EQ value labels (kHz, "dB clearer", "Neutral"), Help as a proper window,
    Edit menu / Close (Ctrl+W).
12. **Engine-side Stop After Chapter cutoff** and EQ smoothing (tracked hardening).

## 6. Decisions to make before implementing

These are differences where the Qt app may have deliberately chosen otherwise:

- **Keyboard map:** spec Esc = Stop and ↑/↓ = chapters, vs Qt Ctrl+S and Ctrl+←/→.
  Spec Cmd+←/→ = speed. Pick a Linux mapping and document it.
- **EQ scope:** spec saves EQ per book; Qt deliberately keeps it per session across
  books. Choose one, or make it a preference.
- **Speed range:** spec 0.5–2.0×; Qt offers up to 3×.
- **Default cover mode:** spec Fit; Qt Fill.
- **Error presentation:** modal alerts (spec) vs inline banner (Qt).
- **Notes presentation:** sheet vs separate dialog window (Linux has no sheets).

When items are adopted, move them into the single
[known gaps and milestones](ARCHITECTURE.md#known-gaps-and-milestones) list so that
list stays the source of truth.
