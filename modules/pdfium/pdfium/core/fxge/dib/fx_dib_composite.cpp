// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/fx_codec.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/dib/dib_int.h"
#include "core/fxge/ge/cfx_cliprgn.h"

namespace {

const uint8_t color_sqrt[256] = {
    0x00, 0x03, 0x07, 0x0B, 0x0F, 0x12, 0x16, 0x19, 0x1D, 0x20, 0x23, 0x26,
    0x29, 0x2C, 0x2F, 0x32, 0x35, 0x37, 0x3A, 0x3C, 0x3F, 0x41, 0x43, 0x46,
    0x48, 0x4A, 0x4C, 0x4E, 0x50, 0x52, 0x54, 0x56, 0x57, 0x59, 0x5B, 0x5C,
    0x5E, 0x60, 0x61, 0x63, 0x64, 0x65, 0x67, 0x68, 0x69, 0x6B, 0x6C, 0x6D,
    0x6E, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
    0x7B, 0x7C, 0x7D, 0x7E, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x91,
    0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C,
    0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA4, 0xA5,
    0xA6, 0xA7, 0xA7, 0xA8, 0xA9, 0xAA, 0xAA, 0xAB, 0xAC, 0xAD, 0xAD, 0xAE,
    0xAF, 0xB0, 0xB0, 0xB1, 0xB2, 0xB3, 0xB3, 0xB4, 0xB5, 0xB5, 0xB6, 0xB7,
    0xB7, 0xB8, 0xB9, 0xBA, 0xBA, 0xBB, 0xBC, 0xBC, 0xBD, 0xBE, 0xBE, 0xBF,
    0xC0, 0xC0, 0xC1, 0xC2, 0xC2, 0xC3, 0xC4, 0xC4, 0xC5, 0xC6, 0xC6, 0xC7,
    0xC7, 0xC8, 0xC9, 0xC9, 0xCA, 0xCB, 0xCB, 0xCC, 0xCC, 0xCD, 0xCE, 0xCE,
    0xCF, 0xD0, 0xD0, 0xD1, 0xD1, 0xD2, 0xD3, 0xD3, 0xD4, 0xD4, 0xD5, 0xD6,
    0xD6, 0xD7, 0xD7, 0xD8, 0xD9, 0xD9, 0xDA, 0xDA, 0xDB, 0xDC, 0xDC, 0xDD,
    0xDD, 0xDE, 0xDE, 0xDF, 0xE0, 0xE0, 0xE1, 0xE1, 0xE2, 0xE2, 0xE3, 0xE4,
    0xE4, 0xE5, 0xE5, 0xE6, 0xE6, 0xE7, 0xE7, 0xE8, 0xE9, 0xE9, 0xEA, 0xEA,
    0xEB, 0xEB, 0xEC, 0xEC, 0xED, 0xED, 0xEE, 0xEE, 0xEF, 0xF0, 0xF0, 0xF1,
    0xF1, 0xF2, 0xF2, 0xF3, 0xF3, 0xF4, 0xF4, 0xF5, 0xF5, 0xF6, 0xF6, 0xF7,
    0xF7, 0xF8, 0xF8, 0xF9, 0xF9, 0xFA, 0xFA, 0xFB, 0xFB, 0xFC, 0xFC, 0xFD,
    0xFD, 0xFE, 0xFE, 0xFF};

int Blend(int blend_mode, int back_color, int src_color) {
  switch (blend_mode) {
    case FXDIB_BLEND_NORMAL:
      return src_color;
    case FXDIB_BLEND_MULTIPLY:
      return src_color * back_color / 255;
    case FXDIB_BLEND_SCREEN:
      return src_color + back_color - src_color * back_color / 255;
    case FXDIB_BLEND_OVERLAY:
      return Blend(FXDIB_BLEND_HARDLIGHT, src_color, back_color);
    case FXDIB_BLEND_DARKEN:
      return src_color < back_color ? src_color : back_color;
    case FXDIB_BLEND_LIGHTEN:
      return src_color > back_color ? src_color : back_color;
    case FXDIB_BLEND_COLORDODGE: {
      if (src_color == 255) {
        return src_color;
      }
      int result = back_color * 255 / (255 - src_color);
      if (result > 255) {
        return 255;
      }
      return result;
    }
    case FXDIB_BLEND_COLORBURN: {
      if (src_color == 0) {
        return src_color;
      }
      int result = (255 - back_color) * 255 / src_color;
      if (result > 255) {
        result = 255;
      }
      return 255 - result;
    }
    case FXDIB_BLEND_HARDLIGHT:
      if (src_color < 128) {
        return (src_color * back_color * 2) / 255;
      }
      return Blend(FXDIB_BLEND_SCREEN, back_color, 2 * src_color - 255);
    case FXDIB_BLEND_SOFTLIGHT: {
      if (src_color < 128) {
        return back_color -
               (255 - 2 * src_color) * back_color * (255 - back_color) / 255 /
                   255;
      }
      return back_color +
             (2 * src_color - 255) * (color_sqrt[back_color] - back_color) /
                 255;
    }
    case FXDIB_BLEND_DIFFERENCE:
      return back_color < src_color ? src_color - back_color
                                    : back_color - src_color;
    case FXDIB_BLEND_EXCLUSION:
      return back_color + src_color - 2 * back_color * src_color / 255;
  }
  return src_color;
}

struct RGB {
  int red;
  int green;
  int blue;
};

int Lum(RGB color) {
  return (color.red * 30 + color.green * 59 + color.blue * 11) / 100;
}

RGB ClipColor(RGB color) {
  int l = Lum(color);
  int n = color.red;
  if (color.green < n) {
    n = color.green;
  }
  if (color.blue < n) {
    n = color.blue;
  }
  int x = color.red;
  if (color.green > x) {
    x = color.green;
  }
  if (color.blue > x) {
    x = color.blue;
  }
  if (n < 0) {
    color.red = l + ((color.red - l) * l / (l - n));
    color.green = l + ((color.green - l) * l / (l - n));
    color.blue = l + ((color.blue - l) * l / (l - n));
  }
  if (x > 255) {
    color.red = l + ((color.red - l) * (255 - l) / (x - l));
    color.green = l + ((color.green - l) * (255 - l) / (x - l));
    color.blue = l + ((color.blue - l) * (255 - l) / (x - l));
  }
  return color;
}

RGB SetLum(RGB color, int l) {
  int d = l - Lum(color);
  color.red += d;
  color.green += d;
  color.blue += d;
  return ClipColor(color);
}

int Sat(RGB color) {
  int n = color.red;
  if (color.green < n) {
    n = color.green;
  }
  if (color.blue < n) {
    n = color.blue;
  }
  int x = color.red;
  if (color.green > x) {
    x = color.green;
  }
  if (color.blue > x) {
    x = color.blue;
  }
  return x - n;
}

RGB SetSat(RGB color, int s) {
  int* max = &color.red;
  int* mid = &color.red;
  int* min = &color.red;
  if (color.green > *max) {
    max = &color.green;
  }
  if (color.blue > *max) {
    max = &color.blue;
  }
  if (color.green < *min) {
    min = &color.green;
  }
  if (color.blue < *min) {
    min = &color.blue;
  }
  if (*max == *min) {
    color.red = 0;
    color.green = 0;
    color.blue = 0;
    return color;
  }
  if (max == &color.red) {
    if (min == &color.green) {
      mid = &color.blue;
    } else {
      mid = &color.green;
    }
  } else if (max == &color.green) {
    if (min == &color.red) {
      mid = &color.blue;
    } else {
      mid = &color.red;
    }
  } else {
    if (min == &color.green) {
      mid = &color.red;
    } else {
      mid = &color.green;
    }
  }
  if (*max > *min) {
    *mid = (*mid - *min) * s / (*max - *min);
    *max = s;
    *min = 0;
  }
  return color;
}

void RGB_Blend(int blend_mode,
               const uint8_t* src_scan,
               uint8_t* dest_scan,
               int results[3]) {
  RGB src;
  RGB back;
  RGB result = {0, 0, 0};
  src.red = src_scan[2];
  src.green = src_scan[1];
  src.blue = src_scan[0];
  back.red = dest_scan[2];
  back.green = dest_scan[1];
  back.blue = dest_scan[0];
  switch (blend_mode) {
    case FXDIB_BLEND_HUE:
      result = SetLum(SetSat(src, Sat(back)), Lum(back));
      break;
    case FXDIB_BLEND_SATURATION:
      result = SetLum(SetSat(back, Sat(src)), Lum(back));
      break;
    case FXDIB_BLEND_COLOR:
      result = SetLum(src, Lum(back));
      break;
    case FXDIB_BLEND_LUMINOSITY:
      result = SetLum(back, Lum(src));
      break;
  }
  results[0] = result.blue;
  results[1] = result.green;
  results[2] = result.red;
}

void CompositeRow_Argb2Mask(uint8_t* dest_scan,
                            const uint8_t* src_scan,
                            int pixel_count,
                            const uint8_t* clip_scan) {
  src_scan += 3;
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha = *src_scan;
    if (clip_scan) {
      src_alpha = clip_scan[col] * src_alpha / 255;
    }
    uint8_t back_alpha = *dest_scan;
    if (!back_alpha) {
      *dest_scan = src_alpha;
    } else if (src_alpha) {
      *dest_scan = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    }
    dest_scan++;
    src_scan += 4;
  }
}

void CompositeRow_Rgba2Mask(uint8_t* dest_scan,
                            const uint8_t* src_alpha_scan,
                            int pixel_count,
                            const uint8_t* clip_scan) {
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha = *src_alpha_scan++;
    if (clip_scan) {
      src_alpha = clip_scan[col] * src_alpha / 255;
    }
    uint8_t back_alpha = *dest_scan;
    if (!back_alpha) {
      *dest_scan = src_alpha;
    } else if (src_alpha) {
      *dest_scan = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    }
    dest_scan++;
  }
}

void CompositeRow_Rgb2Mask(uint8_t* dest_scan,
                           const uint8_t* src_scan,
                           int width,
                           const uint8_t* clip_scan) {
  if (clip_scan) {
    for (int i = 0; i < width; i++) {
      *dest_scan = FXDIB_ALPHA_UNION(*dest_scan, *clip_scan);
      dest_scan++;
      clip_scan++;
    }
  } else {
    FXSYS_memset(dest_scan, 0xff, width);
  }
}

void CompositeRow_Argb2Graya(uint8_t* dest_scan,
                             const uint8_t* src_scan,
                             int pixel_count,
                             int blend_type,
                             const uint8_t* clip_scan,
                             const uint8_t* src_alpha_scan,
                             uint8_t* dst_alpha_scan,
                             void* pIccTransform) {
  CCodec_IccModule* pIccModule = nullptr;
  if (pIccTransform)
    pIccModule = CFX_GEModule::Get()->GetCodecModule()->GetIccModule();

  if (blend_type) {
    bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
    if (src_alpha_scan) {
      for (int col = 0; col < pixel_count; col++) {
        uint8_t back_alpha = *dst_alpha_scan;
        if (back_alpha == 0) {
          int src_alpha = *src_alpha_scan++;
          if (clip_scan)
            src_alpha = clip_scan[col] * src_alpha / 255;

          if (src_alpha) {
            if (pIccTransform) {
              pIccModule->TranslateScanline(pIccTransform, dest_scan, src_scan,
                                            1);
            } else {
              *dest_scan = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
            }
            *dst_alpha_scan = src_alpha;
          }
          dest_scan++;
          dst_alpha_scan++;
          src_scan += 3;
          continue;
        }
        uint8_t src_alpha = *src_alpha_scan++;
        if (clip_scan)
          src_alpha = clip_scan[col] * src_alpha / 255;

        if (src_alpha == 0) {
          dest_scan++;
          dst_alpha_scan++;
          src_scan += 3;
          continue;
        }
        *dst_alpha_scan = FXDIB_ALPHA_UNION(back_alpha, src_alpha);
        int alpha_ratio = src_alpha * 255 / (*dst_alpha_scan);
        uint8_t gray;
        if (pIccTransform) {
          pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
        } else {
          gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
        }
        if (bNonseparableBlend)
          gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
        else
          gray = Blend(blend_type, *dest_scan, gray);
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
        dest_scan++;
        dst_alpha_scan++;
        src_scan += 3;
      }
    } else {
      for (int col = 0; col < pixel_count; col++) {
        uint8_t back_alpha = *dst_alpha_scan;
        if (back_alpha == 0) {
          int src_alpha = src_scan[3];
          if (clip_scan)
            src_alpha = clip_scan[col] * src_alpha / 255;

          if (src_alpha) {
            if (pIccTransform) {
              pIccModule->TranslateScanline(pIccTransform, dest_scan, src_scan,
                                            1);
            } else {
              *dest_scan = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
            }
            *dst_alpha_scan = src_alpha;
          }
          dest_scan++;
          dst_alpha_scan++;
          src_scan += 4;
          continue;
        }
        uint8_t src_alpha = src_scan[3];
        if (clip_scan)
          src_alpha = clip_scan[col] * src_alpha / 255;

        if (src_alpha == 0) {
          dest_scan++;
          dst_alpha_scan++;
          src_scan += 4;
          continue;
        }
        *dst_alpha_scan = FXDIB_ALPHA_UNION(back_alpha, src_alpha);
        int alpha_ratio = src_alpha * 255 / (*dst_alpha_scan);
        uint8_t gray;
        if (pIccTransform)
          pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
        else
          gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);

        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
        dest_scan++;
        dst_alpha_scan++;
        src_scan += 4;
      }
    }
    return;
  }
  if (src_alpha_scan) {
    for (int col = 0; col < pixel_count; col++) {
      uint8_t back_alpha = *dst_alpha_scan;
      if (back_alpha == 0) {
        int src_alpha = *src_alpha_scan++;
        if (clip_scan)
          src_alpha = clip_scan[col] * src_alpha / 255;

        if (src_alpha) {
          if (pIccTransform) {
            pIccModule->TranslateScanline(pIccTransform, dest_scan, src_scan,
                                          1);
          } else {
            *dest_scan = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
          }
          *dst_alpha_scan = src_alpha;
        }
        dest_scan++;
        dst_alpha_scan++;
        src_scan += 3;
        continue;
      }
      uint8_t src_alpha = *src_alpha_scan++;
      if (clip_scan)
        src_alpha = clip_scan[col] * src_alpha / 255;

      if (src_alpha == 0) {
        dest_scan++;
        dst_alpha_scan++;
        src_scan += 3;
        continue;
      }
      *dst_alpha_scan = FXDIB_ALPHA_UNION(back_alpha, src_alpha);
      int alpha_ratio = src_alpha * 255 / (*dst_alpha_scan);
      uint8_t gray;
      if (pIccTransform)
        pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
      else
        gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);

      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
      dest_scan++;
      dst_alpha_scan++;
      src_scan += 3;
    }
  } else {
    for (int col = 0; col < pixel_count; col++) {
      uint8_t back_alpha = *dst_alpha_scan;
      if (back_alpha == 0) {
        int src_alpha = src_scan[3];
        if (clip_scan)
          src_alpha = clip_scan[col] * src_alpha / 255;

        if (src_alpha) {
          if (pIccTransform) {
            pIccModule->TranslateScanline(pIccTransform, dest_scan, src_scan,
                                          1);
          } else {
            *dest_scan = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
          }
          *dst_alpha_scan = src_alpha;
        }
        dest_scan++;
        dst_alpha_scan++;
        src_scan += 4;
        continue;
      }
      uint8_t src_alpha = src_scan[3];
      if (clip_scan)
        src_alpha = clip_scan[col] * src_alpha / 255;

      if (src_alpha == 0) {
        dest_scan++;
        dst_alpha_scan++;
        src_scan += 4;
        continue;
      }
      *dst_alpha_scan = FXDIB_ALPHA_UNION(back_alpha, src_alpha);
      int alpha_ratio = src_alpha * 255 / (*dst_alpha_scan);
      uint8_t gray;
      if (pIccTransform)
        pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
      else
        gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);

      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
      dest_scan++;
      dst_alpha_scan++;
      src_scan += 4;
    }
  }
}

void CompositeRow_Argb2Gray(uint8_t* dest_scan,
                            const uint8_t* src_scan,
                            int pixel_count,
                            int blend_type,
                            const uint8_t* clip_scan,
                            const uint8_t* src_alpha_scan,
                            void* pIccTransform) {
  CCodec_IccModule* pIccModule = nullptr;
  uint8_t gray;
  if (pIccTransform)
    pIccModule = CFX_GEModule::Get()->GetCodecModule()->GetIccModule();

  if (blend_type) {
    bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
    if (src_alpha_scan) {
      for (int col = 0; col < pixel_count; col++) {
        int src_alpha = *src_alpha_scan++;
        if (clip_scan)
          src_alpha = clip_scan[col] * src_alpha / 255;

        if (src_alpha) {
          if (pIccTransform)
            pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
          else
            gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);

          if (bNonseparableBlend)
            gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
          else
            gray = Blend(blend_type, *dest_scan, gray);
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, src_alpha);
        }
        dest_scan++;
        src_scan += 3;
      }
    } else {
      for (int col = 0; col < pixel_count; col++) {
        int src_alpha = src_scan[3];
        if (clip_scan)
          src_alpha = clip_scan[col] * src_alpha / 255;

        if (src_alpha) {
          if (pIccTransform)
            pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
          else
            gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);

          if (bNonseparableBlend)
            gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
          else
            gray = Blend(blend_type, *dest_scan, gray);
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, src_alpha);
        }
        dest_scan++;
        src_scan += 4;
      }
    }
    return;
  }
  if (src_alpha_scan) {
    for (int col = 0; col < pixel_count; col++) {
      int src_alpha = *src_alpha_scan++;
      if (clip_scan)
        src_alpha = clip_scan[col] * src_alpha / 255;

      if (src_alpha) {
        if (pIccTransform)
          pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
        else
          gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);

        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, src_alpha);
      }
      dest_scan++;
      src_scan += 3;
    }
  } else {
    for (int col = 0; col < pixel_count; col++) {
      int src_alpha = src_scan[3];
      if (clip_scan)
        src_alpha = clip_scan[col] * src_alpha / 255;

      if (src_alpha) {
        if (pIccTransform)
          pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
        else
          gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);

        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, src_alpha);
      }
      dest_scan++;
      src_scan += 4;
    }
  }
}

void CompositeRow_Rgb2Gray(uint8_t* dest_scan,
                           const uint8_t* src_scan,
                           int src_Bpp,
                           int pixel_count,
                           int blend_type,
                           const uint8_t* clip_scan,
                           void* pIccTransform) {
  CCodec_IccModule* pIccModule = nullptr;
  uint8_t gray;
  if (pIccTransform) {
    pIccModule = CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  }
  if (blend_type) {
    bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
    for (int col = 0; col < pixel_count; col++) {
      if (pIccTransform) {
        pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
      } else {
        gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
      }
      if (bNonseparableBlend)
        gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
      else
        gray = Blend(blend_type, *dest_scan, gray);
      if (clip_scan && clip_scan[col] < 255) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, clip_scan[col]);
      } else {
        *dest_scan = gray;
      }
      dest_scan++;
      src_scan += src_Bpp;
    }
    return;
  }
  for (int col = 0; col < pixel_count; col++) {
    if (pIccTransform) {
      pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
    } else {
      gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
    }
    if (clip_scan && clip_scan[col] < 255) {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, clip_scan[col]);
    } else {
      *dest_scan = gray;
    }
    dest_scan++;
    src_scan += src_Bpp;
  }
}

void CompositeRow_Rgb2Graya(uint8_t* dest_scan,
                            const uint8_t* src_scan,
                            int src_Bpp,
                            int pixel_count,
                            int blend_type,
                            const uint8_t* clip_scan,
                            uint8_t* dest_alpha_scan,
                            void* pIccTransform) {
  CCodec_IccModule* pIccModule = nullptr;
  if (pIccTransform) {
    pIccModule = CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  }
  if (blend_type) {
    bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
    for (int col = 0; col < pixel_count; col++) {
      int back_alpha = *dest_alpha_scan;
      if (back_alpha == 0) {
        if (pIccTransform) {
          pIccModule->TranslateScanline(pIccTransform, dest_scan, src_scan, 1);
        } else {
          *dest_scan = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
        }
        dest_scan++;
        dest_alpha_scan++;
        src_scan += src_Bpp;
        continue;
      }
      int src_alpha = 255;
      if (clip_scan) {
        src_alpha = clip_scan[col];
      }
      if (src_alpha == 0) {
        dest_scan++;
        dest_alpha_scan++;
        src_scan += src_Bpp;
        continue;
      }
      uint8_t dest_alpha =
          back_alpha + src_alpha - back_alpha * src_alpha / 255;
      *dest_alpha_scan++ = dest_alpha;
      int alpha_ratio = src_alpha * 255 / dest_alpha;
      uint8_t gray;
      if (pIccTransform) {
        pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
      } else {
        gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
      }
      if (bNonseparableBlend)
        gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
      else
        gray = Blend(blend_type, *dest_scan, gray);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
      dest_scan++;
      src_scan += src_Bpp;
    }
    return;
  }
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha = 255;
    if (clip_scan) {
      src_alpha = clip_scan[col];
    }
    if (src_alpha == 255) {
      if (pIccTransform) {
        pIccModule->TranslateScanline(pIccTransform, dest_scan, src_scan, 1);
      } else {
        *dest_scan = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
      }
      dest_scan++;
      *dest_alpha_scan++ = 255;
      src_scan += src_Bpp;
      continue;
    }
    if (src_alpha == 0) {
      dest_scan++;
      dest_alpha_scan++;
      src_scan += src_Bpp;
      continue;
    }
    int back_alpha = *dest_alpha_scan;
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    *dest_alpha_scan++ = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    uint8_t gray;
    if (pIccTransform) {
      pIccModule->TranslateScanline(pIccTransform, &gray, src_scan, 1);
    } else {
      gray = FXRGB2GRAY(src_scan[2], src_scan[1], *src_scan);
    }
    *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
    dest_scan++;
    src_scan += src_Bpp;
  }
}

