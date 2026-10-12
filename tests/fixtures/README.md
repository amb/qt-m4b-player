# Reference fixtures

These three short synthetic M4B files were copied unchanged from
`macos-m4b-player/Tests/m4b-playerTests/Fixtures` at reference commit
`7dd0c79a117bff96cd7faecf5423d50821e49d6c`.

- `captions.m4b`: six seconds, two chapters, cover, two timed `mov_text` cues and gaps.
- `no-captions.m4b`: chapters but no usable caption track.
- `long-captions.m4b`: long text for scrolling/hold behavior.

Tests use these local copies, never require a macOS build, and never write to the
reference project. Expected cue timings and the v1 fingerprint are checked
independently of the playback backend.

`fractional-chapters.m4b` reuses the synthetic audio from `no-captions.m4b`
with a chapter boundary at 3.621 seconds. It covers decoded audio clocks that
round below an exact chapter seek and the resulting Stop After Chapter behavior.
To regenerate it from this directory:

```sh
ffmpeg -v error -i no-captions.m4b -i fractional-chapters.ffmetadata \
  -map 0:a:0 -map_metadata 1 -map_chapters 1 -c:a copy -y fractional-chapters.m4b
```
