#!/usr/bin/env python3
"""Generate Linux app icons from the original macOS PNG (requires Pillow)."""
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter


root = Path(__file__).resolve().parents[1] / "packaging/icons"
name = "io.github.amb.QtM4bPlayer.png"
original = Image.open(root / "1024x1024" / name).convert("RGB")
width, height = original.size

# The source has a white matte rather than an alpha channel. Only remove light
# pixels connected to the outside corners, preserving the white book inside.
red, green, blue = original.split()
outside = ImageChops.darker(ImageChops.darker(red, green), blue).point(
    lambda value: 255 if value >= 220 else 0)
for corner in ((0, 0), (width-1, 0), (0, height-1), (width-1, height-1)):
    ImageDraw.floodfill(outside, corner, 128)
outside = outside.point(lambda value: 255 if value == 128 else 0)
edge = outside.filter(ImageFilter.MaxFilter(25))
result = original.convert("RGBA")
for y in range(height):
    for x in range(width):
        if outside.getpixel((x, y)):
            result.putpixel((x, y), (0, 0, 0, 0))
        elif edge.getpixel((x, y)):
            # Undo the white matte along the antialiased edge. Estimate the
            # purple background from a nearby pixel toward the icon's centre.
            foreground = original.getpixel((x + (12 if x < width/2 else -12),
                                            y + (12 if y < height/2 else -12)))
            pixel = original.getpixel((x, y))
            distances = [255-channel for channel in foreground]
            denominator = sum(value*value for value in distances)
            alpha = min(1.0, max(0.0, sum((255-channel)*distance
                for channel, distance in zip(pixel, distances))/denominator)) if denominator else 1.0
            if alpha < 0.98:
                colour = tuple(round(min(255, max(0, (channel-255*(1-alpha))/alpha)))
                               for channel in pixel) if alpha else (0, 0, 0)
                result.putpixel((x, y), (*colour, round(255*alpha)))

for size in (16, 32, 48, 64, 128, 256, 512):
    result.resize((size, size), Image.Resampling.LANCZOS).save(
        root / f"{size}x{size}" / name, optimize=True)