void CompositeRow_Argb2Argb(uint8_t* dest_scan,
                            const uint8_t* src_scan,
                            int pixel_count,
                            int blend_type,
                            const uint8_t* clip_scan,
                            uint8_t* dest_alpha_scan,
                            const uint8_t* src_alpha_scan) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  if (!dest_alpha_scan) {
    if (!src_alpha_scan) {
      uint8_t back_alpha = 0;
      for (int col = 0; col < pixel_count; col++) {
        back_alpha = dest_scan[3];
        if (back_alpha == 0) {
          if (clip_scan) {
            int src_alpha = clip_scan[col] * src_scan[3] / 255;
            FXARGB_SETDIB(dest_scan, (FXARGB_GETDIB(src_scan) & 0xffffff) |
                                         (src_alpha << 24));
          } else {
            FXARGB_COPY(dest_scan, src_scan);
          }
          dest_scan += 4;
          src_scan += 4;
          continue;
        }
        uint8_t src_alpha;
        if (clip_scan) {
          src_alpha = clip_scan[col] * src_scan[3] / 255;
        } else {
          src_alpha = src_scan[3];
        }
        if (src_alpha == 0) {
          dest_scan += 4;
          src_scan += 4;
          continue;
        }
        uint8_t dest_alpha =
            back_alpha + src_alpha - back_alpha * src_alpha / 255;
        dest_scan[3] = dest_alpha;
        int alpha_ratio = src_alpha * 255 / dest_alpha;
        if (bNonseparableBlend) {
          RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
        }
        for (int color = 0; color < 3; color++) {
          if (blend_type) {
            int blended = bNonseparableBlend
                              ? blended_colors[color]
                              : Blend(blend_type, *dest_scan, *src_scan);
            blended = FXDIB_ALPHA_MERGE(*src_scan, blended, back_alpha);
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, alpha_ratio);
          }
          dest_scan++;
          src_scan++;
        }
        dest_scan++;
        src_scan++;
      }
    } else {
      for (int col = 0; col < pixel_count; col++) {
        uint8_t back_alpha = dest_scan[3];
        if (back_alpha == 0) {
          if (clip_scan) {
            int src_alpha = clip_scan[col] * (*src_alpha_scan) / 255;
            FXARGB_SETDIB(dest_scan, FXARGB_MAKE((src_alpha << 24), src_scan[2],
                                                 src_scan[1], *src_scan));
          } else {
            FXARGB_SETDIB(dest_scan,
                          FXARGB_MAKE((*src_alpha_scan << 24), src_scan[2],
                                      src_scan[1], *src_scan));
          }
          dest_scan += 4;
          src_scan += 3;
          src_alpha_scan++;
          continue;
        }
        uint8_t src_alpha;
        if (clip_scan) {
          src_alpha = clip_scan[col] * (*src_alpha_scan++) / 255;
        } else {
          src_alpha = *src_alpha_scan++;
        }
        if (src_alpha == 0) {
          dest_scan += 4;
          src_scan += 3;
          continue;
        }
        uint8_t dest_alpha =
            back_alpha + src_alpha - back_alpha * src_alpha / 255;
        dest_scan[3] = dest_alpha;
        int alpha_ratio = src_alpha * 255 / dest_alpha;
        if (bNonseparableBlend) {
          RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
        }
        for (int color = 0; color < 3; color++) {
          if (blend_type) {
            int blended = bNonseparableBlend
                              ? blended_colors[color]
                              : Blend(blend_type, *dest_scan, *src_scan);
            blended = FXDIB_ALPHA_MERGE(*src_scan, blended, back_alpha);
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, alpha_ratio);
          }
          dest_scan++;
          src_scan++;
        }
        dest_scan++;
      }
    }
  } else {
    if (src_alpha_scan) {
      for (int col = 0; col < pixel_count; col++) {
        uint8_t back_alpha = *dest_alpha_scan;
        if (back_alpha == 0) {
          if (clip_scan) {
            int src_alpha = clip_scan[col] * (*src_alpha_scan) / 255;
            *dest_alpha_scan = src_alpha;
            *dest_scan++ = *src_scan++;
            *dest_scan++ = *src_scan++;
            *dest_scan++ = *src_scan++;
          } else {
            *dest_alpha_scan = *src_alpha_scan;
            *dest_scan++ = *src_scan++;
            *dest_scan++ = *src_scan++;
            *dest_scan++ = *src_scan++;
          }
          dest_alpha_scan++;
          src_alpha_scan++;
          continue;
        }
        uint8_t src_alpha;
        if (clip_scan) {
          src_alpha = clip_scan[col] * (*src_alpha_scan++) / 255;
        } else {
          src_alpha = *src_alpha_scan++;
        }
        if (src_alpha == 0) {
          dest_scan += 3;
          src_scan += 3;
          dest_alpha_scan++;
          continue;
        }
        uint8_t dest_alpha =
            back_alpha + src_alpha - back_alpha * src_alpha / 255;
        *dest_alpha_scan++ = dest_alpha;
        int alpha_ratio = src_alpha * 255 / dest_alpha;
        if (bNonseparableBlend) {
          RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
        }
        for (int color = 0; color < 3; color++) {
          if (blend_type) {
            int blended = bNonseparableBlend
                              ? blended_colors[color]
                              : Blend(blend_type, *dest_scan, *src_scan);
            blended = FXDIB_ALPHA_MERGE(*src_scan, blended, back_alpha);
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, alpha_ratio);
          }
          dest_scan++;
          src_scan++;
        }
      }
    } else {
      for (int col = 0; col < pixel_count; col++) {
        uint8_t back_alpha = *dest_alpha_scan;
        if (back_alpha == 0) {
          if (clip_scan) {
            int src_alpha = clip_scan[col] * src_scan[3] / 255;
            *dest_alpha_scan = src_alpha;
            *dest_scan++ = *src_scan++;
            *dest_scan++ = *src_scan++;
            *dest_scan++ = *src_scan++;
          } else {
            *dest_alpha_scan = src_scan[3];
            *dest_scan++ = *src_scan++;
            *dest_scan++ = *src_scan++;
            *dest_scan++ = *src_scan++;
          }
          dest_alpha_scan++;
          src_scan++;
          continue;
        }
        uint8_t src_alpha;
        if (clip_scan) {
          src_alpha = clip_scan[col] * src_scan[3] / 255;
        } else {
          src_alpha = src_scan[3];
        }
        if (src_alpha == 0) {
          dest_scan += 3;
          src_scan += 4;
          dest_alpha_scan++;
          continue;
        }
        uint8_t dest_alpha =
            back_alpha + src_alpha - back_alpha * src_alpha / 255;
        *dest_alpha_scan++ = dest_alpha;
        int alpha_ratio = src_alpha * 255 / dest_alpha;
        if (bNonseparableBlend) {
          RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
        }
        for (int color = 0; color < 3; color++) {
          if (blend_type) {
            int blended = bNonseparableBlend
                              ? blended_colors[color]
                              : Blend(blend_type, *dest_scan, *src_scan);
            blended = FXDIB_ALPHA_MERGE(*src_scan, blended, back_alpha);
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, alpha_ratio);
          }
          dest_scan++;
          src_scan++;
        }
        src_scan++;
      }
    }
  }
}

void CompositeRow_Rgb2Argb_Blend_NoClip(uint8_t* dest_scan,
                                        const uint8_t* src_scan,
                                        int width,
                                        int blend_type,
                                        int src_Bpp,
                                        uint8_t* dest_alpha_scan) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  int src_gap = src_Bpp - 3;
  if (dest_alpha_scan) {
    for (int col = 0; col < width; col++) {
      uint8_t back_alpha = *dest_alpha_scan;
      if (back_alpha == 0) {
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        *dest_alpha_scan++ = 0xff;
        src_scan += src_gap;
        continue;
      }
      *dest_alpha_scan++ = 0xff;
      if (bNonseparableBlend) {
        RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
      }
      for (int color = 0; color < 3; color++) {
        int src_color = *src_scan;
        int blended = bNonseparableBlend
                          ? blended_colors[color]
                          : Blend(blend_type, *dest_scan, src_color);
        *dest_scan = FXDIB_ALPHA_MERGE(src_color, blended, back_alpha);
        dest_scan++;
        src_scan++;
      }
      src_scan += src_gap;
    }
  } else {
    for (int col = 0; col < width; col++) {
      uint8_t back_alpha = dest_scan[3];
      if (back_alpha == 0) {
        if (src_Bpp == 4) {
          FXARGB_SETDIB(dest_scan, 0xff000000 | FXARGB_GETDIB(src_scan));
        } else {
          FXARGB_SETDIB(dest_scan, FXARGB_MAKE(0xff, src_scan[2], src_scan[1],
                                               src_scan[0]));
        }
        dest_scan += 4;
        src_scan += src_Bpp;
        continue;
      }
      dest_scan[3] = 0xff;
      if (bNonseparableBlend) {
        RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
      }
      for (int color = 0; color < 3; color++) {
        int src_color = *src_scan;
        int blended = bNonseparableBlend
                          ? blended_colors[color]
                          : Blend(blend_type, *dest_scan, src_color);
        *dest_scan = FXDIB_ALPHA_MERGE(src_color, blended, back_alpha);
        dest_scan++;
        src_scan++;
      }
      dest_scan++;
      src_scan += src_gap;
    }
  }
}

void CompositeRow_Rgb2Argb_Blend_Clip(uint8_t* dest_scan,
                                      const uint8_t* src_scan,
                                      int width,
                                      int blend_type,
                                      int src_Bpp,
                                      const uint8_t* clip_scan,
                                      uint8_t* dest_alpha_scan) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  int src_gap = src_Bpp - 3;
  if (dest_alpha_scan) {
    for (int col = 0; col < width; col++) {
      int src_alpha = *clip_scan++;
      uint8_t back_alpha = *dest_alpha_scan;
      if (back_alpha == 0) {
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        src_scan += src_gap;
        dest_alpha_scan++;
        continue;
      }
      if (src_alpha == 0) {
        dest_scan += 3;
        dest_alpha_scan++;
        src_scan += src_Bpp;
        continue;
      }
      uint8_t dest_alpha =
          back_alpha + src_alpha - back_alpha * src_alpha / 255;
      *dest_alpha_scan++ = dest_alpha;
      int alpha_ratio = src_alpha * 255 / dest_alpha;
      if (bNonseparableBlend) {
        RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
      }
      for (int color = 0; color < 3; color++) {
        int src_color = *src_scan;
        int blended = bNonseparableBlend
                          ? blended_colors[color]
                          : Blend(blend_type, *dest_scan, src_color);
        blended = FXDIB_ALPHA_MERGE(src_color, blended, back_alpha);
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
        dest_scan++;
        src_scan++;
      }
      src_scan += src_gap;
    }
  } else {
    for (int col = 0; col < width; col++) {
      int src_alpha = *clip_scan++;
      uint8_t back_alpha = dest_scan[3];
      if (back_alpha == 0) {
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        src_scan += src_gap;
        dest_scan++;
        continue;
      }
      if (src_alpha == 0) {
        dest_scan += 4;
        src_scan += src_Bpp;
        continue;
      }
      uint8_t dest_alpha =
          back_alpha + src_alpha - back_alpha * src_alpha / 255;
      dest_scan[3] = dest_alpha;
      int alpha_ratio = src_alpha * 255 / dest_alpha;
      if (bNonseparableBlend) {
        RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
      }
      for (int color = 0; color < 3; color++) {
        int src_color = *src_scan;
        int blended = bNonseparableBlend
                          ? blended_colors[color]
                          : Blend(blend_type, *dest_scan, src_color);
        blended = FXDIB_ALPHA_MERGE(src_color, blended, back_alpha);
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
        dest_scan++;
        src_scan++;
      }
      dest_scan++;
      src_scan += src_gap;
    }
  }
}

void CompositeRow_Rgb2Argb_NoBlend_Clip(uint8_t* dest_scan,
                                        const uint8_t* src_scan,
                                        int width,
                                        int src_Bpp,
                                        const uint8_t* clip_scan,
                                        uint8_t* dest_alpha_scan) {
  int src_gap = src_Bpp - 3;
  if (dest_alpha_scan) {
    for (int col = 0; col < width; col++) {
      int src_alpha = clip_scan[col];
      if (src_alpha == 255) {
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        *dest_alpha_scan++ = 255;
        src_scan += src_gap;
        continue;
      }
      if (src_alpha == 0) {
        dest_scan += 3;
        dest_alpha_scan++;
        src_scan += src_Bpp;
        continue;
      }
      int back_alpha = *dest_alpha_scan;
      uint8_t dest_alpha =
          back_alpha + src_alpha - back_alpha * src_alpha / 255;
      *dest_alpha_scan++ = dest_alpha;
      int alpha_ratio = src_alpha * 255 / dest_alpha;
      for (int color = 0; color < 3; color++) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, alpha_ratio);
        dest_scan++;
        src_scan++;
      }
      src_scan += src_gap;
    }
  } else {
    for (int col = 0; col < width; col++) {
      int src_alpha = clip_scan[col];
      if (src_alpha == 255) {
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        *dest_scan++ = 255;
        src_scan += src_gap;
        continue;
      }
      if (src_alpha == 0) {
        dest_scan += 4;
        src_scan += src_Bpp;
        continue;
      }
      int back_alpha = dest_scan[3];
      uint8_t dest_alpha =
          back_alpha + src_alpha - back_alpha * src_alpha / 255;
      dest_scan[3] = dest_alpha;
      int alpha_ratio = src_alpha * 255 / dest_alpha;
      for (int color = 0; color < 3; color++) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, alpha_ratio);
        dest_scan++;
        src_scan++;
      }
      dest_scan++;
      src_scan += src_gap;
    }
  }
}

void CompositeRow_Rgb2Argb_NoBlend_NoClip(uint8_t* dest_scan,
                                          const uint8_t* src_scan,
                                          int width,
                                          int src_Bpp,
                                          uint8_t* dest_alpha_scan) {
  if (dest_alpha_scan) {
    int src_gap = src_Bpp - 3;
    for (int col = 0; col < width; col++) {
      *dest_scan++ = *src_scan++;
      *dest_scan++ = *src_scan++;
      *dest_scan++ = *src_scan++;
      *dest_alpha_scan++ = 0xff;
      src_scan += src_gap;
    }
  } else {
    for (int col = 0; col < width; col++) {
      if (src_Bpp == 4) {
        FXARGB_SETDIB(dest_scan, 0xff000000 | FXARGB_GETDIB(src_scan));
      } else {
        FXARGB_SETDIB(dest_scan,
                      FXARGB_MAKE(0xff, src_scan[2], src_scan[1], src_scan[0]));
      }
      dest_scan += 4;
      src_scan += src_Bpp;
    }
  }
}

void CompositeRow_Argb2Rgb_Blend(uint8_t* dest_scan,
                                 const uint8_t* src_scan,
                                 int width,
                                 int blend_type,
                                 int dest_Bpp,
                                 const uint8_t* clip_scan,
                                 const uint8_t* src_alpha_scan) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  int dest_gap = dest_Bpp - 3;
  if (src_alpha_scan) {
    for (int col = 0; col < width; col++) {
      uint8_t src_alpha;
      if (clip_scan) {
        src_alpha = (*src_alpha_scan++) * (*clip_scan++) / 255;
      } else {
        src_alpha = *src_alpha_scan++;
      }
      if (src_alpha == 0) {
        dest_scan += dest_Bpp;
        src_scan += 3;
        continue;
      }
      if (bNonseparableBlend) {
        RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
      }
      for (int color = 0; color < 3; color++) {
        int back_color = *dest_scan;
        int blended = bNonseparableBlend
                          ? blended_colors[color]
                          : Blend(blend_type, back_color, *src_scan);
        *dest_scan = FXDIB_ALPHA_MERGE(back_color, blended, src_alpha);
        dest_scan++;
        src_scan++;
      }
      dest_scan += dest_gap;
    }
  } else {
    for (int col = 0; col < width; col++) {
      uint8_t src_alpha;
      if (clip_scan) {
        src_alpha = src_scan[3] * (*clip_scan++) / 255;
      } else {
        src_alpha = src_scan[3];
      }
      if (src_alpha == 0) {
        dest_scan += dest_Bpp;
        src_scan += 4;
        continue;
      }
      if (bNonseparableBlend) {
        RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
      }
      for (int color = 0; color < 3; color++) {
        int back_color = *dest_scan;
        int blended = bNonseparableBlend
                          ? blended_colors[color]
                          : Blend(blend_type, back_color, *src_scan);
        *dest_scan = FXDIB_ALPHA_MERGE(back_color, blended, src_alpha);
        dest_scan++;
        src_scan++;
      }
      dest_scan += dest_gap;
      src_scan++;
    }
  }
}

void CompositeRow_Argb2Rgb_NoBlend(uint8_t* dest_scan,
                                   const uint8_t* src_scan,
                                   int width,
                                   int dest_Bpp,
                                   const uint8_t* clip_scan,
                                   const uint8_t* src_alpha_scan) {
  int dest_gap = dest_Bpp - 3;
  if (src_alpha_scan) {
    for (int col = 0; col < width; col++) {
      uint8_t src_alpha;
      if (clip_scan) {
        src_alpha = (*src_alpha_scan++) * (*clip_scan++) / 255;
      } else {
        src_alpha = *src_alpha_scan++;
      }
      if (src_alpha == 255) {
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        dest_scan += dest_gap;
        continue;
      }
      if (src_alpha == 0) {
        dest_scan += dest_Bpp;
        src_scan += 3;
        continue;
      }
      for (int color = 0; color < 3; color++) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, src_alpha);
        dest_scan++;
        src_scan++;
      }
      dest_scan += dest_gap;
    }
  } else {
    for (int col = 0; col < width; col++) {
      uint8_t src_alpha;
      if (clip_scan) {
        src_alpha = src_scan[3] * (*clip_scan++) / 255;
      } else {
        src_alpha = src_scan[3];
      }
      if (src_alpha == 255) {
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        *dest_scan++ = *src_scan++;
        dest_scan += dest_gap;
        src_scan++;
        continue;
      }
      if (src_alpha == 0) {
        dest_scan += dest_Bpp;
        src_scan += 4;
        continue;
      }
      for (int color = 0; color < 3; color++) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, src_alpha);
        dest_scan++;
        src_scan++;
      }
      dest_scan += dest_gap;
      src_scan++;
    }
  }
}

void CompositeRow_Rgb2Rgb_Blend_NoClip(uint8_t* dest_scan,
                                       const uint8_t* src_scan,
                                       int width,
                                       int blend_type,
                                       int dest_Bpp,
                                       int src_Bpp) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  int dest_gap = dest_Bpp - 3;
  int src_gap = src_Bpp - 3;
  for (int col = 0; col < width; col++) {
    if (bNonseparableBlend) {
      RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
    }
    for (int color = 0; color < 3; color++) {
      int back_color = *dest_scan;
      int src_color = *src_scan;
      int blended = bNonseparableBlend
                        ? blended_colors[color]
                        : Blend(blend_type, back_color, src_color);
      *dest_scan = blended;
      dest_scan++;
      src_scan++;
    }
    dest_scan += dest_gap;
    src_scan += src_gap;
  }
}

void CompositeRow_Rgb2Rgb_Blend_Clip(uint8_t* dest_scan,
                                     const uint8_t* src_scan,
                                     int width,
                                     int blend_type,
                                     int dest_Bpp,
                                     int src_Bpp,
                                     const uint8_t* clip_scan) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  int dest_gap = dest_Bpp - 3;
  int src_gap = src_Bpp - 3;
  for (int col = 0; col < width; col++) {
    uint8_t src_alpha = *clip_scan++;
    if (src_alpha == 0) {
      dest_scan += dest_Bpp;
      src_scan += src_Bpp;
      continue;
    }
    if (bNonseparableBlend) {
      RGB_Blend(blend_type, src_scan, dest_scan, blended_colors);
    }
    for (int color = 0; color < 3; color++) {
      int src_color = *src_scan;
      int back_color = *dest_scan;
      int blended = bNonseparableBlend
                        ? blended_colors[color]
                        : Blend(blend_type, back_color, src_color);
      *dest_scan = FXDIB_ALPHA_MERGE(back_color, blended, src_alpha);
      dest_scan++;
      src_scan++;
    }
    dest_scan += dest_gap;
    src_scan += src_gap;
  }
}

void CompositeRow_Rgb2Rgb_NoBlend_NoClip(uint8_t* dest_scan,
                                         const uint8_t* src_scan,
                                         int width,
                                         int dest_Bpp,
                                         int src_Bpp) {
  if (dest_Bpp == src_Bpp) {
    FXSYS_memcpy(dest_scan, src_scan, width * dest_Bpp);
    return;
  }
  for (int col = 0; col < width; col++) {
    dest_scan[0] = src_scan[0];
    dest_scan[1] = src_scan[1];
    dest_scan[2] = src_scan[2];
    dest_scan += dest_Bpp;
    src_scan += src_Bpp;
  }
}

