# Transport icons

These icons come from [Phosphor Icons](https://github.com/phosphor-icons/core),
package `@phosphor-icons/core` version **2.1.1**, at upstream commit
`2b75f3ad12b420c9504ef05df8d2564a28f8500e`.

They are licensed under **MIT**, copyright © 2023 Phosphor Icons.
The full upstream [license](LICENSE.phosphor) is bundled in the application
resources and installed in `share/licenses/qt-m4b-player`.

`upstream/` contains the unchanged source SVGs. The light and dark variants
replace `currentColor` with the appropriate foreground color. QML overlays
“10” on the circular-arrow icons for the ten-second skip commands. CC and speed
remain text, matching the reference. The [manifest](manifest.json) records
source URLs and SHA-256 checksums for originals, variants, and the license.

The filled transport, eject, and speaker symbols and regular chapter/seek
symbols are chosen to approximate the reference application's icon weights.
Qt's SVG image plugin (`libqt6svg6` on this host) renders the bundled vectors
at the display's pixel density. `TransportIconProvider` tints their alpha masks
with the active palette colour before QML displays them, so software and GPU
renderers show the same icons. No icon font or network access is required.
