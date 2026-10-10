#pragma once

namespace onion {

struct OverlayColor {
  float r;
  float g;
  float b;
  float a;
};

struct OverlayPalette {
  OverlayColor cpu;
  OverlayColor gpu;
  OverlayColor memory;
  OverlayColor network;
  OverlayColor fan;
  OverlayColor fps;
  OverlayColor value;
  OverlayColor separator;
  OverlayColor background;
};

inline constexpr OverlayPalette kClassicOverlayPalette = {
    {102.0f / 255.0f, 1.0f, 102.0f / 255.0f, 1.0f},
    {179.0f / 255.0f, 102.0f / 255.0f, 1.0f, 1.0f},
    {1.0f, 179.0f / 255.0f, 77.0f / 255.0f, 1.0f},
    {128.0f / 255.0f, 179.0f / 255.0f, 1.0f, 1.0f},
    {51.0f / 255.0f, 224.0f / 255.0f, 1.0f, 1.0f},
    {1.0f, 230.0f / 255.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {0.75f, 0.75f, 0.75f, 1.0f},
    {0.0f, 0.0f, 0.0f, 180.0f / 255.0f},
};

inline constexpr OverlayPalette kMuseOverlayPalette = {
    {1.0f, 128.0f / 255.0f, 181.0f / 255.0f, 1.0f},
    {196.0f / 255.0f, 160.0f / 255.0f, 1.0f, 1.0f},
    {1.0f, 196.0f / 255.0f, 138.0f / 255.0f, 1.0f},
    {132.0f / 255.0f, 216.0f / 255.0f, 1.0f, 1.0f},
    {120.0f / 255.0f, 232.0f / 255.0f, 211.0f / 255.0f, 1.0f},
    {248.0f / 255.0f, 229.0f / 255.0f, 140.0f / 255.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {0.82f, 0.76f, 0.88f, 1.0f},
    {20.0f / 255.0f, 14.0f / 255.0f, 32.0f / 255.0f,
     184.0f / 255.0f},
};

enum class OverlayPaletteId {
  Classic = 0,
  Muse = 1,
};

constexpr const OverlayPalette &overlay_palette(OverlayPaletteId id) {
  return id == OverlayPaletteId::Muse ? kMuseOverlayPalette
                                      : kClassicOverlayPalette;
}

} // namespace onion