void CompositeRow_Rgb2Rgb_NoBlend_Clip(uint8_t* dest_scan,
                                       const uint8_t* src_scan,
                                       int width,
                                       int dest_Bpp,
                                       int src_Bpp,
                                       const uint8_t* clip_scan) {
  for (int col = 0; col < width; col++) {
    int src_alpha = clip_scan[col];
    if (src_alpha == 255) {
      dest_scan[0] = src_scan[0];
      dest_scan[1] = src_scan[1];
      dest_scan[2] = src_scan[2];
    } else if (src_alpha) {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, src_alpha);
      dest_scan++;
      src_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, src_alpha);
      dest_scan++;
      src_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_scan, src_alpha);
      dest_scan += dest_Bpp - 2;
      src_scan += src_Bpp - 2;
      continue;
    }
    dest_scan += dest_Bpp;
    src_scan += src_Bpp;
  }
}

void CompositeRow_Argb2Argb_Transform(uint8_t* dest_scan,
                                      const uint8_t* src_scan,
                                      int pixel_count,
                                      int blend_type,
                                      const uint8_t* clip_scan,
                                      uint8_t* dest_alpha_scan,
                                      const uint8_t* src_alpha_scan,
                                      uint8_t* src_cache_scan,
                                      void* pIccTransform) {
  uint8_t* dp = src_cache_scan;
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_alpha_scan) {
    if (dest_alpha_scan) {
      pIccModule->TranslateScanline(pIccTransform, dp, src_scan, pixel_count);
    } else {
      for (int col = 0; col < pixel_count; col++) {
        pIccModule->TranslateScanline(pIccTransform, dp, src_scan, 1);
        dp[3] = *src_alpha_scan++;
        src_scan += 3;
        dp += 4;
      }
      src_alpha_scan = nullptr;
    }
  } else {
    if (dest_alpha_scan) {
      int blended_colors[3];
      bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
      for (int col = 0; col < pixel_count; col++) {
        pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                      1);
        uint8_t back_alpha = *dest_alpha_scan;
        if (back_alpha == 0) {
          if (clip_scan) {
            int src_alpha = clip_scan[col] * src_scan[3] / 255;
            *dest_alpha_scan = src_alpha;
            *dest_scan++ = *src_cache_scan++;
            *dest_scan++ = *src_cache_scan++;
            *dest_scan++ = *src_cache_scan++;
          } else {
            *dest_alpha_scan = src_scan[3];
            *dest_scan++ = *src_cache_scan++;
            *dest_scan++ = *src_cache_scan++;
            *dest_scan++ = *src_cache_scan++;
          }
          dest_alpha_scan++;
          src_scan += 4;
          continue;
        }
        uint8_t src_alpha;
        if (clip_scan) {
          src_alpha = clip_scan[col] * src_scan[3] / 255;
        } else {
          src_alpha = src_scan[3];
        }
        src_scan += 4;
        if (src_alpha == 0) {
          dest_scan += 3;
          src_cache_scan += 3;
          dest_alpha_scan++;
          continue;
        }
        uint8_t dest_alpha =
            back_alpha + src_alpha - back_alpha * src_alpha / 255;
        *dest_alpha_scan++ = dest_alpha;
        int alpha_ratio = src_alpha * 255 / dest_alpha;
        if (bNonseparableBlend) {
          RGB_Blend(blend_type, src_cache_scan, dest_scan, blended_colors);
        }
        for (int color = 0; color < 3; color++) {
          if (blend_type) {
            int blended = bNonseparableBlend
                              ? blended_colors[color]
                              : Blend(blend_type, *dest_scan, *src_cache_scan);
            blended = FXDIB_ALPHA_MERGE(*src_cache_scan, blended, back_alpha);
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
          } else {
            *dest_scan =
                FXDIB_ALPHA_MERGE(*dest_scan, *src_cache_scan, alpha_ratio);
          }
          dest_scan++;
          src_cache_scan++;
        }
      }
      return;
    }
    for (int col = 0; col < pixel_count; col++) {
      pIccModule->TranslateScanline(pIccTransform, dp, src_scan, 1);
      dp[3] = src_scan[3];
      src_scan += 4;
      dp += 4;
    }
  }
  CompositeRow_Argb2Argb(dest_scan, src_cache_scan, pixel_count, blend_type,
                         clip_scan, dest_alpha_scan, src_alpha_scan);
}

void CompositeRow_Rgb2Argb_Blend_NoClip_Transform(uint8_t* dest_scan,
                                                  const uint8_t* src_scan,
                                                  int width,
                                                  int blend_type,
                                                  int src_Bpp,
                                                  uint8_t* dest_alpha_scan,
                                                  uint8_t* src_cache_scan,
                                                  void* pIccTransform) {
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_Bpp == 3) {
    pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                  width);
  } else {
    uint8_t* dp = src_cache_scan;
    for (int col = 0; col < width; col++) {
      pIccModule->TranslateScanline(pIccTransform, dp, src_scan, 1);
      src_scan += 4;
      dp += 3;
    }
  }
  CompositeRow_Rgb2Argb_Blend_NoClip(dest_scan, src_cache_scan, width,
                                     blend_type, 3, dest_alpha_scan);
}

void CompositeRow_Rgb2Argb_Blend_Clip_Transform(uint8_t* dest_scan,
                                                const uint8_t* src_scan,
                                                int width,
                                                int blend_type,
                                                int src_Bpp,
                                                const uint8_t* clip_scan,
                                                uint8_t* dest_alpha_scan,
                                                uint8_t* src_cache_scan,
                                                void* pIccTransform) {
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_Bpp == 3) {
    pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                  width);
  } else {
    uint8_t* dp = src_cache_scan;
    for (int col = 0; col < width; col++) {
      pIccModule->TranslateScanline(pIccTransform, dp, src_scan, 1);
      src_scan += 4;
      dp += 3;
    }
  }
  CompositeRow_Rgb2Argb_Blend_Clip(dest_scan, src_cache_scan, width, blend_type,
                                   3, clip_scan, dest_alpha_scan);
}

void CompositeRow_Rgb2Argb_NoBlend_Clip_Transform(uint8_t* dest_scan,
                                                  const uint8_t* src_scan,
                                                  int width,
                                                  int src_Bpp,
                                                  const uint8_t* clip_scan,
                                                  uint8_t* dest_alpha_scan,
                                                  uint8_t* src_cache_scan,
                                                  void* pIccTransform) {
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_Bpp == 3) {
    pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                  width);
  } else {
    uint8_t* dp = src_cache_scan;
    for (int col = 0; col < width; col++) {
      pIccModule->TranslateScanline(pIccTransform, dp, src_scan, 1);
      src_scan += 4;
      dp += 3;
    }
  }
  CompositeRow_Rgb2Argb_NoBlend_Clip(dest_scan, src_cache_scan, width, 3,
                                     clip_scan, dest_alpha_scan);
}

void CompositeRow_Rgb2Argb_NoBlend_NoClip_Transform(uint8_t* dest_scan,
                                                    const uint8_t* src_scan,
                                                    int width,
                                                    int src_Bpp,
                                                    uint8_t* dest_alpha_scan,
                                                    uint8_t* src_cache_scan,
                                                    void* pIccTransform) {
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_Bpp == 3) {
    pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                  width);
  } else {
    uint8_t* dp = src_cache_scan;
    for (int col = 0; col < width; col++) {
      pIccModule->TranslateScanline(pIccTransform, dp, src_scan, 1);
      src_scan += 4;
      dp += 3;
    }
  }
  CompositeRow_Rgb2Argb_NoBlend_NoClip(dest_scan, src_cache_scan, width, 3,
                                       dest_alpha_scan);
}

void CompositeRow_Argb2Rgb_Blend_Transform(uint8_t* dest_scan,
                                           const uint8_t* src_scan,
                                           int width,
                                           int blend_type,
                                           int dest_Bpp,
                                           const uint8_t* clip_scan,
                                           const uint8_t* src_alpha_scan,
                                           uint8_t* src_cache_scan,
                                           void* pIccTransform) {
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_alpha_scan) {
    pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                  width);
  } else {
    int blended_colors[3];
    bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
    int dest_gap = dest_Bpp - 3;
    for (int col = 0; col < width; col++) {
      pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan, 1);
      uint8_t src_alpha;
      if (clip_scan) {
        src_alpha = src_scan[3] * (*clip_scan++) / 255;
      } else {
        src_alpha = src_scan[3];
      }
      src_scan += 4;
      if (src_alpha == 0) {
        dest_scan += dest_Bpp;
        src_cache_scan += 3;
        continue;
      }
      if (bNonseparableBlend) {
        RGB_Blend(blend_type, src_cache_scan, dest_scan, blended_colors);
      }
      for (int color = 0; color < 3; color++) {
        int back_color = *dest_scan;
        int blended = bNonseparableBlend
                          ? blended_colors[color]
                          : Blend(blend_type, back_color, *src_cache_scan);
        *dest_scan = FXDIB_ALPHA_MERGE(back_color, blended, src_alpha);
        dest_scan++;
        src_cache_scan++;
      }
      dest_scan += dest_gap;
    }
    return;
  }
  CompositeRow_Argb2Rgb_Blend(dest_scan, src_cache_scan, width, blend_type,
                              dest_Bpp, clip_scan, src_alpha_scan);
}

void CompositeRow_Argb2Rgb_NoBlend_Transform(uint8_t* dest_scan,
                                             const uint8_t* src_scan,
                                             int width,
                                             int dest_Bpp,
                                             const uint8_t* clip_scan,
                                             const uint8_t* src_alpha_scan,
                                             uint8_t* src_cache_scan,
                                             void* pIccTransform) {
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_alpha_scan) {
    pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                  width);
  } else {
    int dest_gap = dest_Bpp - 3;
    for (int col = 0; col < width; col++) {
      pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan, 1);
      uint8_t src_alpha;
      if (clip_scan) {
        src_alpha = src_scan[3] * (*clip_scan++) / 255;
      } else {
        src_alpha = src_scan[3];
      }
      src_scan += 4;
      if (src_alpha == 255) {
        *dest_scan++ = *src_cache_scan++;
        *dest_scan++ = *src_cache_scan++;
        *dest_scan++ = *src_cache_scan++;
        dest_scan += dest_gap;
        continue;
      }
      if (src_alpha == 0) {
        dest_scan += dest_Bpp;
        src_cache_scan += 3;
        continue;
      }
      for (int color = 0; color < 3; color++) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, *src_cache_scan, src_alpha);
        dest_scan++;
        src_cache_scan++;
      }
      dest_scan += dest_gap;
    }
    return;
  }
  CompositeRow_Argb2Rgb_NoBlend(dest_scan, src_cache_scan, width, dest_Bpp,
                                clip_scan, src_alpha_scan);
}

void CompositeRow_Rgb2Rgb_Blend_NoClip_Transform(uint8_t* dest_scan,
                                                 const uint8_t* src_scan,
                                                 int width,
                                                 int blend_type,
                                                 int dest_Bpp,
                                                 int src_Bpp,
                                                 uint8_t* src_cache_scan,
                                                 void* pIccTransform) {
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_Bpp == 3) {
    pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                  width);
  } else {
    uint8_t* dp = src_cache_scan;
    for (int col = 0; col < width; col++) {
      pIccModule->TranslateScanline(pIccTransform, dp, src_scan, 1);
      src_scan += 4;
      dp += 3;
    }
  }
  CompositeRow_Rgb2Rgb_Blend_NoClip(dest_scan, src_cache_scan, width,
                                    blend_type, dest_Bpp, 3);
}

void CompositeRow_Rgb2Rgb_Blend_Clip_Transform(uint8_t* dest_scan,
                                               const uint8_t* src_scan,
                                               int width,
                                               int blend_type,
                                               int dest_Bpp,
                                               int src_Bpp,
                                               const uint8_t* clip_scan,
                                               uint8_t* src_cache_scan,
                                               void* pIccTransform) {
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_Bpp == 3) {
    pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                  width);
  } else {
    uint8_t* dp = src_cache_scan;
    for (int col = 0; col < width; col++) {
      pIccModule->TranslateScanline(pIccTransform, dp, src_scan, 1);
      src_scan += 4;
      dp += 3;
    }
  }
  CompositeRow_Rgb2Rgb_Blend_Clip(dest_scan, src_cache_scan, width, blend_type,
                                  dest_Bpp, 3, clip_scan);
}

void CompositeRow_Rgb2Rgb_NoBlend_NoClip_Transform(uint8_t* dest_scan,
                                                   const uint8_t* src_scan,
                                                   int width,
                                                   int dest_Bpp,
                                                   int src_Bpp,
                                                   uint8_t* src_cache_scan,
                                                   void* pIccTransform) {
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_Bpp == 3) {
    pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                  width);
  } else {
    uint8_t* dp = src_cache_scan;
    for (int col = 0; col < width; col++) {
      pIccModule->TranslateScanline(pIccTransform, dp, src_scan, 1);
      src_scan += 4;
      dp += 3;
    }
  }
  CompositeRow_Rgb2Rgb_NoBlend_NoClip(dest_scan, src_cache_scan, width,
                                      dest_Bpp, 3);
}

void CompositeRow_Rgb2Rgb_NoBlend_Clip_Transform(uint8_t* dest_scan,
                                                 const uint8_t* src_scan,
                                                 int width,
                                                 int dest_Bpp,
                                                 int src_Bpp,
                                                 const uint8_t* clip_scan,
                                                 uint8_t* src_cache_scan,
                                                 void* pIccTransform) {
  CCodec_IccModule* pIccModule =
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  if (src_Bpp == 3) {
    pIccModule->TranslateScanline(pIccTransform, src_cache_scan, src_scan,
                                  width);
  } else {
    uint8_t* dp = src_cache_scan;
    for (int col = 0; col < width; col++) {
      pIccModule->TranslateScanline(pIccTransform, dp, src_scan, 1);
      src_scan += 4;
      dp += 3;
    }
  }
  CompositeRow_Rgb2Rgb_NoBlend_Clip(dest_scan, src_cache_scan, width, dest_Bpp,
                                    3, clip_scan);
}

void CompositeRow_8bppPal2Gray(uint8_t* dest_scan,
                               const uint8_t* src_scan,
                               const uint8_t* pPalette,
                               int pixel_count,
                               int blend_type,
                               const uint8_t* clip_scan,
                               const uint8_t* src_alpha_scan) {
  if (src_alpha_scan) {
    if (blend_type) {
      bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
      for (int col = 0; col < pixel_count; col++) {
        uint8_t gray = pPalette[*src_scan];
        int src_alpha = *src_alpha_scan++;
        if (clip_scan) {
          src_alpha = clip_scan[col] * src_alpha / 255;
        }
        if (bNonseparableBlend)
          gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
        else
          gray = Blend(blend_type, *dest_scan, gray);
        if (src_alpha) {
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, src_alpha);
        } else {
          *dest_scan = gray;
        }
        dest_scan++;
        src_scan++;
      }
      return;
    }
    for (int col = 0; col < pixel_count; col++) {
      uint8_t gray = pPalette[*src_scan];
      int src_alpha = *src_alpha_scan++;
      if (clip_scan) {
        src_alpha = clip_scan[col] * src_alpha / 255;
      }
      if (src_alpha) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, src_alpha);
      } else {
        *dest_scan = gray;
      }
      dest_scan++;
      src_scan++;
    }
  } else {
    if (blend_type) {
      bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
      for (int col = 0; col < pixel_count; col++) {
        uint8_t gray = pPalette[*src_scan];
        if (bNonseparableBlend)
          gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
        else
          gray = Blend(blend_type, *dest_scan, gray);
        if (clip_scan && clip_scan[col] < 255) {
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, clip_scan[col]);
        } else {
          *dest_scan = gray;
        }
        dest_scan++;
        src_scan++;
      }
      return;
    }
    for (int col = 0; col < pixel_count; col++) {
      uint8_t gray = pPalette[*src_scan];
      if (clip_scan && clip_scan[col] < 255) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, clip_scan[col]);
      } else {
        *dest_scan = gray;
      }
      dest_scan++;
      src_scan++;
    }
  }
}

void CompositeRow_8bppPal2Graya(uint8_t* dest_scan,
                                const uint8_t* src_scan,
                                const uint8_t* pPalette,
                                int pixel_count,
                                int blend_type,
                                const uint8_t* clip_scan,
                                uint8_t* dest_alpha_scan,
                                const uint8_t* src_alpha_scan) {
  if (src_alpha_scan) {
    if (blend_type) {
      bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
      for (int col = 0; col < pixel_count; col++) {
        uint8_t gray = pPalette[*src_scan];
        src_scan++;
        uint8_t back_alpha = *dest_alpha_scan;
        if (back_alpha == 0) {
          int src_alpha = *src_alpha_scan++;
          if (clip_scan) {
            src_alpha = clip_scan[col] * src_alpha / 255;
          }
          if (src_alpha) {
            *dest_scan = gray;
            *dest_alpha_scan = src_alpha;
          }
          dest_scan++;
          dest_alpha_scan++;
          continue;
        }
        uint8_t src_alpha = *src_alpha_scan++;
        if (clip_scan) {
          src_alpha = clip_scan[col] * src_alpha / 255;
        }
        if (src_alpha == 0) {
          dest_scan++;
          dest_alpha_scan++;
          continue;
        }
        *dest_alpha_scan =
            back_alpha + src_alpha - back_alpha * src_alpha / 255;
        int alpha_ratio = src_alpha * 255 / (*dest_alpha_scan);
        if (bNonseparableBlend)
          gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
        else
          gray = Blend(blend_type, *dest_scan, gray);
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
        dest_alpha_scan++;
        dest_scan++;
      }
      return;
    }
    for (int col = 0; col < pixel_count; col++) {
      uint8_t gray = pPalette[*src_scan];
      src_scan++;
      uint8_t back_alpha = *dest_alpha_scan;
      if (back_alpha == 0) {
        int src_alpha = *src_alpha_scan++;
        if (clip_scan) {
          src_alpha = clip_scan[col] * src_alpha / 255;
        }
        if (src_alpha) {
          *dest_scan = gray;
          *dest_alpha_scan = src_alpha;
        }
        dest_scan++;
        dest_alpha_scan++;
        continue;
      }
      uint8_t src_alpha = *src_alpha_scan++;
      if (clip_scan) {
        src_alpha = clip_scan[col] * src_alpha / 255;
      }
      if (src_alpha == 0) {
        dest_scan++;
        dest_alpha_scan++;
        continue;
      }
      *dest_alpha_scan = back_alpha + src_alpha - back_alpha * src_alpha / 255;
      int alpha_ratio = src_alpha * 255 / (*dest_alpha_scan);
      dest_alpha_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
      dest_scan++;
    }
  } else {
    if (blend_type) {
      bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
      for (int col = 0; col < pixel_count; col++) {
        uint8_t gray = pPalette[*src_scan];
        src_scan++;
        if (!clip_scan || clip_scan[col] == 255) {
          *dest_scan++ = gray;
          *dest_alpha_scan++ = 255;
          continue;
        }
        int src_alpha = clip_scan[col];
        if (src_alpha == 0) {
          dest_scan++;
          dest_alpha_scan++;
          continue;
        }
        int back_alpha = *dest_alpha_scan;
        uint8_t dest_alpha =
            back_alpha + src_alpha - back_alpha * src_alpha / 255;
        *dest_alpha_scan++ = dest_alpha;
        int alpha_ratio = src_alpha * 255 / dest_alpha;
        if (bNonseparableBlend)
          gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
        else
          gray = Blend(blend_type, *dest_scan, gray);
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
        dest_scan++;
      }
      return;
    }
    for (int col = 0; col < pixel_count; col++) {
      uint8_t gray = pPalette[*src_scan];
      src_scan++;
      if (!clip_scan || clip_scan[col] == 255) {
        *dest_scan++ = gray;
        *dest_alpha_scan++ = 255;
        continue;
      }
      int src_alpha = clip_scan[col];
      if (src_alpha == 0) {
        dest_scan++;
        dest_alpha_scan++;
        continue;
      }
      int back_alpha = *dest_alpha_scan;
      uint8_t dest_alpha =
          back_alpha + src_alpha - back_alpha * src_alpha / 255;
      *dest_alpha_scan++ = dest_alpha;
      int alpha_ratio = src_alpha * 255 / dest_alpha;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
      dest_scan++;
    }
  }
}

void CompositeRow_1bppPal2Gray(uint8_t* dest_scan,
                               const uint8_t* src_scan,
                               int src_left,
                               const uint8_t* pPalette,
                               int pixel_count,
                               int blend_type,
                               const uint8_t* clip_scan) {
  int reset_gray = pPalette[0];
  int set_gray = pPalette[1];
  if (blend_type) {
    bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
    for (int col = 0; col < pixel_count; col++) {
      uint8_t gray =
          (src_scan[(col + src_left) / 8] & (1 << (7 - (col + src_left) % 8)))
              ? set_gray
              : reset_gray;
      if (bNonseparableBlend)
        gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
      else
        gray = Blend(blend_type, *dest_scan, gray);
      if (clip_scan && clip_scan[col] < 255) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, clip_scan[col]);
      } else {
        *dest_scan = gray;
      }
      dest_scan++;
    }
    return;
  }
  for (int col = 0; col < pixel_count; col++) {
    uint8_t gray =
        (src_scan[(col + src_left) / 8] & (1 << (7 - (col + src_left) % 8)))
            ? set_gray
            : reset_gray;
    if (clip_scan && clip_scan[col] < 255) {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, clip_scan[col]);
    } else {
      *dest_scan = gray;
    }
    dest_scan++;
  }
}

