/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/cursor.h"

#include <algorithm>

#include "webrtc/modules/desktop_capture/win/scoped_gdi_object.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/system_wrappers/interface/compile_assert.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

namespace {

#if defined(WEBRTC_ARCH_LITTLE_ENDIAN)

#define RGBA(r, g, b, a) \
    ((((a) << 24) & 0xff000000) | \
    (((b) << 16) & 0xff0000) | \
    (((g) << 8) & 0xff00) | \
    ((r) & 0xff))

#else  // !defined(WEBRTC_ARCH_LITTLE_ENDIAN)

#define RGBA(r, g, b, a) \
    ((((r) << 24) & 0xff000000) | \
    (((g) << 16) & 0xff0000) | \
    (((b) << 8) & 0xff00) | \
    ((a) & 0xff))

#endif  // !defined(WEBRTC_ARCH_LITTLE_ENDIAN)

const int kBytesPerPixel = DesktopFrame::kBytesPerPixel;

// Pixel colors used when generating cursor outlines.
const uint32_t kPixelRgbaBlack = RGBA(0, 0, 0, 0xff);
const uint32_t kPixelRgbaWhite = RGBA(0xff, 0xff, 0xff, 0xff);
const uint32_t kPixelRgbaTransparent = RGBA(0, 0, 0, 0);

const uint32_t kPixelRgbWhite = RGB(0xff, 0xff, 0xff);
const uint32_t kPixelRgbBlack = RGB(0, 0, 0);

// Expands the cursor shape to add a white outline for visibility against
// dark backgrounds.
void AddCursorOutline(int width, int height, uint32_t* data) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      // If this is a transparent pixel (bgr == 0 and alpha = 0), check the
      // neighbor pixels to see if this should be changed to an outline pixel.
      if (*data == kPixelRgbaTransparent) {
        // Change to white pixel if any neighbors (top, bottom, left, right)
        // are black.
        if ((y > 0 && data[-width] == kPixelRgbaBlack) ||
            (y < height - 1 && data[width] == kPixelRgbaBlack) ||
            (x > 0 && data[-1] == kPixelRgbaBlack) ||
            (x < width - 1 && data[1] == kPixelRgbaBlack)) {
          *data = kPixelRgbaWhite;
        }
      }
      data++;
    }
  }
}

// Premultiplies RGB components of a pixel by its alpha component.
uint32_t AlphaMul(uint32_t pixel) {
  COMPILE_ASSERT(sizeof(uint32_t) == kBytesPerPixel);

  RGBQUAD from = *reinterpret_cast<RGBQUAD*>(&pixel);
  RGBQUAD to = {
    (static_cast<uint16_t>(from.rgbBlue) * from.rgbReserved) / 0xff,
    (static_cast<uint16_t>(from.rgbGreen) * from.rgbReserved) / 0xff,
    (static_cast<uint16_t>(from.rgbRed) * from.rgbReserved) / 0xff,
    from.rgbReserved
  };

  return *reinterpret_cast<uint32_t*>(&to);
}

// Scans a 32bpp bitmap looking for any pixels with non-zero alpha component.
// |*has_alpha| is set to true if non-zero alpha is found. |stride| is expressed
// in pixels.
bool HasAlphaChannel(const uint32_t* data, int stride, int width, int height,
                     bool* has_alpha) {
  const RGBQUAD* plane = reinterpret_cast<const RGBQUAD*>(data);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (plane->rgbReserved != 0) {
        *has_alpha = true;
        return true;
      }
      plane += 1;
    }
    plane += stride - width;
  }

  *has_alpha = false;
  return true;
}

}  // namespace

