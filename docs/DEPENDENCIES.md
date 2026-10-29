# Dependency versions

Initial dependency management is **apt + CMake**, using dynamic system libraries.
The JSON baseline in [dependencies-linux-x86_64.json](dependencies-linux-x86_64.json)
records this project's verified build environment, including the resolved
shared-library packages and the relevant dynamically loaded Qt plugins.

| Dependency | First build version | Ubuntu package revision |
| --- | --- | --- |
| Qt Base / SQL | 6.10.2 | `6.10.2+dfsg-7` |
| Qt Declarative / Quick / QML | 6.10.2 | `6.10.2+dfsg-3` |
| Qt SVG image plugin | 6.10.2 | `6.10.2-2` |
| mpv engine | 0.41.0 | `0.41.0-2ubuntu4` |
| libmpv client API reported by pkg-config | 2.5.0 | Same mpv package |
| FFmpeg | 8.0.1 | `7:8.0.1-3ubuntu2` |
| libavformat | 62.3.100 | Same FFmpeg package |
| libavcodec | 62.11.100 | Same FFmpeg package |
| libavutil | 60.8.100 | Same FFmpeg package |
| libavfilter | 11.4.100 | Same FFmpeg package; directly used by chapter loudness measurement |
| SQLite runtime via Qt SQL | 3.46.1 | `3.46.1-9ubuntu0.3` |

The `mpv.pc` version is the **client API** version, not the engine release.
FFmpeg components similarly expose library versions separate from the FFmpeg
release. Record both instead of comparing unlike version numbers.

Build tooling from the same baseline:

| Tool | Version | Ubuntu package |
| --- | --- | --- |
| CMake | 4.2.3 | `cmake 4.2.3-2ubuntu2` |
| Ninja | 1.13.2 | `ninja-build 1.13.2-1` |
| GNU C++ | 15.2.0 | `g++ 4:15.2.0-5ubuntu1` |
| pkg-config | 2.5.1 | — |
| Python 3 | 3.14.3 | `python3 3.14.3-0ubuntu2` |

CMake requires Qt >= 6.5, libmpv API >= 2.0, and FFmpeg 6-era library versions
or newer, including libavfilter >= 9 for chapter loudness measurement. These are
build minimums; only the baseline above has been validated.
No code or headers are downloaded during configuration.

The bundled Phosphor icon assets use `@phosphor-icons/core` 2.1.1, pinned to
commit `2b75f3ad12b420c9504ef05df8d2564a28f8500e`. They are MIT licensed;
[the asset manifest](../assets/icons/manifest.json) records exact source URLs
and SHA-256 checksums. Qt's SVG plugin renders them; it is already installed on
this host and is included in the package inventory. A `QQuickImageProvider`
rasterizes the SVGs at the requested pixel size and uses `QPainter` to tint
their alpha masks from the active palette. The resulting ordinary QML images
work with both software and GPU rendering, without a Qt Quick Effects dependency.

Release builds are distributed as self-hosted Flatpak bundles built against the
`org.kde.Platform` runtime, which supplies Qt and KDE Frameworks; mpv, libass and
libplacebo are built in the manifest. See [releasing](RELEASING.md). The apt
versions below remain the native development baseline.

## Capture an environment

```bash
python3 scripts/dependency_report.py --binary build/debug/qt-m4b-player \
  --output build/debug/dependency-report.json
```

The script records direct pkg-config versions, build tools, runtime versions,
apt package revisions and libraries resolved by `ldd` for the executable and
selected Qt SQL/QML plugins and the SVG image plugin. Plugins/optional media components dynamically
loaded in other configurations may require additional inventory. The report
does not capture an entire operating-system image or promise identical binaries.

`build/debug/dependency-versions.txt` records configure-time versions and
`qt-m4b-player --dependency-versions` prints runtime Qt/mpv/SQLite versions.
Inspect a new report after apt upgrades, rebuild and rerun the tests. Updating
the committed baseline is a deliberate source change after validation.

This is an **environment record, not a dependency lockfile**: apt can update
libraries, and old package revisions may leave the configured mirrors. Future
reproducible releases need a pinned repository snapshot/container or a proper
dependency build workflow. There are currently no held apt packages and no
project-managed changes to the machine's package policy.