void CompositeRow_1bppPal2Graya(uint8_t* dest_scan,
                                const uint8_t* src_scan,
                                int src_left,
                                const uint8_t* pPalette,
                                int pixel_count,
                                int blend_type,
                                const uint8_t* clip_scan,
                                uint8_t* dest_alpha_scan) {
  int reset_gray = pPalette[0];
  int set_gray = pPalette[1];
  if (blend_type) {
    bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
    for (int col = 0; col < pixel_count; col++) {
      uint8_t gray =
          (src_scan[(col + src_left) / 8] & (1 << (7 - (col + src_left) % 8)))
              ? set_gray
              : reset_gray;
      if (!clip_scan || clip_scan[col] == 255) {
        *dest_scan++ = gray;
        *dest_alpha_scan++ = 255;
        continue;
      }
      int src_alpha = clip_scan[col];
      if (src_alpha == 0) {
        dest_scan++;
        dest_alpha_scan++;
        continue;
      }
      int back_alpha = *dest_alpha_scan;
      uint8_t dest_alpha =
          back_alpha + src_alpha - back_alpha * src_alpha / 255;
      *dest_alpha_scan++ = dest_alpha;
      int alpha_ratio = src_alpha * 255 / dest_alpha;
      if (bNonseparableBlend)
        gray = blend_type == FXDIB_BLEND_LUMINOSITY ? gray : *dest_scan;
      else
        gray = Blend(blend_type, *dest_scan, gray);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
      dest_scan++;
    }
    return;
  }
  for (int col = 0; col < pixel_count; col++) {
    uint8_t gray =
        (src_scan[(col + src_left) / 8] & (1 << (7 - (col + src_left) % 8)))
            ? set_gray
            : reset_gray;
    if (!clip_scan || clip_scan[col] == 255) {
      *dest_scan++ = gray;
      *dest_alpha_scan++ = 255;
      continue;
    }
    int src_alpha = clip_scan[col];
    if (src_alpha == 0) {
      dest_scan++;
      dest_alpha_scan++;
      continue;
    }
    int back_alpha = *dest_alpha_scan;
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    *dest_alpha_scan++ = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, alpha_ratio);
    dest_scan++;
  }
}

void CompositeRow_8bppRgb2Rgb_NoBlend(uint8_t* dest_scan,
                                      const uint8_t* src_scan,
                                      uint32_t* pPalette,
                                      int pixel_count,
                                      int DestBpp,
                                      const uint8_t* clip_scan,
                                      const uint8_t* src_alpha_scan) {
  if (src_alpha_scan) {
    int dest_gap = DestBpp - 3;
    FX_ARGB argb = 0;
    for (int col = 0; col < pixel_count; col++) {
      argb = pPalette[*src_scan];
      int src_r = FXARGB_R(argb);
      int src_g = FXARGB_G(argb);
      int src_b = FXARGB_B(argb);
      src_scan++;
      uint8_t src_alpha = 0;
      if (clip_scan) {
        src_alpha = (*src_alpha_scan++) * (*clip_scan++) / 255;
      } else {
        src_alpha = *src_alpha_scan++;
      }
      if (src_alpha == 255) {
        *dest_scan++ = src_b;
        *dest_scan++ = src_g;
        *dest_scan++ = src_r;
        dest_scan += dest_gap;
        continue;
      }
      if (src_alpha == 0) {
        dest_scan += DestBpp;
        continue;
      }
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, src_alpha);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, src_alpha);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, src_alpha);
      dest_scan++;
      dest_scan += dest_gap;
    }
  } else {
    FX_ARGB argb = 0;
    for (int col = 0; col < pixel_count; col++) {
      argb = pPalette[*src_scan];
      int src_r = FXARGB_R(argb);
      int src_g = FXARGB_G(argb);
      int src_b = FXARGB_B(argb);
      if (clip_scan && clip_scan[col] < 255) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, clip_scan[col]);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, clip_scan[col]);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, clip_scan[col]);
        dest_scan++;
      } else {
        *dest_scan++ = src_b;
        *dest_scan++ = src_g;
        *dest_scan++ = src_r;
      }
      if (DestBpp == 4) {
        dest_scan++;
      }
      src_scan++;
    }
  }
}

void CompositeRow_1bppRgb2Rgb_NoBlend(uint8_t* dest_scan,
                                      const uint8_t* src_scan,
                                      int src_left,
                                      uint32_t* pPalette,
                                      int pixel_count,
                                      int DestBpp,
                                      const uint8_t* clip_scan) {
  int reset_r, reset_g, reset_b;
  int set_r, set_g, set_b;
  reset_r = FXARGB_R(pPalette[0]);
  reset_g = FXARGB_G(pPalette[0]);
  reset_b = FXARGB_B(pPalette[0]);
  set_r = FXARGB_R(pPalette[1]);
  set_g = FXARGB_G(pPalette[1]);
  set_b = FXARGB_B(pPalette[1]);
  for (int col = 0; col < pixel_count; col++) {
    int src_r, src_g, src_b;
    if (src_scan[(col + src_left) / 8] & (1 << (7 - (col + src_left) % 8))) {
      src_r = set_r;
      src_g = set_g;
      src_b = set_b;
    } else {
      src_r = reset_r;
      src_g = reset_g;
      src_b = reset_b;
    }
    if (clip_scan && clip_scan[col] < 255) {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, clip_scan[col]);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, clip_scan[col]);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, clip_scan[col]);
      dest_scan++;
    } else {
      *dest_scan++ = src_b;
      *dest_scan++ = src_g;
      *dest_scan++ = src_r;
    }
    if (DestBpp == 4) {
      dest_scan++;
    }
  }
}

void CompositeRow_8bppRgb2Argb_NoBlend(uint8_t* dest_scan,
                                       const uint8_t* src_scan,
                                       int width,
                                       uint32_t* pPalette,
                                       const uint8_t* clip_scan,
                                       const uint8_t* src_alpha_scan) {
  if (src_alpha_scan) {
    for (int col = 0; col < width; col++) {
      FX_ARGB argb = pPalette[*src_scan];
      src_scan++;
      int src_r = FXARGB_R(argb);
      int src_g = FXARGB_G(argb);
      int src_b = FXARGB_B(argb);
      uint8_t back_alpha = dest_scan[3];
      if (back_alpha == 0) {
        if (clip_scan) {
          int src_alpha = clip_scan[col] * (*src_alpha_scan) / 255;
          FXARGB_SETDIB(dest_scan, FXARGB_MAKE(src_alpha, src_r, src_g, src_b));
        } else {
          FXARGB_SETDIB(dest_scan,
                        FXARGB_MAKE(*src_alpha_scan, src_r, src_g, src_b));
        }
        dest_scan += 4;
        src_alpha_scan++;
        continue;
      }
      uint8_t src_alpha;
      if (clip_scan) {
        src_alpha = clip_scan[col] * (*src_alpha_scan++) / 255;
      } else {
        src_alpha = *src_alpha_scan++;
      }
      if (src_alpha == 0) {
        dest_scan += 4;
        continue;
      }
      uint8_t dest_alpha =
          back_alpha + src_alpha - back_alpha * src_alpha / 255;
      dest_scan[3] = dest_alpha;
      int alpha_ratio = src_alpha * 255 / dest_alpha;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, alpha_ratio);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, alpha_ratio);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, alpha_ratio);
      dest_scan++;
      dest_scan++;
    }
  } else {
    for (int col = 0; col < width; col++) {
      FX_ARGB argb = pPalette[*src_scan];
      int src_r = FXARGB_R(argb);
      int src_g = FXARGB_G(argb);
      int src_b = FXARGB_B(argb);
      if (!clip_scan || clip_scan[col] == 255) {
        *dest_scan++ = src_b;
        *dest_scan++ = src_g;
        *dest_scan++ = src_r;
        *dest_scan++ = 255;
        src_scan++;
        continue;
      }
      int src_alpha = clip_scan[col];
      if (src_alpha == 0) {
        dest_scan += 4;
        src_scan++;
        continue;
      }
      int back_alpha = dest_scan[3];
      uint8_t dest_alpha =
          back_alpha + src_alpha - back_alpha * src_alpha / 255;
      dest_scan[3] = dest_alpha;
      int alpha_ratio = src_alpha * 255 / dest_alpha;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, alpha_ratio);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, alpha_ratio);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, alpha_ratio);
      dest_scan++;
      dest_scan++;
      src_scan++;
    }
  }
}

void CompositeRow_1bppRgb2Argb_NoBlend(uint8_t* dest_scan,
                                       const uint8_t* src_scan,
                                       int src_left,
                                       int width,
                                       uint32_t* pPalette,
                                       const uint8_t* clip_scan) {
  int reset_r, reset_g, reset_b;
  int set_r, set_g, set_b;
  reset_r = FXARGB_R(pPalette[0]);
  reset_g = FXARGB_G(pPalette[0]);
  reset_b = FXARGB_B(pPalette[0]);
  set_r = FXARGB_R(pPalette[1]);
  set_g = FXARGB_G(pPalette[1]);
  set_b = FXARGB_B(pPalette[1]);
  for (int col = 0; col < width; col++) {
    int src_r, src_g, src_b;
    if (src_scan[(col + src_left) / 8] & (1 << (7 - (col + src_left) % 8))) {
      src_r = set_r;
      src_g = set_g;
      src_b = set_b;
    } else {
      src_r = reset_r;
      src_g = reset_g;
      src_b = reset_b;
    }
    if (!clip_scan || clip_scan[col] == 255) {
      *dest_scan++ = src_b;
      *dest_scan++ = src_g;
      *dest_scan++ = src_r;
      *dest_scan++ = 255;
      continue;
    }
    int src_alpha = clip_scan[col];
    if (src_alpha == 0) {
      dest_scan += 4;
      continue;
    }
    int back_alpha = dest_scan[3];
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    dest_scan[3] = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, alpha_ratio);
    dest_scan++;
    *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, alpha_ratio);
    dest_scan++;
    *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, alpha_ratio);
    dest_scan++;
    dest_scan++;
  }
}

void CompositeRow_1bppRgb2Rgba_NoBlend(uint8_t* dest_scan,
                                       const uint8_t* src_scan,
                                       int src_left,
                                       int width,
                                       uint32_t* pPalette,
                                       const uint8_t* clip_scan,
                                       uint8_t* dest_alpha_scan) {
  int reset_r, reset_g, reset_b;
  int set_r, set_g, set_b;
  reset_r = FXARGB_R(pPalette[0]);
  reset_g = FXARGB_G(pPalette[0]);
  reset_b = FXARGB_B(pPalette[0]);
  set_r = FXARGB_R(pPalette[1]);
  set_g = FXARGB_G(pPalette[1]);
  set_b = FXARGB_B(pPalette[1]);
  for (int col = 0; col < width; col++) {
    int src_r, src_g, src_b;
    if (src_scan[(col + src_left) / 8] & (1 << (7 - (col + src_left) % 8))) {
      src_r = set_r;
      src_g = set_g;
      src_b = set_b;
    } else {
      src_r = reset_r;
      src_g = reset_g;
      src_b = reset_b;
    }
    if (!clip_scan || clip_scan[col] == 255) {
      *dest_scan++ = src_b;
      *dest_scan++ = src_g;
      *dest_scan++ = src_r;
      *dest_alpha_scan++ = 255;
      continue;
    }
    int src_alpha = clip_scan[col];
    if (src_alpha == 0) {
      dest_scan += 3;
      dest_alpha_scan++;
      continue;
    }
    int back_alpha = *dest_alpha_scan;
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    *dest_alpha_scan++ = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, alpha_ratio);
    dest_scan++;
    *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, alpha_ratio);
    dest_scan++;
    *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, alpha_ratio);
    dest_scan++;
  }
}

void CompositeRow_ByteMask2Argb(uint8_t* dest_scan,
                                const uint8_t* src_scan,
                                int mask_alpha,
                                int src_r,
                                int src_g,
                                int src_b,
                                int pixel_count,
                                int blend_type,
                                const uint8_t* clip_scan) {
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] * src_scan[col] / 255 / 255;
    } else {
      src_alpha = mask_alpha * src_scan[col] / 255;
    }
    uint8_t back_alpha = dest_scan[3];
    if (back_alpha == 0) {
      FXARGB_SETDIB(dest_scan, FXARGB_MAKE(src_alpha, src_r, src_g, src_b));
      dest_scan += 4;
      continue;
    }
    if (src_alpha == 0) {
      dest_scan += 4;
      continue;
    }
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    dest_scan[3] = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    if (blend_type >= FXDIB_BLEND_NONSEPARABLE) {
      int blended_colors[3];
      uint8_t scan[3] = {static_cast<uint8_t>(src_b),
                         static_cast<uint8_t>(src_g),
                         static_cast<uint8_t>(src_r)};
      RGB_Blend(blend_type, scan, dest_scan, blended_colors);
      *dest_scan =
          FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[0], alpha_ratio);
      dest_scan++;
      *dest_scan =
          FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[1], alpha_ratio);
      dest_scan++;
      *dest_scan =
          FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[2], alpha_ratio);
    } else if (blend_type) {
      int blended = Blend(blend_type, *dest_scan, src_b);
      blended = FXDIB_ALPHA_MERGE(src_b, blended, back_alpha);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
      dest_scan++;
      blended = Blend(blend_type, *dest_scan, src_g);
      blended = FXDIB_ALPHA_MERGE(src_g, blended, back_alpha);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
      dest_scan++;
      blended = Blend(blend_type, *dest_scan, src_r);
      blended = FXDIB_ALPHA_MERGE(src_r, blended, back_alpha);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
    } else {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, alpha_ratio);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, alpha_ratio);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, alpha_ratio);
    }
    dest_scan += 2;
  }
}

void CompositeRow_ByteMask2Rgba(uint8_t* dest_scan,
                                const uint8_t* src_scan,
                                int mask_alpha,
                                int src_r,
                                int src_g,
                                int src_b,
                                int pixel_count,
                                int blend_type,
                                const uint8_t* clip_scan,
                                uint8_t* dest_alpha_scan) {
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] * src_scan[col] / 255 / 255;
    } else {
      src_alpha = mask_alpha * src_scan[col] / 255;
    }
    uint8_t back_alpha = *dest_alpha_scan;
    if (back_alpha == 0) {
      *dest_scan++ = src_b;
      *dest_scan++ = src_g;
      *dest_scan++ = src_r;
      *dest_alpha_scan++ = src_alpha;
      continue;
    }
    if (src_alpha == 0) {
      dest_scan += 3;
      dest_alpha_scan++;
      continue;
    }
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    *dest_alpha_scan++ = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    if (blend_type >= FXDIB_BLEND_NONSEPARABLE) {
      int blended_colors[3];
      uint8_t scan[3] = {static_cast<uint8_t>(src_b),
                         static_cast<uint8_t>(src_g),
                         static_cast<uint8_t>(src_r)};
      RGB_Blend(blend_type, scan, dest_scan, blended_colors);
      *dest_scan =
          FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[0], alpha_ratio);
      dest_scan++;
      *dest_scan =
          FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[1], alpha_ratio);
      dest_scan++;
      *dest_scan =
          FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[2], alpha_ratio);
      dest_scan++;
    } else if (blend_type) {
      int blended = Blend(blend_type, *dest_scan, src_b);
      blended = FXDIB_ALPHA_MERGE(src_b, blended, back_alpha);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
      dest_scan++;
      blended = Blend(blend_type, *dest_scan, src_g);
      blended = FXDIB_ALPHA_MERGE(src_g, blended, back_alpha);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
      dest_scan++;
      blended = Blend(blend_type, *dest_scan, src_r);
      blended = FXDIB_ALPHA_MERGE(src_r, blended, back_alpha);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
      dest_scan++;
    } else {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, alpha_ratio);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, alpha_ratio);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, alpha_ratio);
      dest_scan++;
    }
  }
}

void CompositeRow_ByteMask2Rgb(uint8_t* dest_scan,
                               const uint8_t* src_scan,
                               int mask_alpha,
                               int src_r,
                               int src_g,
                               int src_b,
                               int pixel_count,
                               int blend_type,
                               int Bpp,
                               const uint8_t* clip_scan) {
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] * src_scan[col] / 255 / 255;
    } else {
      src_alpha = mask_alpha * src_scan[col] / 255;
    }
    if (src_alpha == 0) {
      dest_scan += Bpp;
      continue;
    }
    if (blend_type >= FXDIB_BLEND_NONSEPARABLE) {
      int blended_colors[3];
      uint8_t scan[3] = {static_cast<uint8_t>(src_b),
                         static_cast<uint8_t>(src_g),
                         static_cast<uint8_t>(src_r)};
      RGB_Blend(blend_type, scan, dest_scan, blended_colors);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[0], src_alpha);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[1], src_alpha);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[2], src_alpha);
    } else if (blend_type) {
      int blended = Blend(blend_type, *dest_scan, src_b);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, src_alpha);
      dest_scan++;
      blended = Blend(blend_type, *dest_scan, src_g);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, src_alpha);
      dest_scan++;
      blended = Blend(blend_type, *dest_scan, src_r);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, src_alpha);
    } else {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, src_alpha);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, src_alpha);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, src_alpha);
    }
    dest_scan += Bpp - 2;
  }
}

void CompositeRow_ByteMask2Mask(uint8_t* dest_scan,
                                const uint8_t* src_scan,
                                int mask_alpha,
                                int pixel_count,
                                const uint8_t* clip_scan) {
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] * src_scan[col] / 255 / 255;
    } else {
      src_alpha = mask_alpha * src_scan[col] / 255;
    }
    uint8_t back_alpha = *dest_scan;
    if (!back_alpha) {
      *dest_scan = src_alpha;
    } else if (src_alpha) {
      *dest_scan = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    }
    dest_scan++;
  }
}

void CompositeRow_ByteMask2Gray(uint8_t* dest_scan,
                                const uint8_t* src_scan,
                                int mask_alpha,
                                int src_gray,
                                int pixel_count,
                                const uint8_t* clip_scan) {
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] * src_scan[col] / 255 / 255;
    } else {
      src_alpha = mask_alpha * src_scan[col] / 255;
    }
    if (src_alpha) {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_gray, src_alpha);
    }
    dest_scan++;
  }
}

void CompositeRow_ByteMask2Graya(uint8_t* dest_scan,
                                 const uint8_t* src_scan,
                                 int mask_alpha,
                                 int src_gray,
                                 int pixel_count,
                                 const uint8_t* clip_scan,
                                 uint8_t* dest_alpha_scan) {
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] * src_scan[col] / 255 / 255;
    } else {
      src_alpha = mask_alpha * src_scan[col] / 255;
    }
    uint8_t back_alpha = *dest_alpha_scan;
    if (back_alpha == 0) {
      *dest_scan++ = src_gray;
      *dest_alpha_scan++ = src_alpha;
      continue;
    }
    if (src_alpha == 0) {
      dest_scan++;
      dest_alpha_scan++;
      continue;
    }
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    *dest_alpha_scan++ = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_gray, alpha_ratio);
    dest_scan++;
  }
}

void CompositeRow_BitMask2Argb(uint8_t* dest_scan,
                               const uint8_t* src_scan,
                               int mask_alpha,
                               int src_r,
                               int src_g,
                               int src_b,
                               int src_left,
                               int pixel_count,
                               int blend_type,
                               const uint8_t* clip_scan) {
  if (blend_type == FXDIB_BLEND_NORMAL && !clip_scan && mask_alpha == 255) {
    FX_ARGB argb = FXARGB_MAKE(0xff, src_r, src_g, src_b);
    for (int col = 0; col < pixel_count; col++) {
      if (src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8))) {
        FXARGB_SETDIB(dest_scan, argb);
      }
      dest_scan += 4;
    }
    return;
  }
  for (int col = 0; col < pixel_count; col++) {
    if (!(src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8)))) {
      dest_scan += 4;
      continue;
    }
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] / 255;
    } else {
      src_alpha = mask_alpha;
    }
    uint8_t back_alpha = dest_scan[3];
    if (back_alpha == 0) {
      FXARGB_SETDIB(dest_scan, FXARGB_MAKE(src_alpha, src_r, src_g, src_b));
      dest_scan += 4;
      continue;
    }
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    dest_scan[3] = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    if (blend_type >= FXDIB_BLEND_NONSEPARABLE) {
      int blended_colors[3];
      uint8_t scan[3] = {static_cast<uint8_t>(src_b),
                         static_cast<uint8_t>(src_g),
                         static_cast<uint8_t>(src_r)};
      RGB_Blend(blend_type, scan, dest_scan, blended_colors);
      *dest_scan =
          FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[0], alpha_ratio);
      dest_scan++;
      *dest_scan =
          FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[1], alpha_ratio);
      dest_scan++;
      *dest_scan =
          FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[2], alpha_ratio);
    } else if (blend_type) {
      int blended = Blend(blend_type, *dest_scan, src_b);
      blended = FXDIB_ALPHA_MERGE(src_b, blended, back_alpha);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
      dest_scan++;
      blended = Blend(blend_type, *dest_scan, src_g);
      blended = FXDIB_ALPHA_MERGE(src_g, blended, back_alpha);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
      dest_scan++;
      blended = Blend(blend_type, *dest_scan, src_r);
      blended = FXDIB_ALPHA_MERGE(src_r, blended, back_alpha);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, alpha_ratio);
    } else {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, alpha_ratio);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, alpha_ratio);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, alpha_ratio);
    }
    dest_scan += 2;
  }
}