MouseCursorShape* CreateMouseCursorShapeFromCursor(HDC dc, HCURSOR cursor) {
  ICONINFO iinfo;
  if (!GetIconInfo(cursor, &iinfo)) {
    LOG_F(LS_ERROR) << "Unable to get cursor icon info. Error = "
                    << GetLastError();
    return NULL;
  }

  int hotspot_x = iinfo.xHotspot;
  int hotspot_y = iinfo.yHotspot;

  // Make sure the bitmaps will be freed.
  win::ScopedBitmap scoped_mask(iinfo.hbmMask);
  win::ScopedBitmap scoped_color(iinfo.hbmColor);
  bool is_color = iinfo.hbmColor != NULL;

  // Get |scoped_mask| dimensions.
  BITMAP bitmap_info;
  if (!GetObject(scoped_mask, sizeof(bitmap_info), &bitmap_info)) {
    LOG_F(LS_ERROR) << "Unable to get bitmap info. Error = "
                    << GetLastError();
    return NULL;
  }

  int width = bitmap_info.bmWidth;
  int height = bitmap_info.bmHeight;
  scoped_array<uint32_t> mask_data(new uint32_t[width * height]);

  // Get pixel data from |scoped_mask| converting it to 32bpp along the way.
  // GetDIBits() sets the alpha component of every pixel to 0.
  BITMAPV5HEADER bmi = {0};
  bmi.bV5Size = sizeof(bmi);
  bmi.bV5Width = width;
  bmi.bV5Height = -height;  // request a top-down bitmap.
  bmi.bV5Planes = 1;
  bmi.bV5BitCount = kBytesPerPixel * 8;
  bmi.bV5Compression = BI_RGB;
  bmi.bV5AlphaMask = 0xff000000;
  bmi.bV5CSType = LCS_WINDOWS_COLOR_SPACE;
  bmi.bV5Intent = LCS_GM_BUSINESS;
  if (!GetDIBits(dc,
                 scoped_mask,
                 0,
                 height,
                 mask_data.get(),
                 reinterpret_cast<BITMAPINFO*>(&bmi),
                 DIB_RGB_COLORS)) {
    LOG_F(LS_ERROR) << "Unable to get bitmap bits. Error = "
                    << GetLastError();
    return NULL;
  }

  uint32_t* mask_plane = mask_data.get();

  scoped_array<uint32_t> color_data;
  uint32_t* color_plane = NULL;
  int color_stride = 0;
  bool has_alpha = false;

  if (is_color) {
    // Get the pixels from the color bitmap.
    color_data.reset(new uint32_t[width * height]);
    if (!GetDIBits(dc,
                   scoped_color,
                   0,
                   height,
                   color_data.get(),
                   reinterpret_cast<BITMAPINFO*>(&bmi),
                   DIB_RGB_COLORS)) {
      LOG_F(LS_ERROR) << "Unable to get bitmap bits. Error = "
                      << GetLastError();
      return NULL;
    }

    color_plane = color_data.get();
    color_stride = width;

    // GetDIBits() does not provide any indication whether the bitmap has alpha
    // channel, so we use HasAlphaChannel() below to find it out.
    if (!HasAlphaChannel(color_plane, color_stride, width, height, &has_alpha))
      return NULL;
  } else {
    // For non-color cursors, the mask contains both an AND and an XOR mask and
    // the height includes both. Thus, the width is correct, but we need to
    // divide by 2 to get the correct mask height.
    height /= 2;

    // The XOR mask becomes the color bitmap.
    color_plane = mask_plane + (width * height);
    color_stride = width;
  }

  // Reconstruct transparency from the mask if the color image does not has
  // alpha channel.
  if (!has_alpha) {
    bool add_outline = false;
    uint32_t* color = color_plane;
    uint32_t* dst = color_plane;
    uint32_t* mask = mask_plane;
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        // The two bitmaps combine as follows:
        //  mask  color   Windows Result   Our result    RGB   Alpha
        //   0     00      Black            Black         00    ff
        //   0     ff      White            White         ff    ff
        //   1     00      Screen           Transparent   00    00
        //   1     ff      Reverse-screen   Black         00    ff
        //
        // Since we don't support XOR cursors, we replace the "Reverse Screen"
        // with black. In this case, we also add an outline around the cursor
        // so that it is visible against a dark background.
        if (*mask == kPixelRgbWhite) {
          if (*color != 0) {
            add_outline = true;
            *dst = kPixelRgbaBlack;
          } else {
            *dst = kPixelRgbaTransparent;
          }
        } else {
          *dst = kPixelRgbaBlack ^ *color;
        }

        ++color;
        ++dst;
        ++mask;
      }
    }
    if (add_outline) {
      AddCursorOutline(width, height, color_plane);
    }
  }

  scoped_ptr<MouseCursorShape> result(new MouseCursorShape());
  result->data.assign(reinterpret_cast<char*>(color_plane),
                      height * width * kBytesPerPixel);
  result->size.set(width, height);
  result->hotspot.set(hotspot_x, hotspot_y);
  return result.release();
}

}  // namespace webrtc
