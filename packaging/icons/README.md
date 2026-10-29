# Application icon

The application uses the purple book/play artwork from the macOS reference
project. The 1024×1024 PNG is copied unchanged from `Resources/icon-1024.png`
(introduced in reference commit `8c25b4b`), under the project's MIT license.

The master is RGB with opaque white outside corners. Run
`python3 scripts/generate-app-icons.py` from the project root (requires Pillow)
to remove the outside matte, recover antialiased transparent edges and generate
the smaller PNGs with Lanczos resampling. The white book artwork stays opaque.

CMake installs sizes up to
512×512 in the hicolor icon theme for desktop launchers and Flatpak exports
(Flatpak's maximum raster icon size). The 512×512 image is also embedded in the
executable for development builds and window icons.