void CompositeRow_BitMask2Rgb(uint8_t* dest_scan,
                              const uint8_t* src_scan,
                              int mask_alpha,
                              int src_r,
                              int src_g,
                              int src_b,
                              int src_left,
                              int pixel_count,
                              int blend_type,
                              int Bpp,
                              const uint8_t* clip_scan) {
  if (blend_type == FXDIB_BLEND_NORMAL && !clip_scan && mask_alpha == 255) {
    for (int col = 0; col < pixel_count; col++) {
      if (src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8))) {
        dest_scan[2] = src_r;
        dest_scan[1] = src_g;
        dest_scan[0] = src_b;
      }
      dest_scan += Bpp;
    }
    return;
  }
  for (int col = 0; col < pixel_count; col++) {
    if (!(src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8)))) {
      dest_scan += Bpp;
      continue;
    }
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] / 255;
    } else {
      src_alpha = mask_alpha;
    }
    if (src_alpha == 0) {
      dest_scan += Bpp;
      continue;
    }
    if (blend_type >= FXDIB_BLEND_NONSEPARABLE) {
      int blended_colors[3];
      uint8_t scan[3] = {static_cast<uint8_t>(src_b),
                         static_cast<uint8_t>(src_g),
                         static_cast<uint8_t>(src_r)};
      RGB_Blend(blend_type, scan, dest_scan, blended_colors);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[0], src_alpha);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[1], src_alpha);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended_colors[2], src_alpha);
    } else if (blend_type) {
      int blended = Blend(blend_type, *dest_scan, src_b);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, src_alpha);
      dest_scan++;
      blended = Blend(blend_type, *dest_scan, src_g);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, src_alpha);
      dest_scan++;
      blended = Blend(blend_type, *dest_scan, src_r);
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, blended, src_alpha);
    } else {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, src_alpha);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, src_alpha);
      dest_scan++;
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, src_alpha);
    }
    dest_scan += Bpp - 2;
  }
}

void CompositeRow_BitMask2Mask(uint8_t* dest_scan,
                               const uint8_t* src_scan,
                               int mask_alpha,
                               int src_left,
                               int pixel_count,
                               const uint8_t* clip_scan) {
  for (int col = 0; col < pixel_count; col++) {
    if (!(src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8)))) {
      dest_scan++;
      continue;
    }
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] / 255;
    } else {
      src_alpha = mask_alpha;
    }
    uint8_t back_alpha = *dest_scan;
    if (!back_alpha) {
      *dest_scan = src_alpha;
    } else if (src_alpha) {
      *dest_scan = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    }
    dest_scan++;
  }
}

void CompositeRow_BitMask2Gray(uint8_t* dest_scan,
                               const uint8_t* src_scan,
                               int mask_alpha,
                               int src_gray,
                               int src_left,
                               int pixel_count,
                               const uint8_t* clip_scan) {
  for (int col = 0; col < pixel_count; col++) {
    if (!(src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8)))) {
      dest_scan++;
      continue;
    }
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] / 255;
    } else {
      src_alpha = mask_alpha;
    }
    if (src_alpha) {
      *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_gray, src_alpha);
    }
    dest_scan++;
  }
}

void CompositeRow_BitMask2Graya(uint8_t* dest_scan,
                                const uint8_t* src_scan,
                                int mask_alpha,
                                int src_gray,
                                int src_left,
                                int pixel_count,
                                const uint8_t* clip_scan,
                                uint8_t* dest_alpha_scan) {
  for (int col = 0; col < pixel_count; col++) {
    if (!(src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8)))) {
      dest_scan++;
      dest_alpha_scan++;
      continue;
    }
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] / 255;
    } else {
      src_alpha = mask_alpha;
    }
    uint8_t back_alpha = *dest_alpha_scan;
    if (back_alpha == 0) {
      *dest_scan++ = src_gray;
      *dest_alpha_scan++ = src_alpha;
      continue;
    }
    if (src_alpha == 0) {
      dest_scan++;
      dest_alpha_scan++;
      continue;
    }
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    *dest_alpha_scan++ = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_gray, alpha_ratio);
    dest_scan++;
  }
}

void CompositeRow_Argb2Argb_RgbByteOrder(uint8_t* dest_scan,
                                         const uint8_t* src_scan,
                                         int pixel_count,
                                         int blend_type,
                                         const uint8_t* clip_scan) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  for (int col = 0; col < pixel_count; col++) {
    uint8_t back_alpha = dest_scan[3];
    if (back_alpha == 0) {
      if (clip_scan) {
        int src_alpha = clip_scan[col] * src_scan[3] / 255;
        dest_scan[3] = src_alpha;
        dest_scan[0] = src_scan[2];
        dest_scan[1] = src_scan[1];
        dest_scan[2] = src_scan[0];
      } else {
        FXARGB_RGBORDERCOPY(dest_scan, src_scan);
      }
      dest_scan += 4;
      src_scan += 4;
      continue;
    }
    uint8_t src_alpha;
    if (clip_scan) {
      src_alpha = clip_scan[col] * src_scan[3] / 255;
    } else {
      src_alpha = src_scan[3];
    }
    if (src_alpha == 0) {
      dest_scan += 4;
      src_scan += 4;
      continue;
    }
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    dest_scan[3] = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    if (bNonseparableBlend) {
      uint8_t dest_scan_o[3];
      dest_scan_o[0] = dest_scan[2];
      dest_scan_o[1] = dest_scan[1];
      dest_scan_o[2] = dest_scan[0];
      RGB_Blend(blend_type, src_scan, dest_scan_o, blended_colors);
    }
    for (int color = 0; color < 3; color++) {
      int index = 2 - color;
      if (blend_type) {
        int blended = bNonseparableBlend
                          ? blended_colors[color]
                          : Blend(blend_type, dest_scan[index], *src_scan);
        blended = FXDIB_ALPHA_MERGE(*src_scan, blended, back_alpha);
        dest_scan[index] =
            FXDIB_ALPHA_MERGE(dest_scan[index], blended, alpha_ratio);
      } else {
        dest_scan[index] =
            FXDIB_ALPHA_MERGE(dest_scan[index], *src_scan, alpha_ratio);
      }
      src_scan++;
    }
    dest_scan += 4;
    src_scan++;
  }
}

void CompositeRow_Rgb2Argb_Blend_NoClip_RgbByteOrder(uint8_t* dest_scan,
                                                     const uint8_t* src_scan,
                                                     int width,
                                                     int blend_type,
                                                     int src_Bpp) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  int src_gap = src_Bpp - 3;
  for (int col = 0; col < width; col++) {
    uint8_t back_alpha = dest_scan[3];
    if (back_alpha == 0) {
      if (src_Bpp == 4) {
        FXARGB_SETRGBORDERDIB(dest_scan, 0xff000000 | FXARGB_GETDIB(src_scan));
      } else {
        FXARGB_SETRGBORDERDIB(dest_scan, FXARGB_MAKE(0xff, src_scan[2],
                                                     src_scan[1], src_scan[0]));
      }
      dest_scan += 4;
      src_scan += src_Bpp;
      continue;
    }
    dest_scan[3] = 0xff;
    if (bNonseparableBlend) {
      uint8_t dest_scan_o[3];
      dest_scan_o[0] = dest_scan[2];
      dest_scan_o[1] = dest_scan[1];
      dest_scan_o[2] = dest_scan[0];
      RGB_Blend(blend_type, src_scan, dest_scan_o, blended_colors);
    }
    for (int color = 0; color < 3; color++) {
      int index = 2 - color;
      int src_color = *src_scan;
      int blended = bNonseparableBlend
                        ? blended_colors[color]
                        : Blend(blend_type, dest_scan[index], src_color);
      dest_scan[index] = FXDIB_ALPHA_MERGE(src_color, blended, back_alpha);
      src_scan++;
    }
    dest_scan += 4;
    src_scan += src_gap;
  }
}

void CompositeRow_Argb2Rgb_Blend_RgbByteOrder(uint8_t* dest_scan,
                                              const uint8_t* src_scan,
                                              int width,
                                              int blend_type,
                                              int dest_Bpp,
                                              const uint8_t* clip_scan) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  for (int col = 0; col < width; col++) {
    uint8_t src_alpha;
    if (clip_scan) {
      src_alpha = src_scan[3] * (*clip_scan++) / 255;
    } else {
      src_alpha = src_scan[3];
    }
    if (src_alpha == 0) {
      dest_scan += dest_Bpp;
      src_scan += 4;
      continue;
    }
    if (bNonseparableBlend) {
      uint8_t dest_scan_o[3];
      dest_scan_o[0] = dest_scan[2];
      dest_scan_o[1] = dest_scan[1];
      dest_scan_o[2] = dest_scan[0];
      RGB_Blend(blend_type, src_scan, dest_scan_o, blended_colors);
    }
    for (int color = 0; color < 3; color++) {
      int index = 2 - color;
      int back_color = dest_scan[index];
      int blended = bNonseparableBlend
                        ? blended_colors[color]
                        : Blend(blend_type, back_color, *src_scan);
      dest_scan[index] = FXDIB_ALPHA_MERGE(back_color, blended, src_alpha);
      src_scan++;
    }
    dest_scan += dest_Bpp;
    src_scan++;
  }
}

void CompositeRow_Rgb2Argb_NoBlend_NoClip_RgbByteOrder(uint8_t* dest_scan,
                                                       const uint8_t* src_scan,
                                                       int width,
                                                       int src_Bpp) {
  for (int col = 0; col < width; col++) {
    if (src_Bpp == 4) {
      FXARGB_SETRGBORDERDIB(dest_scan, 0xff000000 | FXARGB_GETDIB(src_scan));
    } else {
      FXARGB_SETRGBORDERDIB(
          dest_scan, FXARGB_MAKE(0xff, src_scan[2], src_scan[1], src_scan[0]));
    }
    dest_scan += 4;
    src_scan += src_Bpp;
  }
}

void CompositeRow_Rgb2Rgb_Blend_NoClip_RgbByteOrder(uint8_t* dest_scan,
                                                    const uint8_t* src_scan,
                                                    int width,
                                                    int blend_type,
                                                    int dest_Bpp,
                                                    int src_Bpp) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  int src_gap = src_Bpp - 3;
  for (int col = 0; col < width; col++) {
    if (bNonseparableBlend) {
      uint8_t dest_scan_o[3];
      dest_scan_o[0] = dest_scan[2];
      dest_scan_o[1] = dest_scan[1];
      dest_scan_o[2] = dest_scan[0];
      RGB_Blend(blend_type, src_scan, dest_scan_o, blended_colors);
    }
    for (int color = 0; color < 3; color++) {
      int index = 2 - color;
      int back_color = dest_scan[index];
      int src_color = *src_scan;
      int blended = bNonseparableBlend
                        ? blended_colors[color]
                        : Blend(blend_type, back_color, src_color);
      dest_scan[index] = blended;
      src_scan++;
    }
    dest_scan += dest_Bpp;
    src_scan += src_gap;
  }
}

void CompositeRow_Argb2Rgb_NoBlend_RgbByteOrder(uint8_t* dest_scan,
                                                const uint8_t* src_scan,
                                                int width,
                                                int dest_Bpp,
                                                const uint8_t* clip_scan) {
  for (int col = 0; col < width; col++) {
    uint8_t src_alpha;
    if (clip_scan) {
      src_alpha = src_scan[3] * (*clip_scan++) / 255;
    } else {
      src_alpha = src_scan[3];
    }
    if (src_alpha == 255) {
      dest_scan[2] = *src_scan++;
      dest_scan[1] = *src_scan++;
      dest_scan[0] = *src_scan++;
      dest_scan += dest_Bpp;
      src_scan++;
      continue;
    }
    if (src_alpha == 0) {
      dest_scan += dest_Bpp;
      src_scan += 4;
      continue;
    }
    for (int color = 0; color < 3; color++) {
      int index = 2 - color;
      dest_scan[index] =
          FXDIB_ALPHA_MERGE(dest_scan[index], *src_scan, src_alpha);
      src_scan++;
    }
    dest_scan += dest_Bpp;
    src_scan++;
  }
}

void CompositeRow_Rgb2Rgb_NoBlend_NoClip_RgbByteOrder(uint8_t* dest_scan,
                                                      const uint8_t* src_scan,
                                                      int width,
                                                      int dest_Bpp,
                                                      int src_Bpp) {
  for (int col = 0; col < width; col++) {
    dest_scan[2] = src_scan[0];
    dest_scan[1] = src_scan[1];
    dest_scan[0] = src_scan[2];
    dest_scan += dest_Bpp;
    src_scan += src_Bpp;
  }
}

void CompositeRow_Rgb2Argb_Blend_Clip_RgbByteOrder(uint8_t* dest_scan,
                                                   const uint8_t* src_scan,
                                                   int width,
                                                   int blend_type,
                                                   int src_Bpp,
                                                   const uint8_t* clip_scan) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  int src_gap = src_Bpp - 3;
  for (int col = 0; col < width; col++) {
    int src_alpha = *clip_scan++;
    uint8_t back_alpha = dest_scan[3];
    if (back_alpha == 0) {
      dest_scan[2] = *src_scan++;
      dest_scan[1] = *src_scan++;
      dest_scan[0] = *src_scan++;
      src_scan += src_gap;
      dest_scan += 4;
      continue;
    }
    if (src_alpha == 0) {
      dest_scan += 4;
      src_scan += src_Bpp;
      continue;
    }
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    dest_scan[3] = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    if (bNonseparableBlend) {
      uint8_t dest_scan_o[3];
      dest_scan_o[0] = dest_scan[2];
      dest_scan_o[1] = dest_scan[1];
      dest_scan_o[2] = dest_scan[0];
      RGB_Blend(blend_type, src_scan, dest_scan_o, blended_colors);
    }
    for (int color = 0; color < 3; color++) {
      int index = 2 - color;
      int src_color = *src_scan;
      int blended = bNonseparableBlend
                        ? blended_colors[color]
                        : Blend(blend_type, dest_scan[index], src_color);
      blended = FXDIB_ALPHA_MERGE(src_color, blended, back_alpha);
      dest_scan[index] =
          FXDIB_ALPHA_MERGE(dest_scan[index], blended, alpha_ratio);
      src_scan++;
    }
    dest_scan += 4;
    src_scan += src_gap;
  }
}

void CompositeRow_Rgb2Rgb_Blend_Clip_RgbByteOrder(uint8_t* dest_scan,
                                                  const uint8_t* src_scan,
                                                  int width,
                                                  int blend_type,
                                                  int dest_Bpp,
                                                  int src_Bpp,
                                                  const uint8_t* clip_scan) {
  int blended_colors[3];
  bool bNonseparableBlend = blend_type >= FXDIB_BLEND_NONSEPARABLE;
  int src_gap = src_Bpp - 3;
  for (int col = 0; col < width; col++) {
    uint8_t src_alpha = *clip_scan++;
    if (src_alpha == 0) {
      dest_scan += dest_Bpp;
      src_scan += src_Bpp;
      continue;
    }
    if (bNonseparableBlend) {
      uint8_t dest_scan_o[3];
      dest_scan_o[0] = dest_scan[2];
      dest_scan_o[1] = dest_scan[1];
      dest_scan_o[2] = dest_scan[0];
      RGB_Blend(blend_type, src_scan, dest_scan_o, blended_colors);
    }
    for (int color = 0; color < 3; color++) {
      int index = 2 - color;
      int src_color = *src_scan;
      int back_color = dest_scan[index];
      int blended = bNonseparableBlend
                        ? blended_colors[color]
                        : Blend(blend_type, back_color, src_color);
      dest_scan[index] = FXDIB_ALPHA_MERGE(back_color, blended, src_alpha);
      src_scan++;
    }
    dest_scan += dest_Bpp;
    src_scan += src_gap;
  }
}

void CompositeRow_Rgb2Argb_NoBlend_Clip_RgbByteOrder(uint8_t* dest_scan,
                                                     const uint8_t* src_scan,
                                                     int width,
                                                     int src_Bpp,
                                                     const uint8_t* clip_scan) {
  int src_gap = src_Bpp - 3;
  for (int col = 0; col < width; col++) {
    int src_alpha = clip_scan[col];
    if (src_alpha == 255) {
      dest_scan[2] = *src_scan++;
      dest_scan[1] = *src_scan++;
      dest_scan[0] = *src_scan++;
      dest_scan[3] = 255;
      dest_scan += 4;
      src_scan += src_gap;
      continue;
    }
    if (src_alpha == 0) {
      dest_scan += 4;
      src_scan += src_Bpp;
      continue;
    }
    int back_alpha = dest_scan[3];
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    dest_scan[3] = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    for (int color = 0; color < 3; color++) {
      int index = 2 - color;
      dest_scan[index] =
          FXDIB_ALPHA_MERGE(dest_scan[index], *src_scan, alpha_ratio);
      src_scan++;
    }
    dest_scan += 4;
    src_scan += src_gap;
  }
}

void CompositeRow_Rgb2Rgb_NoBlend_Clip_RgbByteOrder(uint8_t* dest_scan,
                                                    const uint8_t* src_scan,
                                                    int width,
                                                    int dest_Bpp,
                                                    int src_Bpp,
                                                    const uint8_t* clip_scan) {
  for (int col = 0; col < width; col++) {
    int src_alpha = clip_scan[col];
    if (src_alpha == 255) {
      dest_scan[2] = src_scan[0];
      dest_scan[1] = src_scan[1];
      dest_scan[0] = src_scan[2];
    } else if (src_alpha) {
      dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], *src_scan, src_alpha);
      src_scan++;
      dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], *src_scan, src_alpha);
      src_scan++;
      dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], *src_scan, src_alpha);
      dest_scan += dest_Bpp;
      src_scan += src_Bpp - 2;
      continue;
    }
    dest_scan += dest_Bpp;
    src_scan += src_Bpp;
  }
}

void CompositeRow_8bppRgb2Rgb_NoBlend_RgbByteOrder(uint8_t* dest_scan,
                                                   const uint8_t* src_scan,
                                                   FX_ARGB* pPalette,
                                                   int pixel_count,
                                                   int DestBpp,
                                                   const uint8_t* clip_scan) {
  for (int col = 0; col < pixel_count; col++) {
    FX_ARGB argb = pPalette ? pPalette[*src_scan] : (*src_scan) * 0x010101;
    int src_r = FXARGB_R(argb);
    int src_g = FXARGB_G(argb);
    int src_b = FXARGB_B(argb);
    if (clip_scan && clip_scan[col] < 255) {
      dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], src_b, clip_scan[col]);
      dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], src_g, clip_scan[col]);
      dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], src_r, clip_scan[col]);
    } else {
      dest_scan[2] = src_b;
      dest_scan[1] = src_g;
      dest_scan[0] = src_r;
    }
    dest_scan += DestBpp;
    src_scan++;
  }
}

void CompositeRow_1bppRgb2Rgb_NoBlend_RgbByteOrder(uint8_t* dest_scan,
                                                   const uint8_t* src_scan,
                                                   int src_left,
                                                   FX_ARGB* pPalette,
                                                   int pixel_count,
                                                   int DestBpp,
                                                   const uint8_t* clip_scan) {
  int reset_r, reset_g, reset_b;
  int set_r, set_g, set_b;
  if (pPalette) {
    reset_r = FXARGB_R(pPalette[0]);
    reset_g = FXARGB_G(pPalette[0]);
    reset_b = FXARGB_B(pPalette[0]);
    set_r = FXARGB_R(pPalette[1]);
    set_g = FXARGB_G(pPalette[1]);
    set_b = FXARGB_B(pPalette[1]);
  } else {
    reset_r = reset_g = reset_b = 0;
    set_r = set_g = set_b = 255;
  }
  for (int col = 0; col < pixel_count; col++) {
    int src_r, src_g, src_b;
    if (src_scan[(col + src_left) / 8] & (1 << (7 - (col + src_left) % 8))) {
      src_r = set_r;
      src_g = set_g;
      src_b = set_b;
    } else {
      src_r = reset_r;
      src_g = reset_g;
      src_b = reset_b;
    }
    if (clip_scan && clip_scan[col] < 255) {
      dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], src_b, clip_scan[col]);
      dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], src_g, clip_scan[col]);
      dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], src_r, clip_scan[col]);
    } else {
      dest_scan[2] = src_b;
      dest_scan[1] = src_g;
      dest_scan[0] = src_r;
    }
    dest_scan += DestBpp;
  }
}

void CompositeRow_8bppRgb2Argb_NoBlend_RgbByteOrder(uint8_t* dest_scan,
                                                    const uint8_t* src_scan,
                                                    int width,
                                                    FX_ARGB* pPalette,
                                                    const uint8_t* clip_scan) {
  for (int col = 0; col < width; col++) {
    int src_r, src_g, src_b;
    if (pPalette) {
      FX_ARGB argb = pPalette[*src_scan];
      src_r = FXARGB_R(argb);
      src_g = FXARGB_G(argb);
      src_b = FXARGB_B(argb);
    } else {
      src_r = src_g = src_b = *src_scan;
    }
    if (!clip_scan || clip_scan[col] == 255) {
      dest_scan[2] = src_b;
      dest_scan[1] = src_g;
      dest_scan[0] = src_r;
      dest_scan[3] = 255;
      src_scan++;
      dest_scan += 4;
      continue;
    }
    int src_alpha = clip_scan[col];
    if (src_alpha == 0) {
      dest_scan += 4;
      src_scan++;
      continue;
    }
    int back_alpha = dest_scan[3];
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    dest_scan[3] = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], src_b, alpha_ratio);
    dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], src_g, alpha_ratio);
    dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], src_r, alpha_ratio);
    dest_scan += 4;
    src_scan++;
  }
}

void CompositeRow_1bppRgb2Argb_NoBlend_RgbByteOrder(uint8_t* dest_scan,
                                                    const uint8_t* src_scan,
                                                    int src_left,
                                                    int width,
                                                    FX_ARGB* pPalette,
                                                    const uint8_t* clip_scan) {
  int reset_r, reset_g, reset_b;
  int set_r, set_g, set_b;
  if (pPalette) {
    reset_r = FXARGB_R(pPalette[0]);
    reset_g = FXARGB_G(pPalette[0]);
    reset_b = FXARGB_B(pPalette[0]);
    set_r = FXARGB_R(pPalette[1]);
    set_g = FXARGB_G(pPalette[1]);
    set_b = FXARGB_B(pPalette[1]);
  } else {
    reset_r = reset_g = reset_b = 0;
    set_r = set_g = set_b = 255;
  }
  for (int col = 0; col < width; col++) {
    int src_r, src_g, src_b;
    if (src_scan[(col + src_left) / 8] & (1 << (7 - (col + src_left) % 8))) {
      src_r = set_r;
      src_g = set_g;
      src_b = set_b;
    } else {
      src_r = reset_r;
      src_g = reset_g;
      src_b = reset_b;
    }
    if (!clip_scan || clip_scan[col] == 255) {
      dest_scan[2] = src_b;
      dest_scan[1] = src_g;
      dest_scan[0] = src_r;
      dest_scan[3] = 255;
      dest_scan += 4;
      continue;
    }
    int src_alpha = clip_scan[col];
    if (src_alpha == 0) {
      dest_scan += 4;
      continue;
    }
    int back_alpha = dest_scan[3];
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    dest_scan[3] = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], src_b, alpha_ratio);
    dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], src_g, alpha_ratio);
    dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], src_r, alpha_ratio);
    dest_scan += 4;
  }
}

void CompositeRow_ByteMask2Argb_RgbByteOrder(uint8_t* dest_scan,
                                             const uint8_t* src_scan,
                                             int mask_alpha,
                                             int src_r,
                                             int src_g,
                                             int src_b,
                                             int pixel_count,
                                             int blend_type,
                                             const uint8_t* clip_scan) {
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] * src_scan[col] / 255 / 255;
    } else {
      src_alpha = mask_alpha * src_scan[col] / 255;
    }
    uint8_t back_alpha = dest_scan[3];
    if (back_alpha == 0) {
      FXARGB_SETRGBORDERDIB(dest_scan,
                            FXARGB_MAKE(src_alpha, src_r, src_g, src_b));
      dest_scan += 4;
      continue;
    }
    if (src_alpha == 0) {
      dest_scan += 4;
      continue;
    }
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    dest_scan[3] = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    if (blend_type >= FXDIB_BLEND_NONSEPARABLE) {
      int blended_colors[3];
      uint8_t scan[3] = {static_cast<uint8_t>(src_b),
                         static_cast<uint8_t>(src_g),
                         static_cast<uint8_t>(src_r)};
      uint8_t dest_scan_o[3] = {dest_scan[2], dest_scan[1], dest_scan[0]};
      RGB_Blend(blend_type, scan, dest_scan_o, blended_colors);
      dest_scan[2] =
          FXDIB_ALPHA_MERGE(dest_scan[2], blended_colors[0], alpha_ratio);
      dest_scan[1] =
          FXDIB_ALPHA_MERGE(dest_scan[1], blended_colors[1], alpha_ratio);
      dest_scan[0] =
          FXDIB_ALPHA_MERGE(dest_scan[0], blended_colors[2], alpha_ratio);
    } else if (blend_type) {
      int blended = Blend(blend_type, dest_scan[2], src_b);
      blended = FXDIB_ALPHA_MERGE(src_b, blended, back_alpha);
      dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], blended, alpha_ratio);
      blended = Blend(blend_type, dest_scan[1], src_g);
      blended = FXDIB_ALPHA_MERGE(src_g, blended, back_alpha);
      dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], blended, alpha_ratio);
      blended = Blend(blend_type, dest_scan[0], src_r);
      blended = FXDIB_ALPHA_MERGE(src_r, blended, back_alpha);
      dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], blended, alpha_ratio);
    } else {
      dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], src_b, alpha_ratio);
      dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], src_g, alpha_ratio);
      dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], src_r, alpha_ratio);
    }
    dest_scan += 4;
  }
}

void CompositeRow_ByteMask2Rgb_RgbByteOrder(uint8_t* dest_scan,
                                            const uint8_t* src_scan,
                                            int mask_alpha,
                                            int src_r,
                                            int src_g,
                                            int src_b,
                                            int pixel_count,
                                            int blend_type,
                                            int Bpp,
                                            const uint8_t* clip_scan) {
  for (int col = 0; col < pixel_count; col++) {
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] * src_scan[col] / 255 / 255;
    } else {
      src_alpha = mask_alpha * src_scan[col] / 255;
    }
    if (src_alpha == 0) {
      dest_scan += Bpp;
      continue;
    }
    if (blend_type >= FXDIB_BLEND_NONSEPARABLE) {
      int blended_colors[3];
      uint8_t scan[3] = {static_cast<uint8_t>(src_b),
                         static_cast<uint8_t>(src_g),
                         static_cast<uint8_t>(src_r)};
      uint8_t dest_scan_o[3] = {dest_scan[2], dest_scan[1], dest_scan[0]};
      RGB_Blend(blend_type, scan, dest_scan_o, blended_colors);
      dest_scan[2] =
          FXDIB_ALPHA_MERGE(dest_scan[2], blended_colors[0], src_alpha);
      dest_scan[1] =
          FXDIB_ALPHA_MERGE(dest_scan[1], blended_colors[1], src_alpha);
      dest_scan[0] =
          FXDIB_ALPHA_MERGE(dest_scan[0], blended_colors[2], src_alpha);
    } else if (blend_type) {
      int blended = Blend(blend_type, dest_scan[2], src_b);
      dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], blended, src_alpha);
      blended = Blend(blend_type, dest_scan[1], src_g);
      dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], blended, src_alpha);
      blended = Blend(blend_type, dest_scan[0], src_r);
      dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], blended, src_alpha);
    } else {
      dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], src_b, src_alpha);
      dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], src_g, src_alpha);
      dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], src_r, src_alpha);
    }
    dest_scan += Bpp;
  }
}

void CompositeRow_BitMask2Argb_RgbByteOrder(uint8_t* dest_scan,
                                            const uint8_t* src_scan,
                                            int mask_alpha,
                                            int src_r,
                                            int src_g,
                                            int src_b,
                                            int src_left,
                                            int pixel_count,
                                            int blend_type,
                                            const uint8_t* clip_scan) {
  if (blend_type == FXDIB_BLEND_NORMAL && !clip_scan && mask_alpha == 255) {
    FX_ARGB argb = FXARGB_MAKE(0xff, src_r, src_g, src_b);
    for (int col = 0; col < pixel_count; col++) {
      if (src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8))) {
        FXARGB_SETRGBORDERDIB(dest_scan, argb);
      }
      dest_scan += 4;
    }
    return;
  }
  for (int col = 0; col < pixel_count; col++) {
    if (!(src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8)))) {
      dest_scan += 4;
      continue;
    }
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] / 255;
    } else {
      src_alpha = mask_alpha;
    }
    uint8_t back_alpha = dest_scan[3];
    if (back_alpha == 0) {
      FXARGB_SETRGBORDERDIB(dest_scan,
                            FXARGB_MAKE(src_alpha, src_r, src_g, src_b));
      dest_scan += 4;
      continue;
    }
    uint8_t dest_alpha = back_alpha + src_alpha - back_alpha * src_alpha / 255;
    dest_scan[3] = dest_alpha;
    int alpha_ratio = src_alpha * 255 / dest_alpha;
    if (blend_type >= FXDIB_BLEND_NONSEPARABLE) {
      int blended_colors[3];
      uint8_t scan[3] = {static_cast<uint8_t>(src_b),
                         static_cast<uint8_t>(src_g),
                         static_cast<uint8_t>(src_r)};
      uint8_t dest_scan_o[3] = {dest_scan[2], dest_scan[1], dest_scan[0]};
      RGB_Blend(blend_type, scan, dest_scan_o, blended_colors);
      dest_scan[2] =
          FXDIB_ALPHA_MERGE(dest_scan[2], blended_colors[0], alpha_ratio);
      dest_scan[1] =
          FXDIB_ALPHA_MERGE(dest_scan[1], blended_colors[1], alpha_ratio);
      dest_scan[0] =
          FXDIB_ALPHA_MERGE(dest_scan[0], blended_colors[2], alpha_ratio);
    } else if (blend_type) {
      int blended = Blend(blend_type, dest_scan[2], src_b);
      blended = FXDIB_ALPHA_MERGE(src_b, blended, back_alpha);
      dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], blended, alpha_ratio);
      blended = Blend(blend_type, dest_scan[1], src_g);
      blended = FXDIB_ALPHA_MERGE(src_g, blended, back_alpha);
      dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], blended, alpha_ratio);
      blended = Blend(blend_type, dest_scan[0], src_r);
      blended = FXDIB_ALPHA_MERGE(src_r, blended, back_alpha);
      dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], blended, alpha_ratio);
    } else {
      dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], src_b, alpha_ratio);
      dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], src_g, alpha_ratio);
      dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], src_r, alpha_ratio);
    }
    dest_scan += 4;
  }
}

void CompositeRow_BitMask2Rgb_RgbByteOrder(uint8_t* dest_scan,
                                           const uint8_t* src_scan,
                                           int mask_alpha,
                                           int src_r,
                                           int src_g,
                                           int src_b,
                                           int src_left,
                                           int pixel_count,
                                           int blend_type,
                                           int Bpp,
                                           const uint8_t* clip_scan) {
  if (blend_type == FXDIB_BLEND_NORMAL && !clip_scan && mask_alpha == 255) {
    for (int col = 0; col < pixel_count; col++) {
      if (src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8))) {
        dest_scan[2] = src_b;
        dest_scan[1] = src_g;
        dest_scan[0] = src_r;
      }
      dest_scan += Bpp;
    }
    return;
  }
  for (int col = 0; col < pixel_count; col++) {
    if (!(src_scan[(src_left + col) / 8] & (1 << (7 - (src_left + col) % 8)))) {
      dest_scan += Bpp;
      continue;
    }
    int src_alpha;
    if (clip_scan) {
      src_alpha = mask_alpha * clip_scan[col] / 255;
    } else {
      src_alpha = mask_alpha;
    }
    if (src_alpha == 0) {
      dest_scan += Bpp;
      continue;
    }
    if (blend_type >= FXDIB_BLEND_NONSEPARABLE) {
      int blended_colors[3];
      uint8_t scan[3] = {static_cast<uint8_t>(src_b),
                         static_cast<uint8_t>(src_g),
                         static_cast<uint8_t>(src_r)};
      uint8_t dest_scan_o[3] = {dest_scan[2], dest_scan[1], dest_scan[0]};
      RGB_Blend(blend_type, scan, dest_scan_o, blended_colors);
      dest_scan[2] =
          FXDIB_ALPHA_MERGE(dest_scan[2], blended_colors[0], src_alpha);
      dest_scan[1] =
          FXDIB_ALPHA_MERGE(dest_scan[1], blended_colors[1], src_alpha);
      dest_scan[0] =
          FXDIB_ALPHA_MERGE(dest_scan[0], blended_colors[2], src_alpha);
    } else if (blend_type) {
      int back_color = dest_scan[2];
      int blended = Blend(blend_type, back_color, src_b);
      dest_scan[2] = FXDIB_ALPHA_MERGE(back_color, blended, src_alpha);
      back_color = dest_scan[1];
      blended = Blend(blend_type, back_color, src_g);
      dest_scan[1] = FXDIB_ALPHA_MERGE(back_color, blended, src_alpha);
      back_color = dest_scan[0];
      blended = Blend(blend_type, back_color, src_r);
      dest_scan[0] = FXDIB_ALPHA_MERGE(back_color, blended, src_alpha);
    } else {
      dest_scan[2] = FXDIB_ALPHA_MERGE(dest_scan[2], src_b, src_alpha);
      dest_scan[1] = FXDIB_ALPHA_MERGE(dest_scan[1], src_g, src_alpha);
      dest_scan[0] = FXDIB_ALPHA_MERGE(dest_scan[0], src_r, src_alpha);
    }
    dest_scan += Bpp;
  }
}

bool ScanlineCompositor_InitSourceMask(FXDIB_Format dest_format,
                                       int alpha_flag,
                                       uint32_t mask_color,
                                       int& mask_alpha,
                                       int& mask_red,
                                       int& mask_green,
                                       int& mask_blue,
                                       int& mask_black,
                                       CCodec_IccModule* pIccModule,
                                       void* pIccTransform) {
  if (alpha_flag >> 8) {
    mask_alpha = alpha_flag & 0xff;
    mask_red = FXSYS_GetCValue(mask_color);
    mask_green = FXSYS_GetMValue(mask_color);
    mask_blue = FXSYS_GetYValue(mask_color);
    mask_black = FXSYS_GetKValue(mask_color);
  } else {
    mask_alpha = FXARGB_A(mask_color);
    mask_red = FXARGB_R(mask_color);
    mask_green = FXARGB_G(mask_color);
    mask_blue = FXARGB_B(mask_color);
  }
  if (dest_format == FXDIB_8bppMask) {
    return true;
  }
  if ((dest_format & 0xff) == 8) {
    if (pIccTransform) {
      mask_color = (alpha_flag >> 8) ? FXCMYK_TODIB(mask_color)
                                     : FXARGB_TODIB(mask_color);
      uint8_t* gray_p = (uint8_t*)&mask_color;
      pIccModule->TranslateScanline(pIccTransform, gray_p, gray_p, 1);
      mask_red = dest_format & 0x0400 ? FX_CCOLOR(gray_p[0]) : gray_p[0];
    } else {
      if (alpha_flag >> 8) {
        uint8_t r, g, b;
        AdobeCMYK_to_sRGB1(mask_red, mask_green, mask_blue, mask_black, r, g,
                           b);
        mask_red = FXRGB2GRAY(r, g, b);
      } else {
        mask_red = FXRGB2GRAY(mask_red, mask_green, mask_blue);
      }
      if (dest_format & 0x0400) {
        mask_red = FX_CCOLOR(mask_red);
      }
    }
  } else {
    uint8_t* mask_color_p = (uint8_t*)&mask_color;
    mask_color =
        (alpha_flag >> 8) ? FXCMYK_TODIB(mask_color) : FXARGB_TODIB(mask_color);
    if (pIccTransform) {
      pIccModule->TranslateScanline(pIccTransform, mask_color_p, mask_color_p,
                                    1);
      mask_red = mask_color_p[2];
      mask_green = mask_color_p[1];
      mask_blue = mask_color_p[0];
    } else if (alpha_flag >> 8) {
      AdobeCMYK_to_sRGB1(mask_color_p[0], mask_color_p[1], mask_color_p[2],
                         mask_color_p[3], mask_color_p[2], mask_color_p[1],
                         mask_color_p[0]);
      mask_red = mask_color_p[2];
      mask_green = mask_color_p[1];
      mask_blue = mask_color_p[0];
    }
  }
  return true;
}

void ScanlineCompositor_InitSourcePalette(FXDIB_Format src_format,
                                          FXDIB_Format dest_format,
                                          uint32_t*& pDestPalette,
                                          uint32_t* pSrcPalette,
                                          CCodec_IccModule* pIccModule,
                                          void* pIccTransform) {
  bool isSrcCmyk = !!(src_format & 0x0400);
  bool isDstCmyk = !!(dest_format & 0x0400);
  pDestPalette = nullptr;
  if (pIccTransform) {
    if (pSrcPalette) {
      if ((dest_format & 0xff) == 8) {
        int pal_count = 1 << (src_format & 0xff);
        uint8_t* gray_pal = FX_Alloc(uint8_t, pal_count);
        pDestPalette = (uint32_t*)gray_pal;
        for (int i = 0; i < pal_count; i++) {
          uint32_t color = isSrcCmyk ? FXCMYK_TODIB(pSrcPalette[i])
                                     : FXARGB_TODIB(pSrcPalette[i]);
          pIccModule->TranslateScanline(pIccTransform, gray_pal,
                                        (const uint8_t*)&color, 1);
          gray_pal++;
        }
      } else {
        int palsize = 1 << (src_format & 0xff);
        pDestPalette = FX_Alloc(uint32_t, palsize);
        for (int i = 0; i < palsize; i++) {
          uint32_t color = isSrcCmyk ? FXCMYK_TODIB(pSrcPalette[i])
                                     : FXARGB_TODIB(pSrcPalette[i]);
          pIccModule->TranslateScanline(pIccTransform, (uint8_t*)&color,
                                        (const uint8_t*)&color, 1);
          pDestPalette[i] =
              isDstCmyk ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
        }
      }
    } else {
      int pal_count = 1 << (src_format & 0xff);
      uint8_t* gray_pal = FX_Alloc(uint8_t, pal_count);
      if (pal_count == 2) {
        gray_pal[0] = 0;
        gray_pal[1] = 255;
      } else {
        for (int i = 0; i < pal_count; i++) {
          gray_pal[i] = i;
        }
      }
      if ((dest_format & 0xff) == 8) {
        pIccModule->TranslateScanline(pIccTransform, gray_pal, gray_pal,
                                      pal_count);
        pDestPalette = (uint32_t*)gray_pal;
      } else {
        pDestPalette = FX_Alloc(uint32_t, pal_count);
        for (int i = 0; i < pal_count; i++) {
          pIccModule->TranslateScanline(
              pIccTransform, (uint8_t*)&pDestPalette[i], &gray_pal[i], 1);
          pDestPalette[i] = isDstCmyk ? FXCMYK_TODIB(pDestPalette[i])
                                      : FXARGB_TODIB(pDestPalette[i]);
        }
        FX_Free(gray_pal);
      }
    }
  } else {
    if (pSrcPalette) {
      if ((dest_format & 0xff) == 8) {
        int pal_count = 1 << (src_format & 0xff);
        uint8_t* gray_pal = FX_Alloc(uint8_t, pal_count);
        pDestPalette = (uint32_t*)gray_pal;
        if (isSrcCmyk) {
          for (int i = 0; i < pal_count; i++) {
            FX_CMYK cmyk = pSrcPalette[i];
            uint8_t r, g, b;
            AdobeCMYK_to_sRGB1(FXSYS_GetCValue(cmyk), FXSYS_GetMValue(cmyk),
                               FXSYS_GetYValue(cmyk), FXSYS_GetKValue(cmyk), r,
                               g, b);
            *gray_pal++ = FXRGB2GRAY(r, g, b);
          }
        } else {
          for (int i = 0; i < pal_count; i++) {
            FX_ARGB argb = pSrcPalette[i];
            *gray_pal++ =
                FXRGB2GRAY(FXARGB_R(argb), FXARGB_G(argb), FXARGB_B(argb));
          }
        }
      } else {
        int palsize = 1 << (src_format & 0xff);
        pDestPalette = FX_Alloc(uint32_t, palsize);
        if (isDstCmyk == isSrcCmyk) {
          FXSYS_memcpy(pDestPalette, pSrcPalette, palsize * sizeof(uint32_t));
        } else {
          for (int i = 0; i < palsize; i++) {
            FX_CMYK cmyk = pSrcPalette[i];
            uint8_t r, g, b;
            AdobeCMYK_to_sRGB1(FXSYS_GetCValue(cmyk), FXSYS_GetMValue(cmyk),
                               FXSYS_GetYValue(cmyk), FXSYS_GetKValue(cmyk), r,
                               g, b);
            pDestPalette[i] = FXARGB_MAKE(0xff, r, g, b);
          }
        }
      }
    } else {
      if ((dest_format & 0xff) == 8) {
        int pal_count = 1 << (src_format & 0xff);
        uint8_t* gray_pal = FX_Alloc(uint8_t, pal_count);
        if (pal_count == 2) {
          gray_pal[0] = 0;
          gray_pal[1] = 255;
        } else {
          for (int i = 0; i < pal_count; i++) {
            gray_pal[i] = i;
          }
        }
        pDestPalette = (uint32_t*)gray_pal;
      } else {
        int palsize = 1 << (src_format & 0xff);
        pDestPalette = FX_Alloc(uint32_t, palsize);
        if (palsize == 2) {
          pDestPalette[0] = isSrcCmyk ? 255 : 0xff000000;
          pDestPalette[1] = isSrcCmyk ? 0 : 0xffffffff;
        } else {
          for (int i = 0; i < palsize; i++) {
            pDestPalette[i] = isSrcCmyk ? FX_CCOLOR(i) : (i * 0x10101);
          }
        }
        if (isSrcCmyk != isDstCmyk) {
          for (int i = 0; i < palsize; i++) {
            FX_CMYK cmyk = pDestPalette[i];
            uint8_t r, g, b;
            AdobeCMYK_to_sRGB1(FXSYS_GetCValue(cmyk), FXSYS_GetMValue(cmyk),
                               FXSYS_GetYValue(cmyk), FXSYS_GetKValue(cmyk), r,
                               g, b);
            pDestPalette[i] = FXARGB_MAKE(0xff, r, g, b);
          }
        }
      }
    }
  }
}

}  // namespace

CFX_ScanlineCompositor::CFX_ScanlineCompositor() {
  m_pSrcPalette = nullptr;
  m_pCacheScanline = nullptr;
  m_CacheSize = 0;
  m_bRgbByteOrder = false;
  m_BlendType = FXDIB_BLEND_NORMAL;
  m_pIccTransform = nullptr;
}

CFX_ScanlineCompositor::~CFX_ScanlineCompositor() {
  FX_Free(m_pSrcPalette);
  FX_Free(m_pCacheScanline);
}

bool CFX_ScanlineCompositor::Init(FXDIB_Format dest_format,
                                  FXDIB_Format src_format,
                                  int32_t width,
                                  uint32_t* pSrcPalette,
                                  uint32_t mask_color,
                                  int blend_type,
                                  bool bClip,
                                  bool bRgbByteOrder,
                                  int alpha_flag,
                                  void* pIccTransform) {
  m_SrcFormat = src_format;
  m_DestFormat = dest_format;
  m_BlendType = blend_type;
  m_bRgbByteOrder = bRgbByteOrder;
  CCodec_IccModule* pIccModule = nullptr;
  if (CFX_GEModule::Get()->GetCodecModule()) {
    pIccModule = CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
  }
  if (!pIccModule) {
    pIccTransform = nullptr;
  }
  m_pIccTransform = pIccTransform;
  if ((dest_format & 0xff) == 1) {
    return false;
  }
  if (m_SrcFormat == FXDIB_1bppMask || m_SrcFormat == FXDIB_8bppMask) {
    return ScanlineCompositor_InitSourceMask(
        dest_format, alpha_flag, mask_color, m_MaskAlpha, m_MaskRed,
        m_MaskGreen, m_MaskBlue, m_MaskBlack, pIccModule, pIccTransform);
  }
  if (!pIccTransform && (~src_format & 0x0400) && (dest_format & 0x0400)) {
    return false;
  }
  if ((m_SrcFormat & 0xff) <= 8) {
    if (dest_format == FXDIB_8bppMask) {
      return true;
    }
    ScanlineCompositor_InitSourcePalette(src_format, dest_format, m_pSrcPalette,
                                         pSrcPalette, pIccModule,
                                         pIccTransform);
    m_Transparency =
        (dest_format == FXDIB_Argb ? 1 : 0) + (dest_format & 0x0200 ? 2 : 0) +
        (dest_format & 0x0400 ? 4 : 0) + ((src_format & 0xff) == 1 ? 8 : 0);
    return true;
  }
  m_Transparency = (src_format & 0x0200 ? 0 : 1) +
                   (dest_format & 0x0200 ? 0 : 2) +
                   (blend_type == FXDIB_BLEND_NORMAL ? 4 : 0) +
                   (bClip ? 8 : 0) + (src_format & 0x0400 ? 16 : 0) +
                   (dest_format & 0x0400 ? 32 : 0) + (pIccTransform ? 64 : 0);
  return true;
}

void CFX_ScanlineCompositor::CompositeRgbBitmapLine(
    uint8_t* dest_scan,
    const uint8_t* src_scan,
    int width,
    const uint8_t* clip_scan,
    const uint8_t* src_extra_alpha,
    uint8_t* dst_extra_alpha) {
  int src_Bpp = (m_SrcFormat & 0xff) >> 3;
  int dest_Bpp = (m_DestFormat & 0xff) >> 3;
  if (m_bRgbByteOrder) {
    switch (m_Transparency) {
      case 0:
      case 4:
      case 8:
      case 12:
        CompositeRow_Argb2Argb_RgbByteOrder(dest_scan, src_scan, width,
                                            m_BlendType, clip_scan);
        break;
      case 1:
        CompositeRow_Rgb2Argb_Blend_NoClip_RgbByteOrder(
            dest_scan, src_scan, width, m_BlendType, src_Bpp);
        break;
      case 2:
      case 10:
        CompositeRow_Argb2Rgb_Blend_RgbByteOrder(
            dest_scan, src_scan, width, m_BlendType, dest_Bpp, clip_scan);
        break;
      case 3:
        CompositeRow_Rgb2Rgb_Blend_NoClip_RgbByteOrder(
            dest_scan, src_scan, width, m_BlendType, dest_Bpp, src_Bpp);
        break;
      case 5:
        CompositeRow_Rgb2Argb_NoBlend_NoClip_RgbByteOrder(dest_scan, src_scan,
                                                          width, src_Bpp);
        break;
      case 6:
      case 14:
        CompositeRow_Argb2Rgb_NoBlend_RgbByteOrder(dest_scan, src_scan, width,
                                                   dest_Bpp, clip_scan);
        break;
      case 7:
        CompositeRow_Rgb2Rgb_NoBlend_NoClip_RgbByteOrder(
            dest_scan, src_scan, width, dest_Bpp, src_Bpp);
        break;
      case 9:
        CompositeRow_Rgb2Argb_Blend_Clip_RgbByteOrder(
            dest_scan, src_scan, width, m_BlendType, src_Bpp, clip_scan);
        break;
      case 11:
        CompositeRow_Rgb2Rgb_Blend_Clip_RgbByteOrder(dest_scan, src_scan, width,
                                                     m_BlendType, dest_Bpp,
                                                     src_Bpp, clip_scan);
        break;
      case 13:
        CompositeRow_Rgb2Argb_NoBlend_Clip_RgbByteOrder(
            dest_scan, src_scan, width, src_Bpp, clip_scan);
        break;
      case 15:
        CompositeRow_Rgb2Rgb_NoBlend_Clip_RgbByteOrder(
            dest_scan, src_scan, width, dest_Bpp, src_Bpp, clip_scan);
        break;
    }
    return;
  }
  if (m_DestFormat == FXDIB_8bppMask) {
    if (m_SrcFormat & 0x0200) {
      if (m_SrcFormat == FXDIB_Argb) {
        CompositeRow_Argb2Mask(dest_scan, src_scan, width, clip_scan);
      } else {
        CompositeRow_Rgba2Mask(dest_scan, src_extra_alpha, width, clip_scan);
      }
    } else {
      CompositeRow_Rgb2Mask(dest_scan, src_scan, width, clip_scan);
    }
  } else if ((m_DestFormat & 0xff) == 8) {
    if (m_DestFormat & 0x0400) {
      for (int i = 0; i < width; i++) {
        *dest_scan = ~*dest_scan;
        dest_scan++;
      }
    }
    if (m_SrcFormat & 0x0200) {
      if (m_DestFormat & 0x0200) {
        CompositeRow_Argb2Graya(dest_scan, src_scan, width, m_BlendType,
                                clip_scan, src_extra_alpha, dst_extra_alpha,
                                m_pIccTransform);
      } else {
        CompositeRow_Argb2Gray(dest_scan, src_scan, width, m_BlendType,
                               clip_scan, src_extra_alpha, m_pIccTransform);
      }
    } else {
      if (m_DestFormat & 0x0200) {
        CompositeRow_Rgb2Graya(dest_scan, src_scan, src_Bpp, width, m_BlendType,
                               clip_scan, dst_extra_alpha, m_pIccTransform);
      } else {
        CompositeRow_Rgb2Gray(dest_scan, src_scan, src_Bpp, width, m_BlendType,
                              clip_scan, m_pIccTransform);
      }
    }
    if (m_DestFormat & 0x0400) {
      for (int i = 0; i < width; i++) {
        *dest_scan = ~*dest_scan;
        dest_scan++;
      }
    }
  } else {
    int dest_Size = width * dest_Bpp + 4;
    if (dest_Size > m_CacheSize) {
      m_pCacheScanline = FX_Realloc(uint8_t, m_pCacheScanline, dest_Size);
      if (!m_pCacheScanline) {
        return;
      }
      m_CacheSize = dest_Size;
    }
    switch (m_Transparency) {
      case 0:
      case 4:
      case 8:
      case 4 + 8: {
        CompositeRow_Argb2Argb(dest_scan, src_scan, width, m_BlendType,
                               clip_scan, dst_extra_alpha, src_extra_alpha);
      } break;
      case 64:
      case 4 + 64:
      case 8 + 64:
      case 4 + 8 + 64: {
        CompositeRow_Argb2Argb_Transform(
            dest_scan, src_scan, width, m_BlendType, clip_scan, dst_extra_alpha,
            src_extra_alpha, m_pCacheScanline, m_pIccTransform);
      } break;
      case 1:
        CompositeRow_Rgb2Argb_Blend_NoClip(
            dest_scan, src_scan, width, m_BlendType, src_Bpp, dst_extra_alpha);
        break;
      case 1 + 64:
        CompositeRow_Rgb2Argb_Blend_NoClip_Transform(
            dest_scan, src_scan, width, m_BlendType, src_Bpp, dst_extra_alpha,
            m_pCacheScanline, m_pIccTransform);
        break;
      case 1 + 8:
        CompositeRow_Rgb2Argb_Blend_Clip(dest_scan, src_scan, width,
                                         m_BlendType, src_Bpp, clip_scan,
                                         dst_extra_alpha);
        break;
      case 1 + 8 + 64:
        CompositeRow_Rgb2Argb_Blend_Clip_Transform(
            dest_scan, src_scan, width, m_BlendType, src_Bpp, clip_scan,
            dst_extra_alpha, m_pCacheScanline, m_pIccTransform);
        break;
      case 1 + 4:
        CompositeRow_Rgb2Argb_NoBlend_NoClip(dest_scan, src_scan, width,
                                             src_Bpp, dst_extra_alpha);
        break;
      case 1 + 4 + 64:
        CompositeRow_Rgb2Argb_NoBlend_NoClip_Transform(
            dest_scan, src_scan, width, src_Bpp, dst_extra_alpha,
            m_pCacheScanline, m_pIccTransform);
        break;
      case 1 + 4 + 8:
        CompositeRow_Rgb2Argb_NoBlend_Clip(dest_scan, src_scan, width, src_Bpp,
                                           clip_scan, dst_extra_alpha);
        break;
      case 1 + 4 + 8 + 64:
        CompositeRow_Rgb2Argb_NoBlend_Clip_Transform(
            dest_scan, src_scan, width, src_Bpp, clip_scan, dst_extra_alpha,
            m_pCacheScanline, m_pIccTransform);
        break;
      case 2:
      case 2 + 8:
        CompositeRow_Argb2Rgb_Blend(dest_scan, src_scan, width, m_BlendType,
                                    dest_Bpp, clip_scan, src_extra_alpha);
        break;
      case 2 + 64:
      case 2 + 8 + 64:
        CompositeRow_Argb2Rgb_Blend_Transform(
            dest_scan, src_scan, width, m_BlendType, dest_Bpp, clip_scan,
            src_extra_alpha, m_pCacheScanline, m_pIccTransform);
        break;
      case 2 + 4:
      case 2 + 4 + 8:
        CompositeRow_Argb2Rgb_NoBlend(dest_scan, src_scan, width, dest_Bpp,
                                      clip_scan, src_extra_alpha);
        break;
      case 2 + 4 + 64:
      case 2 + 4 + 8 + 64:
        CompositeRow_Argb2Rgb_NoBlend_Transform(
            dest_scan, src_scan, width, dest_Bpp, clip_scan, src_extra_alpha,
            m_pCacheScanline, m_pIccTransform);
        break;
      case 1 + 2:
        CompositeRow_Rgb2Rgb_Blend_NoClip(dest_scan, src_scan, width,
                                          m_BlendType, dest_Bpp, src_Bpp);
        break;
      case 1 + 2 + 64:
        CompositeRow_Rgb2Rgb_Blend_NoClip_Transform(
            dest_scan, src_scan, width, m_BlendType, dest_Bpp, src_Bpp,
            m_pCacheScanline, m_pIccTransform);
        break;
      case 1 + 2 + 8:
        CompositeRow_Rgb2Rgb_Blend_Clip(dest_scan, src_scan, width, m_BlendType,
                                        dest_Bpp, src_Bpp, clip_scan);
        break;
      case 1 + 2 + 8 + 64:
        CompositeRow_Rgb2Rgb_Blend_Clip_Transform(
            dest_scan, src_scan, width, m_BlendType, dest_Bpp, src_Bpp,
            clip_scan, m_pCacheScanline, m_pIccTransform);
        break;
      case 1 + 2 + 4:
        CompositeRow_Rgb2Rgb_NoBlend_NoClip(dest_scan, src_scan, width,
                                            dest_Bpp, src_Bpp);
        break;
      case 1 + 2 + 4 + 64:
        CompositeRow_Rgb2Rgb_NoBlend_NoClip_Transform(
            dest_scan, src_scan, width, dest_Bpp, src_Bpp, m_pCacheScanline,
            m_pIccTransform);
        break;
      case 1 + 2 + 4 + 8:
        CompositeRow_Rgb2Rgb_NoBlend_Clip(dest_scan, src_scan, width, dest_Bpp,
                                          src_Bpp, clip_scan);
        break;
      case 1 + 2 + 4 + 8 + 64:
        CompositeRow_Rgb2Rgb_NoBlend_Clip_Transform(
            dest_scan, src_scan, width, dest_Bpp, src_Bpp, clip_scan,
            m_pCacheScanline, m_pIccTransform);
        break;
    }
  }
}

void CFX_ScanlineCompositor::CompositePalBitmapLine(
    uint8_t* dest_scan,
    const uint8_t* src_scan,
    int src_left,
    int width,
    const uint8_t* clip_scan,
    const uint8_t* src_extra_alpha,
    uint8_t* dst_extra_alpha) {
  if (m_bRgbByteOrder) {
    if (m_SrcFormat == FXDIB_1bppRgb) {
      if (m_DestFormat == FXDIB_8bppRgb) {
        return;
      }
      if (m_DestFormat == FXDIB_Argb) {
        CompositeRow_1bppRgb2Argb_NoBlend_RgbByteOrder(
            dest_scan, src_scan, src_left, width, m_pSrcPalette, clip_scan);
      } else {
        CompositeRow_1bppRgb2Rgb_NoBlend_RgbByteOrder(
            dest_scan, src_scan, src_left, m_pSrcPalette, width,
            (m_DestFormat & 0xff) >> 3, clip_scan);
      }
    } else {
      if (m_DestFormat == FXDIB_8bppRgb) {
        return;
      }
      if (m_DestFormat == FXDIB_Argb) {
        CompositeRow_8bppRgb2Argb_NoBlend_RgbByteOrder(
            dest_scan, src_scan, width, m_pSrcPalette, clip_scan);
      } else {
        CompositeRow_8bppRgb2Rgb_NoBlend_RgbByteOrder(
            dest_scan, src_scan, m_pSrcPalette, width,
            (m_DestFormat & 0xff) >> 3, clip_scan);
      }
    }
    return;
  }
  if (m_DestFormat == FXDIB_8bppMask) {
    CompositeRow_Rgb2Mask(dest_scan, src_scan, width, clip_scan);
    return;
  }
  if ((m_DestFormat & 0xff) == 8) {
    if (m_Transparency & 8) {
      if (m_DestFormat & 0x0200) {
        CompositeRow_1bppPal2Graya(dest_scan, src_scan, src_left,
                                   (const uint8_t*)m_pSrcPalette, width,
                                   m_BlendType, clip_scan, dst_extra_alpha);
      } else {
        CompositeRow_1bppPal2Gray(dest_scan, src_scan, src_left,
                                  (const uint8_t*)m_pSrcPalette, width,
                                  m_BlendType, clip_scan);
      }
    } else {
      if (m_DestFormat & 0x0200)
        CompositeRow_8bppPal2Graya(
            dest_scan, src_scan, (const uint8_t*)m_pSrcPalette, width,
            m_BlendType, clip_scan, dst_extra_alpha, src_extra_alpha);
      else
        CompositeRow_8bppPal2Gray(dest_scan, src_scan,
                                  (const uint8_t*)m_pSrcPalette, width,
                                  m_BlendType, clip_scan, src_extra_alpha);
    }
  } else {
    switch (m_Transparency) {
      case 1 + 2:
        CompositeRow_8bppRgb2Argb_NoBlend(dest_scan, src_scan, width,
                                          m_pSrcPalette, clip_scan,
                                          src_extra_alpha);
        break;
      case 1 + 2 + 8:
        CompositeRow_1bppRgb2Argb_NoBlend(dest_scan, src_scan, src_left, width,
                                          m_pSrcPalette, clip_scan);
        break;
      case 0:
        CompositeRow_8bppRgb2Rgb_NoBlend(dest_scan, src_scan, m_pSrcPalette,
                                         width, (m_DestFormat & 0xff) >> 3,
                                         clip_scan, src_extra_alpha);
        break;
      case 0 + 8:
        CompositeRow_1bppRgb2Rgb_NoBlend(dest_scan, src_scan, src_left,
                                         m_pSrcPalette, width,
                                         (m_DestFormat & 0xff) >> 3, clip_scan);
        break;
      case 0 + 2:
        CompositeRow_8bppRgb2Rgb_NoBlend(dest_scan, src_scan, m_pSrcPalette,
                                         width, (m_DestFormat & 0xff) >> 3,
                                         clip_scan, src_extra_alpha);
        break;
      case 0 + 2 + 8:
        CompositeRow_1bppRgb2Rgba_NoBlend(dest_scan, src_scan, src_left, width,
                                          m_pSrcPalette, clip_scan,
                                          dst_extra_alpha);
        break;
        break;
    }
  }
}

void CFX_ScanlineCompositor::CompositeByteMaskLine(uint8_t* dest_scan,
                                                   const uint8_t* src_scan,
                                                   int width,
                                                   const uint8_t* clip_scan,
                                                   uint8_t* dst_extra_alpha) {
  if (m_DestFormat == FXDIB_8bppMask) {
    CompositeRow_ByteMask2Mask(dest_scan, src_scan, m_MaskAlpha, width,
                               clip_scan);
  } else if ((m_DestFormat & 0xff) == 8) {
    if (m_DestFormat & 0x0200) {
      CompositeRow_ByteMask2Graya(dest_scan, src_scan, m_MaskAlpha, m_MaskRed,
                                  width, clip_scan, dst_extra_alpha);
    } else {
      CompositeRow_ByteMask2Gray(dest_scan, src_scan, m_MaskAlpha, m_MaskRed,
                                 width, clip_scan);
    }
  } else if (m_bRgbByteOrder) {
    if (m_DestFormat == FXDIB_Argb) {
      CompositeRow_ByteMask2Argb_RgbByteOrder(
          dest_scan, src_scan, m_MaskAlpha, m_MaskRed, m_MaskGreen, m_MaskBlue,
          width, m_BlendType, clip_scan);
    } else {
      CompositeRow_ByteMask2Rgb_RgbByteOrder(
          dest_scan, src_scan, m_MaskAlpha, m_MaskRed, m_MaskGreen, m_MaskBlue,
          width, m_BlendType, (m_DestFormat & 0xff) >> 3, clip_scan);
    }
    return;
  } else if (m_DestFormat == FXDIB_Argb) {
    CompositeRow_ByteMask2Argb(dest_scan, src_scan, m_MaskAlpha, m_MaskRed,
                               m_MaskGreen, m_MaskBlue, width, m_BlendType,
                               clip_scan);
  } else if (m_DestFormat == FXDIB_Rgb || m_DestFormat == FXDIB_Rgb32) {
    CompositeRow_ByteMask2Rgb(dest_scan, src_scan, m_MaskAlpha, m_MaskRed,
                              m_MaskGreen, m_MaskBlue, width, m_BlendType,
                              (m_DestFormat & 0xff) >> 3, clip_scan);
  } else if (m_DestFormat == FXDIB_Rgba) {
    CompositeRow_ByteMask2Rgba(dest_scan, src_scan, m_MaskAlpha, m_MaskRed,
                               m_MaskGreen, m_MaskBlue, width, m_BlendType,
                               clip_scan, dst_extra_alpha);
  }
}

void CFX_ScanlineCompositor::CompositeBitMaskLine(uint8_t* dest_scan,
                                                  const uint8_t* src_scan,
                                                  int src_left,
                                                  int width,
                                                  const uint8_t* clip_scan,
                                                  uint8_t* dst_extra_alpha) {
  if (m_DestFormat == FXDIB_8bppMask) {
    CompositeRow_BitMask2Mask(dest_scan, src_scan, m_MaskAlpha, src_left, width,
                              clip_scan);
  } else if ((m_DestFormat & 0xff) == 8) {
    if (m_DestFormat & 0x0200) {
      CompositeRow_BitMask2Graya(dest_scan, src_scan, m_MaskAlpha, m_MaskRed,
                                 src_left, width, clip_scan, dst_extra_alpha);
    } else {
      CompositeRow_BitMask2Gray(dest_scan, src_scan, m_MaskAlpha, m_MaskRed,
                                src_left, width, clip_scan);
    }
  } else if (m_bRgbByteOrder) {
    if (m_DestFormat == FXDIB_Argb) {
      CompositeRow_BitMask2Argb_RgbByteOrder(
          dest_scan, src_scan, m_MaskAlpha, m_MaskRed, m_MaskGreen, m_MaskBlue,
          src_left, width, m_BlendType, clip_scan);
    } else {
      CompositeRow_BitMask2Rgb_RgbByteOrder(
          dest_scan, src_scan, m_MaskAlpha, m_MaskRed, m_MaskGreen, m_MaskBlue,
          src_left, width, m_BlendType, (m_DestFormat & 0xff) >> 3, clip_scan);
    }
    return;
  } else if (m_DestFormat == FXDIB_Argb) {
    CompositeRow_BitMask2Argb(dest_scan, src_scan, m_MaskAlpha, m_MaskRed,
                              m_MaskGreen, m_MaskBlue, src_left, width,
                              m_BlendType, clip_scan);
  } else if (m_DestFormat == FXDIB_Rgb || m_DestFormat == FXDIB_Rgb32) {
    CompositeRow_BitMask2Rgb(
        dest_scan, src_scan, m_MaskAlpha, m_MaskRed, m_MaskGreen, m_MaskBlue,
        src_left, width, m_BlendType, (m_DestFormat & 0xff) >> 3, clip_scan);
  }
}

bool CFX_DIBitmap::CompositeBitmap(int dest_left,
                                   int dest_top,
                                   int width,
                                   int height,
                                   const CFX_DIBSource* pSrcBitmap,
                                   int src_left,
                                   int src_top,
                                   int blend_type,
                                   const CFX_ClipRgn* pClipRgn,
                                   bool bRgbByteOrder,
                                   void* pIccTransform) {
  if (!m_pBuffer) {
    return false;
  }
  ASSERT(!pSrcBitmap->IsAlphaMask());
  ASSERT(m_bpp >= 8);
  if (pSrcBitmap->IsAlphaMask() || m_bpp < 8) {
    return false;
  }
  GetOverlapRect(dest_left, dest_top, width, height, pSrcBitmap->GetWidth(),
                 pSrcBitmap->GetHeight(), src_left, src_top, pClipRgn);
  if (width == 0 || height == 0) {
    return true;
  }
  const CFX_DIBitmap* pClipMask = nullptr;
  FX_RECT clip_box;
  if (pClipRgn && pClipRgn->GetType() != CFX_ClipRgn::RectI) {
    ASSERT(pClipRgn->GetType() == CFX_ClipRgn::MaskF);
    pClipMask = pClipRgn->GetMask().GetObject();
    clip_box = pClipRgn->GetBox();
  }
  CFX_ScanlineCompositor compositor;
  if (!compositor.Init(GetFormat(), pSrcBitmap->GetFormat(), width,
                       pSrcBitmap->GetPalette(), 0, blend_type,
                       pClipMask != nullptr, bRgbByteOrder, 0, pIccTransform)) {
    return false;
  }
  int dest_Bpp = m_bpp / 8;
  int src_Bpp = pSrcBitmap->GetBPP() / 8;
  bool bRgb = src_Bpp > 1 && !pSrcBitmap->IsCmykImage();
  CFX_DIBitmap* pSrcAlphaMask = pSrcBitmap->m_pAlphaMask;
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan =
        m_pBuffer + (dest_top + row) * m_Pitch + dest_left * dest_Bpp;
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row) + src_left * src_Bpp;
    const uint8_t* src_scan_extra_alpha =
        pSrcAlphaMask ? pSrcAlphaMask->GetScanline(src_top + row) + src_left
                      : nullptr;
    uint8_t* dst_scan_extra_alpha =
        m_pAlphaMask
            ? (uint8_t*)m_pAlphaMask->GetScanline(dest_top + row) + dest_left
            : nullptr;
    const uint8_t* clip_scan = nullptr;
    if (pClipMask) {
      clip_scan = pClipMask->m_pBuffer +
                  (dest_top + row - clip_box.top) * pClipMask->m_Pitch +
                  (dest_left - clip_box.left);
    }
    if (bRgb) {
      compositor.CompositeRgbBitmapLine(dest_scan, src_scan, width, clip_scan,
                                        src_scan_extra_alpha,
                                        dst_scan_extra_alpha);
    } else {
      compositor.CompositePalBitmapLine(dest_scan, src_scan, src_left, width,
                                        clip_scan, src_scan_extra_alpha,
                                        dst_scan_extra_alpha);
    }
  }
  return true;
}

bool CFX_DIBitmap::CompositeMask(int dest_left,
                                 int dest_top,
                                 int width,
                                 int height,
                                 const CFX_DIBSource* pMask,
                                 uint32_t color,
                                 int src_left,
                                 int src_top,
                                 int blend_type,
                                 const CFX_ClipRgn* pClipRgn,
                                 bool bRgbByteOrder,
                                 int alpha_flag,
                                 void* pIccTransform) {
  if (!m_pBuffer) {
    return false;
  }
  ASSERT(pMask->IsAlphaMask());
  ASSERT(m_bpp >= 8);
  if (!pMask->IsAlphaMask() || m_bpp < 8) {
    return false;
  }
  GetOverlapRect(dest_left, dest_top, width, height, pMask->GetWidth(),
                 pMask->GetHeight(), src_left, src_top, pClipRgn);
  if (width == 0 || height == 0) {
    return true;
  }
  int src_alpha =
      (uint8_t)(alpha_flag >> 8) ? (alpha_flag & 0xff) : FXARGB_A(color);
  if (src_alpha == 0) {
    return true;
  }
  const CFX_DIBitmap* pClipMask = nullptr;
  FX_RECT clip_box;
  if (pClipRgn && pClipRgn->GetType() != CFX_ClipRgn::RectI) {
    ASSERT(pClipRgn->GetType() == CFX_ClipRgn::MaskF);
    pClipMask = pClipRgn->GetMask().GetObject();
    clip_box = pClipRgn->GetBox();
  }
  int src_bpp = pMask->GetBPP();
  int Bpp = GetBPP() / 8;
  CFX_ScanlineCompositor compositor;
  if (!compositor.Init(GetFormat(), pMask->GetFormat(), width, nullptr, color,
                       blend_type, pClipMask != nullptr, bRgbByteOrder,
                       alpha_flag, pIccTransform)) {
    return false;
  }
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan =
        m_pBuffer + (dest_top + row) * m_Pitch + dest_left * Bpp;
    const uint8_t* src_scan = pMask->GetScanline(src_top + row);
    uint8_t* dst_scan_extra_alpha =
        m_pAlphaMask
            ? (uint8_t*)m_pAlphaMask->GetScanline(dest_top + row) + dest_left
            : nullptr;
    const uint8_t* clip_scan = nullptr;
    if (pClipMask) {
      clip_scan = pClipMask->m_pBuffer +
                  (dest_top + row - clip_box.top) * pClipMask->m_Pitch +
                  (dest_left - clip_box.left);
    }
    if (src_bpp == 1) {
      compositor.CompositeBitMaskLine(dest_scan, src_scan, src_left, width,
                                      clip_scan, dst_scan_extra_alpha);
    } else {
      compositor.CompositeByteMaskLine(dest_scan, src_scan + src_left, width,
                                       clip_scan, dst_scan_extra_alpha);
    }
  }
  return true;
}

bool CFX_DIBitmap::CompositeRect(int left,
                                 int top,
                                 int width,
                                 int height,
                                 uint32_t color,
                                 int alpha_flag,
                                 void* pIccTransform) {
  if (!m_pBuffer) {
    return false;
  }
  int src_alpha = (alpha_flag >> 8) ? (alpha_flag & 0xff) : FXARGB_A(color);
  if (src_alpha == 0) {
    return true;
  }
  FX_RECT rect(left, top, left + width, top + height);
  rect.Intersect(0, 0, m_Width, m_Height);
  if (rect.IsEmpty()) {
    return true;
  }
  width = rect.Width();
  uint32_t dst_color;
  if (alpha_flag >> 8) {
    dst_color = FXCMYK_TODIB(color);
  } else {
    dst_color = FXARGB_TODIB(color);
  }
  uint8_t* color_p = (uint8_t*)&dst_color;
  if (m_bpp == 8) {
    uint8_t gray = 255;
    if (!IsAlphaMask()) {
      if (pIccTransform && CFX_GEModule::Get()->GetCodecModule() &&
          CFX_GEModule::Get()->GetCodecModule()->GetIccModule()) {
        CCodec_IccModule* pIccModule =
            CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
        pIccModule->TranslateScanline(pIccTransform, &gray, color_p, 1);
      } else {
        if (alpha_flag >> 8) {
          uint8_t r, g, b;
          AdobeCMYK_to_sRGB1(color_p[0], color_p[1], color_p[2], color_p[3], r,
                             g, b);
          gray = FXRGB2GRAY(r, g, b);
        } else {
          gray = (uint8_t)FXRGB2GRAY((int)color_p[2], color_p[1], color_p[0]);
        }
      }
      if (IsCmykImage()) {
        gray = ~gray;
      }
    }
    for (int row = rect.top; row < rect.bottom; row++) {
      uint8_t* dest_scan = m_pBuffer + row * m_Pitch + rect.left;
      if (src_alpha == 255) {
        FXSYS_memset(dest_scan, gray, width);
      } else {
        for (int col = 0; col < width; col++) {
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, src_alpha);
          dest_scan++;
        }
      }
    }
    return true;
  }
  if (m_bpp == 1) {
    ASSERT(!IsCmykImage() && (uint8_t)(alpha_flag >> 8) == 0);
    int left_shift = rect.left % 8;
    int right_shift = rect.right % 8;
    int new_width = rect.right / 8 - rect.left / 8;
    int index = 0;
    if (m_pPalette) {
      for (int i = 0; i < 2; i++) {
        if (m_pPalette.get()[i] == color) {
          index = i;
        }
      }
    } else {
      index = ((uint8_t)color == 0xff) ? 1 : 0;
    }
    for (int row = rect.top; row < rect.bottom; row++) {
      uint8_t* dest_scan_top = (uint8_t*)GetScanline(row) + rect.left / 8;
      uint8_t* dest_scan_top_r = (uint8_t*)GetScanline(row) + rect.right / 8;
      uint8_t left_flag = *dest_scan_top & (255 << (8 - left_shift));
      uint8_t right_flag = *dest_scan_top_r & (255 >> right_shift);
      if (new_width) {
        FXSYS_memset(dest_scan_top + 1, index ? 255 : 0, new_width - 1);
        if (!index) {
          *dest_scan_top &= left_flag;
          *dest_scan_top_r &= right_flag;
        } else {
          *dest_scan_top |= ~left_flag;
          *dest_scan_top_r |= ~right_flag;
        }
      } else {
        if (!index) {
          *dest_scan_top &= left_flag | right_flag;
        } else {
          *dest_scan_top |= ~(left_flag | right_flag);
        }
      }
    }
    return true;
  }
  ASSERT(m_bpp >= 24);
  if (m_bpp < 24) {
    return false;
  }
  if (pIccTransform && CFX_GEModule::Get()->GetCodecModule()) {
    CCodec_IccModule* pIccModule =
        CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
    pIccModule->TranslateScanline(pIccTransform, color_p, color_p, 1);
  } else {
    if (alpha_flag >> 8 && !IsCmykImage()) {
      AdobeCMYK_to_sRGB1(FXSYS_GetCValue(color), FXSYS_GetMValue(color),
                         FXSYS_GetYValue(color), FXSYS_GetKValue(color),
                         color_p[2], color_p[1], color_p[0]);
    } else if (!(alpha_flag >> 8) && IsCmykImage()) {
      return false;
    }
  }
  if (!IsCmykImage()) {
    color_p[3] = (uint8_t)src_alpha;
  }
  int Bpp = m_bpp / 8;
  bool bAlpha = HasAlpha();
  bool bArgb = GetFormat() == FXDIB_Argb;
  if (src_alpha == 255) {
    for (int row = rect.top; row < rect.bottom; row++) {
      uint8_t* dest_scan = m_pBuffer + row * m_Pitch + rect.left * Bpp;
      uint8_t* dest_scan_alpha =
          m_pAlphaMask ? (uint8_t*)m_pAlphaMask->GetScanline(row) + rect.left
                       : nullptr;
      if (dest_scan_alpha) {
        FXSYS_memset(dest_scan_alpha, 0xff, width);
      }
      if (Bpp == 4) {
        uint32_t* scan = (uint32_t*)dest_scan;
        for (int col = 0; col < width; col++) {
          *scan++ = dst_color;
        }
      } else {
        for (int col = 0; col < width; col++) {
          *dest_scan++ = color_p[0];
          *dest_scan++ = color_p[1];
          *dest_scan++ = color_p[2];
        }
      }
    }
    return true;
  }
  for (int row = rect.top; row < rect.bottom; row++) {
    uint8_t* dest_scan = m_pBuffer + row * m_Pitch + rect.left * Bpp;
    if (bAlpha) {
      if (bArgb) {
        for (int col = 0; col < width; col++) {
          uint8_t back_alpha = dest_scan[3];
          if (back_alpha == 0) {
            FXARGB_SETDIB(dest_scan, FXARGB_MAKE(src_alpha, color_p[2],
                                                 color_p[1], color_p[0]));
            dest_scan += 4;
            continue;
          }
          uint8_t dest_alpha =
              back_alpha + src_alpha - back_alpha * src_alpha / 255;
          int alpha_ratio = src_alpha * 255 / dest_alpha;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, color_p[0], alpha_ratio);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, color_p[1], alpha_ratio);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, color_p[2], alpha_ratio);
          dest_scan++;
          *dest_scan++ = dest_alpha;
        }
      } else {
        uint8_t* dest_scan_alpha =
            (uint8_t*)m_pAlphaMask->GetScanline(row) + rect.left;
        for (int col = 0; col < width; col++) {
          uint8_t back_alpha = *dest_scan_alpha;
          if (back_alpha == 0) {
            *dest_scan_alpha++ = src_alpha;
            FXSYS_memcpy(dest_scan, color_p, Bpp);
            dest_scan += Bpp;
            continue;
          }
          uint8_t dest_alpha =
              back_alpha + src_alpha - back_alpha * src_alpha / 255;
          *dest_scan_alpha++ = dest_alpha;
          int alpha_ratio = src_alpha * 255 / dest_alpha;
          for (int comps = 0; comps < Bpp; comps++) {
            *dest_scan =
                FXDIB_ALPHA_MERGE(*dest_scan, color_p[comps], alpha_ratio);
            dest_scan++;
          }
        }
      }
    } else {
      for (int col = 0; col < width; col++) {
        for (int comps = 0; comps < Bpp; comps++) {
          if (comps == 3) {
            *dest_scan++ = 255;
            continue;
          }
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, color_p[comps], src_alpha);
          dest_scan++;
        }
      }
    }
  }
  return true;
}

CFX_BitmapComposer::CFX_BitmapComposer() {
  m_pScanlineV = nullptr;
  m_pScanlineAlphaV = nullptr;
  m_pClipScanV = nullptr;
  m_pAddClipScan = nullptr;
  m_bRgbByteOrder = false;
  m_BlendType = FXDIB_BLEND_NORMAL;
}

CFX_BitmapComposer::~CFX_BitmapComposer() {
  FX_Free(m_pScanlineV);
  FX_Free(m_pScanlineAlphaV);
  FX_Free(m_pClipScanV);
  FX_Free(m_pAddClipScan);
}

void CFX_BitmapComposer::Compose(CFX_DIBitmap* pDest,
                                 const CFX_ClipRgn* pClipRgn,
                                 int bitmap_alpha,
                                 uint32_t mask_color,
                                 FX_RECT& dest_rect,
                                 bool bVertical,
                                 bool bFlipX,
                                 bool bFlipY,
                                 bool bRgbByteOrder,
                                 int alpha_flag,
                                 void* pIccTransform,
                                 int blend_type) {
  m_pBitmap = pDest;
  m_pClipRgn = pClipRgn;
  m_DestLeft = dest_rect.left;
  m_DestTop = dest_rect.top;
  m_DestWidth = dest_rect.Width();
  m_DestHeight = dest_rect.Height();
  m_BitmapAlpha = bitmap_alpha;
  m_MaskColor = mask_color;
  m_pClipMask = nullptr;
  if (pClipRgn && pClipRgn->GetType() != CFX_ClipRgn::RectI) {
    m_pClipMask = pClipRgn->GetMask().GetObject();
  }
  m_bVertical = bVertical;
  m_bFlipX = bFlipX;
  m_bFlipY = bFlipY;
  m_AlphaFlag = alpha_flag;
  m_pIccTransform = pIccTransform;
  m_bRgbByteOrder = bRgbByteOrder;
  m_BlendType = blend_type;
}
bool CFX_BitmapComposer::SetInfo(int width,
                                 int height,
                                 FXDIB_Format src_format,
                                 uint32_t* pSrcPalette) {
  m_SrcFormat = src_format;
  if (!m_Compositor.Init(m_pBitmap->GetFormat(), src_format, width, pSrcPalette,
                         m_MaskColor, FXDIB_BLEND_NORMAL,
                         m_pClipMask != nullptr || (m_BitmapAlpha < 255),
                         m_bRgbByteOrder, m_AlphaFlag, m_pIccTransform)) {
    return false;
  }
  if (m_bVertical) {
    m_pScanlineV = FX_Alloc(uint8_t, m_pBitmap->GetBPP() / 8 * width + 4);
    m_pClipScanV = FX_Alloc(uint8_t, m_pBitmap->GetHeight());
    if (m_pBitmap->m_pAlphaMask) {
      m_pScanlineAlphaV = FX_Alloc(uint8_t, width + 4);
    }
  }
  if (m_BitmapAlpha < 255) {
    m_pAddClipScan = FX_Alloc(
        uint8_t, m_bVertical ? m_pBitmap->GetHeight() : m_pBitmap->GetWidth());
  }
  return true;
}

void CFX_BitmapComposer::DoCompose(uint8_t* dest_scan,
                                   const uint8_t* src_scan,
                                   int dest_width,
                                   const uint8_t* clip_scan,
                                   const uint8_t* src_extra_alpha,
                                   uint8_t* dst_extra_alpha) {
  if (m_BitmapAlpha < 255) {
    if (clip_scan) {
      for (int i = 0; i < dest_width; i++) {
        m_pAddClipScan[i] = clip_scan[i] * m_BitmapAlpha / 255;
      }
    } else {
      FXSYS_memset(m_pAddClipScan, m_BitmapAlpha, dest_width);
    }
    clip_scan = m_pAddClipScan;
  }
  if (m_SrcFormat == FXDIB_8bppMask) {
    m_Compositor.CompositeByteMaskLine(dest_scan, src_scan, dest_width,
                                       clip_scan, dst_extra_alpha);
  } else if ((m_SrcFormat & 0xff) == 8) {
    m_Compositor.CompositePalBitmapLine(dest_scan, src_scan, 0, dest_width,
                                        clip_scan, src_extra_alpha,
                                        dst_extra_alpha);
  } else {
    m_Compositor.CompositeRgbBitmapLine(dest_scan, src_scan, dest_width,
                                        clip_scan, src_extra_alpha,
                                        dst_extra_alpha);
  }
}

void CFX_BitmapComposer::ComposeScanline(int line,
                                         const uint8_t* scanline,
                                         const uint8_t* scan_extra_alpha) {
  if (m_bVertical) {
    ComposeScanlineV(line, scanline, scan_extra_alpha);
    return;
  }
  const uint8_t* clip_scan = nullptr;
  if (m_pClipMask)
    clip_scan = m_pClipMask->GetBuffer() +
                (m_DestTop + line - m_pClipRgn->GetBox().top) *
                    m_pClipMask->GetPitch() +
                (m_DestLeft - m_pClipRgn->GetBox().left);
  uint8_t* dest_scan = (uint8_t*)m_pBitmap->GetScanline(line + m_DestTop) +
                       m_DestLeft * m_pBitmap->GetBPP() / 8;
  uint8_t* dest_alpha_scan =
      m_pBitmap->m_pAlphaMask
          ? (uint8_t*)m_pBitmap->m_pAlphaMask->GetScanline(line + m_DestTop) +
                m_DestLeft
          : nullptr;
  DoCompose(dest_scan, scanline, m_DestWidth, clip_scan, scan_extra_alpha,
            dest_alpha_scan);
}

void CFX_BitmapComposer::ComposeScanlineV(int line,
                                          const uint8_t* scanline,
                                          const uint8_t* scan_extra_alpha) {
  int i;
  int Bpp = m_pBitmap->GetBPP() / 8;
  int dest_pitch = m_pBitmap->GetPitch();
  int dest_alpha_pitch =
      m_pBitmap->m_pAlphaMask ? m_pBitmap->m_pAlphaMask->GetPitch() : 0;
  int dest_x = m_DestLeft + (m_bFlipX ? (m_DestWidth - line - 1) : line);
  uint8_t* dest_buf =
      m_pBitmap->GetBuffer() + dest_x * Bpp + m_DestTop * dest_pitch;
  uint8_t* dest_alpha_buf = m_pBitmap->m_pAlphaMask
                                ? m_pBitmap->m_pAlphaMask->GetBuffer() +
                                      dest_x + m_DestTop * dest_alpha_pitch
                                : nullptr;
  if (m_bFlipY) {
    dest_buf += dest_pitch * (m_DestHeight - 1);
    dest_alpha_buf += dest_alpha_pitch * (m_DestHeight - 1);
  }
  int y_step = dest_pitch;
  int y_alpha_step = dest_alpha_pitch;
  if (m_bFlipY) {
    y_step = -y_step;
    y_alpha_step = -y_alpha_step;
  }
  uint8_t* src_scan = m_pScanlineV;
  uint8_t* dest_scan = dest_buf;
  for (i = 0; i < m_DestHeight; i++) {
    for (int j = 0; j < Bpp; j++) {
      *src_scan++ = dest_scan[j];
    }
    dest_scan += y_step;
  }
  uint8_t* src_alpha_scan = m_pScanlineAlphaV;
  uint8_t* dest_alpha_scan = dest_alpha_buf;
  if (dest_alpha_scan) {
    for (i = 0; i < m_DestHeight; i++) {
      *src_alpha_scan++ = *dest_alpha_scan;
      dest_alpha_scan += y_alpha_step;
    }
  }
  uint8_t* clip_scan = nullptr;
  if (m_pClipMask) {
    clip_scan = m_pClipScanV;
    int clip_pitch = m_pClipMask->GetPitch();
    const uint8_t* src_clip =
        m_pClipMask->GetBuffer() +
        (m_DestTop - m_pClipRgn->GetBox().top) * clip_pitch +
        (dest_x - m_pClipRgn->GetBox().left);
    if (m_bFlipY) {
      src_clip += clip_pitch * (m_DestHeight - 1);
      clip_pitch = -clip_pitch;
    }
    for (i = 0; i < m_DestHeight; i++) {
      clip_scan[i] = *src_clip;
      src_clip += clip_pitch;
    }
  }
  DoCompose(m_pScanlineV, scanline, m_DestHeight, clip_scan, scan_extra_alpha,
            m_pScanlineAlphaV);
  src_scan = m_pScanlineV;
  dest_scan = dest_buf;
  for (i = 0; i < m_DestHeight; i++) {
    for (int j = 0; j < Bpp; j++) {
      dest_scan[j] = *src_scan++;
    }
    dest_scan += y_step;
  }
  src_alpha_scan = m_pScanlineAlphaV;
  dest_alpha_scan = dest_alpha_buf;
  if (dest_alpha_scan) {
    for (i = 0; i < m_DestHeight; i++) {
      *dest_alpha_scan = *src_alpha_scan++;
      dest_alpha_scan += y_alpha_step;
    }
  }
}
