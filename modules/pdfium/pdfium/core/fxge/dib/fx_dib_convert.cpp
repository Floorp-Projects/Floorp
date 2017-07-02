// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <memory>
#include <utility>

#include "core/fxcodec/fx_codec.h"
#include "core/fxcrt/cfx_maybe_owned.h"
#include "core/fxge/fx_dib.h"
#include "third_party/base/ptr_util.h"

class CFX_Palette {
 public:
  CFX_Palette();
  ~CFX_Palette();

  bool BuildPalette(const CFX_DIBSource* pBitmap);
  uint32_t* GetPalette() const { return m_pPalette; }
  uint32_t* GetColorLut() const { return m_cLut; }
  uint32_t* GetAmountLut() const { return m_aLut; }
  int32_t Getlut() const { return m_lut; }

 protected:
  uint32_t* m_pPalette;
  uint32_t* m_cLut;
  uint32_t* m_aLut;
  int m_lut;
};
int _Partition(uint32_t* alut, uint32_t* clut, int l, int r) {
  uint32_t p_a = alut[l];
  uint32_t p_c = clut[l];
  while (l < r) {
    while (l < r && alut[r] >= p_a) {
      r--;
    }
    if (l < r) {
      alut[l] = alut[r];
      clut[l++] = clut[r];
    }
    while (l < r && alut[l] <= p_a) {
      l++;
    }
    if (l < r) {
      alut[r] = alut[l];
      clut[r--] = clut[l];
    }
  }
  alut[l] = p_a;
  clut[l] = p_c;
  return l;
}

void _Qsort(uint32_t* alut, uint32_t* clut, int l, int r) {
  if (l < r) {
    int pI = _Partition(alut, clut, l, r);
    _Qsort(alut, clut, l, pI - 1);
    _Qsort(alut, clut, pI + 1, r);
  }
}

void _ColorDecode(uint32_t pal_v, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = (uint8_t)((pal_v & 0xf00) >> 4);
  g = (uint8_t)(pal_v & 0x0f0);
  b = (uint8_t)((pal_v & 0x00f) << 4);
}

void _Obtain_Pal(uint32_t* aLut,
                 uint32_t* cLut,
                 uint32_t* dest_pal,
                 uint32_t lut) {
  uint32_t lut_1 = lut - 1;
  for (int row = 0; row < 256; row++) {
    int lut_offset = lut_1 - row;
    if (lut_offset < 0) {
      lut_offset += 256;
    }
    uint32_t color = cLut[lut_offset];
    uint8_t r;
    uint8_t g;
    uint8_t b;
    _ColorDecode(color, r, g, b);
    dest_pal[row] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b | 0xff000000;
    aLut[lut_offset] = row;
  }
}

CFX_Palette::CFX_Palette() {
  m_pPalette = nullptr;
  m_cLut = nullptr;
  m_aLut = nullptr;
  m_lut = 0;
}

CFX_Palette::~CFX_Palette() {
  FX_Free(m_pPalette);
  FX_Free(m_cLut);
  FX_Free(m_aLut);
  m_lut = 0;
}

bool CFX_Palette::BuildPalette(const CFX_DIBSource* pBitmap) {
  if (!pBitmap) {
    return false;
  }
  FX_Free(m_pPalette);
  m_pPalette = FX_Alloc(uint32_t, 256);
  int bpp = pBitmap->GetBPP() / 8;
  int width = pBitmap->GetWidth();
  int height = pBitmap->GetHeight();
  FX_Free(m_cLut);
  m_cLut = nullptr;
  FX_Free(m_aLut);
  m_aLut = nullptr;
  m_cLut = FX_Alloc(uint32_t, 4096);
  m_aLut = FX_Alloc(uint32_t, 4096);
  int row, col;
  m_lut = 0;
  for (row = 0; row < height; row++) {
    uint8_t* scan_line = (uint8_t*)pBitmap->GetScanline(row);
    for (col = 0; col < width; col++) {
      uint8_t* src_port = scan_line + col * bpp;
      uint32_t b = src_port[0] & 0xf0;
      uint32_t g = src_port[1] & 0xf0;
      uint32_t r = src_port[2] & 0xf0;
      uint32_t index = (r << 4) + g + (b >> 4);
      m_aLut[index]++;
    }
  }
  for (row = 0; row < 4096; row++) {
    if (m_aLut[row] != 0) {
      m_aLut[m_lut] = m_aLut[row];
      m_cLut[m_lut] = row;
      m_lut++;
    }
  }
  _Qsort(m_aLut, m_cLut, 0, m_lut - 1);
  _Obtain_Pal(m_aLut, m_cLut, m_pPalette, m_lut);
  return true;
}

bool ConvertBuffer_1bppMask2Gray(uint8_t* dest_buf,
                                 int dest_pitch,
                                 int width,
                                 int height,
                                 const CFX_DIBSource* pSrcBitmap,
                                 int src_left,
                                 int src_top) {
  uint8_t set_gray, reset_gray;
  set_gray = 0xff;
  reset_gray = 0x00;
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    FXSYS_memset(dest_scan, reset_gray, width);
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row);
    for (int col = src_left; col < src_left + width; col++) {
      if (src_scan[col / 8] & (1 << (7 - col % 8))) {
        *dest_scan = set_gray;
      }
      dest_scan++;
    }
  }
  return true;
}

bool ConvertBuffer_8bppMask2Gray(uint8_t* dest_buf,
                                 int dest_pitch,
                                 int width,
                                 int height,
                                 const CFX_DIBSource* pSrcBitmap,
                                 int src_left,
                                 int src_top) {
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row) + src_left;
    FXSYS_memcpy(dest_scan, src_scan, width);
  }
  return true;
}

bool ConvertBuffer_1bppPlt2Gray(uint8_t* dest_buf,
                                int dest_pitch,
                                int width,
                                int height,
                                const CFX_DIBSource* pSrcBitmap,
                                int src_left,
                                int src_top) {
  uint32_t* src_plt = pSrcBitmap->GetPalette();
  uint8_t gray[2];
  uint8_t reset_r;
  uint8_t reset_g;
  uint8_t reset_b;
  uint8_t set_r;
  uint8_t set_g;
  uint8_t set_b;
  if (pSrcBitmap->IsCmykImage()) {
    AdobeCMYK_to_sRGB1(FXSYS_GetCValue(src_plt[0]), FXSYS_GetMValue(src_plt[0]),
                       FXSYS_GetYValue(src_plt[0]), FXSYS_GetKValue(src_plt[0]),
                       reset_r, reset_g, reset_b);
    AdobeCMYK_to_sRGB1(FXSYS_GetCValue(src_plt[1]), FXSYS_GetMValue(src_plt[1]),
                       FXSYS_GetYValue(src_plt[1]), FXSYS_GetKValue(src_plt[1]),
                       set_r, set_g, set_b);
  } else {
    reset_r = FXARGB_R(src_plt[0]);
    reset_g = FXARGB_G(src_plt[0]);
    reset_b = FXARGB_B(src_plt[0]);
    set_r = FXARGB_R(src_plt[1]);
    set_g = FXARGB_G(src_plt[1]);
    set_b = FXARGB_B(src_plt[1]);
  }
  gray[0] = FXRGB2GRAY(reset_r, reset_g, reset_b);
  gray[1] = FXRGB2GRAY(set_r, set_g, set_b);

  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    FXSYS_memset(dest_scan, gray[0], width);
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row);
    for (int col = src_left; col < src_left + width; col++) {
      if (src_scan[col / 8] & (1 << (7 - col % 8))) {
        *dest_scan = gray[1];
      }
      dest_scan++;
    }
  }
  return true;
}

bool ConvertBuffer_8bppPlt2Gray(uint8_t* dest_buf,
                                int dest_pitch,
                                int width,
                                int height,
                                const CFX_DIBSource* pSrcBitmap,
                                int src_left,
                                int src_top) {
  uint32_t* src_plt = pSrcBitmap->GetPalette();
  uint8_t gray[256];
  if (pSrcBitmap->IsCmykImage()) {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    for (size_t i = 0; i < FX_ArraySize(gray); i++) {
      AdobeCMYK_to_sRGB1(
          FXSYS_GetCValue(src_plt[i]), FXSYS_GetMValue(src_plt[i]),
          FXSYS_GetYValue(src_plt[i]), FXSYS_GetKValue(src_plt[i]), r, g, b);
      gray[i] = FXRGB2GRAY(r, g, b);
    }
  } else {
    for (size_t i = 0; i < FX_ArraySize(gray); i++) {
      gray[i] = FXRGB2GRAY(FXARGB_R(src_plt[i]), FXARGB_G(src_plt[i]),
                           FXARGB_B(src_plt[i]));
    }
  }

  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row) + src_left;
    for (int col = 0; col < width; col++) {
      *dest_scan++ = gray[*src_scan++];
    }
  }
  return true;
}

bool ConvertBuffer_RgbOrCmyk2Gray(uint8_t* dest_buf,
                                  int dest_pitch,
                                  int width,
                                  int height,
                                  const CFX_DIBSource* pSrcBitmap,
                                  int src_left,
                                  int src_top) {
  int Bpp = pSrcBitmap->GetBPP() / 8;
  if (pSrcBitmap->IsCmykImage()) {
    for (int row = 0; row < height; row++) {
      uint8_t* dest_scan = dest_buf + row * dest_pitch;
      const uint8_t* src_scan =
          pSrcBitmap->GetScanline(src_top + row) + src_left * 4;
      for (int col = 0; col < width; col++) {
        uint8_t r, g, b;
        AdobeCMYK_to_sRGB1(FXSYS_GetCValue((uint32_t)src_scan[0]),
                           FXSYS_GetMValue((uint32_t)src_scan[1]),
                           FXSYS_GetYValue((uint32_t)src_scan[2]),
                           FXSYS_GetKValue((uint32_t)src_scan[3]), r, g, b);
        *dest_scan++ = FXRGB2GRAY(r, g, b);
        src_scan += 4;
      }
    }
  } else {
    for (int row = 0; row < height; row++) {
      uint8_t* dest_scan = dest_buf + row * dest_pitch;
      const uint8_t* src_scan =
          pSrcBitmap->GetScanline(src_top + row) + src_left * Bpp;
      for (int col = 0; col < width; col++) {
        *dest_scan++ = FXRGB2GRAY(src_scan[2], src_scan[1], src_scan[0]);
        src_scan += Bpp;
      }
    }
  }
  return true;
}

void ConvertBuffer_IndexCopy(uint8_t* dest_buf,
                             int dest_pitch,
                             int width,
                             int height,
                             const CFX_DIBSource* pSrcBitmap,
                             int src_left,
                             int src_top) {
  if (pSrcBitmap->GetBPP() == 1) {
    for (int row = 0; row < height; row++) {
      uint8_t* dest_scan = dest_buf + row * dest_pitch;
      FXSYS_memset(dest_scan, 0, width);
      const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row);
      for (int col = src_left; col < src_left + width; col++) {
        if (src_scan[col / 8] & (1 << (7 - col % 8))) {
          *dest_scan = 1;
        }
        dest_scan++;
      }
    }
  } else {
    for (int row = 0; row < height; row++) {
      uint8_t* dest_scan = dest_buf + row * dest_pitch;
      const uint8_t* src_scan =
          pSrcBitmap->GetScanline(src_top + row) + src_left;
      FXSYS_memcpy(dest_scan, src_scan, width);
    }
  }
}

bool ConvertBuffer_Plt2PltRgb8(uint8_t* dest_buf,
                               int dest_pitch,
                               int width,
                               int height,
                               const CFX_DIBSource* pSrcBitmap,
                               int src_left,
                               int src_top,
                               uint32_t* dst_plt) {
  ConvertBuffer_IndexCopy(dest_buf, dest_pitch, width, height, pSrcBitmap,
                          src_left, src_top);
  uint32_t* src_plt = pSrcBitmap->GetPalette();
  int plt_size = pSrcBitmap->GetPaletteSize();
  if (pSrcBitmap->IsCmykImage()) {
    for (int i = 0; i < plt_size; i++) {
      uint8_t r;
      uint8_t g;
      uint8_t b;
      AdobeCMYK_to_sRGB1(
          FXSYS_GetCValue(src_plt[i]), FXSYS_GetMValue(src_plt[i]),
          FXSYS_GetYValue(src_plt[i]), FXSYS_GetKValue(src_plt[i]), r, g, b);
      dst_plt[i] = FXARGB_MAKE(0xff, r, g, b);
    }
  } else {
    FXSYS_memcpy(dst_plt, src_plt, plt_size * 4);
  }
  return true;
}

bool ConvertBuffer_Rgb2PltRgb8(uint8_t* dest_buf,
                               int dest_pitch,
                               int width,
                               int height,
                               const CFX_DIBSource* pSrcBitmap,
                               int src_left,
                               int src_top,
                               uint32_t* dst_plt) {
  int bpp = pSrcBitmap->GetBPP() / 8;
  CFX_Palette palette;
  palette.BuildPalette(pSrcBitmap);
  uint32_t* cLut = palette.GetColorLut();
  uint32_t* aLut = palette.GetAmountLut();
  if (!cLut || !aLut) {
    return false;
  }
  int lut = palette.Getlut();
  uint32_t* pPalette = palette.GetPalette();
  if (lut > 256) {
    int err, min_err;
    int lut_256 = lut - 256;
    for (int row = 0; row < lut_256; row++) {
      min_err = 1000000;
      uint8_t r, g, b;
      _ColorDecode(cLut[row], r, g, b);
      int clrindex = 0;
      for (int col = 0; col < 256; col++) {
        uint32_t p_color = *(pPalette + col);
        int d_r = r - (uint8_t)(p_color >> 16);
        int d_g = g - (uint8_t)(p_color >> 8);
        int d_b = b - (uint8_t)(p_color);
        err = d_r * d_r + d_g * d_g + d_b * d_b;
        if (err < min_err) {
          min_err = err;
          clrindex = col;
        }
      }
      aLut[row] = clrindex;
    }
  }
  int32_t lut_1 = lut - 1;
  for (int row = 0; row < height; row++) {
    uint8_t* src_scan =
        (uint8_t*)pSrcBitmap->GetScanline(src_top + row) + src_left;
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    for (int col = 0; col < width; col++) {
      uint8_t* src_port = src_scan + col * bpp;
      int r = src_port[2] & 0xf0;
      int g = src_port[1] & 0xf0;
      int b = src_port[0] & 0xf0;
      uint32_t clrindex = (r << 4) + g + (b >> 4);
      for (int i = lut_1; i >= 0; i--)
        if (clrindex == cLut[i]) {
          *(dest_scan + col) = (uint8_t)(aLut[i]);
          break;
        }
    }
  }
  FXSYS_memcpy(dst_plt, pPalette, sizeof(uint32_t) * 256);
  return true;
}

bool ConvertBuffer_1bppMask2Rgb(FXDIB_Format dst_format,
                                uint8_t* dest_buf,
                                int dest_pitch,
                                int width,
                                int height,
                                const CFX_DIBSource* pSrcBitmap,
                                int src_left,
                                int src_top) {
  int comps = (dst_format & 0xff) / 8;
  uint8_t set_gray, reset_gray;
  set_gray = 0xff;
  reset_gray = 0x00;
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row);
    for (int col = src_left; col < src_left + width; col++) {
      if (src_scan[col / 8] & (1 << (7 - col % 8))) {
        dest_scan[0] = set_gray;
        dest_scan[1] = set_gray;
        dest_scan[2] = set_gray;
      } else {
        dest_scan[0] = reset_gray;
        dest_scan[1] = reset_gray;
        dest_scan[2] = reset_gray;
      }
      dest_scan += comps;
    }
  }
  return true;
}

bool ConvertBuffer_8bppMask2Rgb(FXDIB_Format dst_format,
                                uint8_t* dest_buf,
                                int dest_pitch,
                                int width,
                                int height,
                                const CFX_DIBSource* pSrcBitmap,
                                int src_left,
                                int src_top) {
  int comps = (dst_format & 0xff) / 8;
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row) + src_left;
    uint8_t src_pixel;
    for (int col = 0; col < width; col++) {
      src_pixel = *src_scan++;
      *dest_scan++ = src_pixel;
      *dest_scan++ = src_pixel;
      *dest_scan = src_pixel;
      dest_scan += comps - 2;
    }
  }
  return true;
}

bool ConvertBuffer_1bppPlt2Rgb(FXDIB_Format dst_format,
                               uint8_t* dest_buf,
                               int dest_pitch,
                               int width,
                               int height,
                               const CFX_DIBSource* pSrcBitmap,
                               int src_left,
                               int src_top) {
  int comps = (dst_format & 0xff) / 8;
  uint32_t* src_plt = pSrcBitmap->GetPalette();
  uint32_t plt[2];
  uint8_t* bgr_ptr = (uint8_t*)plt;
  if (pSrcBitmap->IsCmykImage()) {
    plt[0] = FXCMYK_TODIB(src_plt[0]);
    plt[1] = FXCMYK_TODIB(src_plt[1]);
  } else {
    bgr_ptr[0] = FXARGB_B(src_plt[0]);
    bgr_ptr[1] = FXARGB_G(src_plt[0]);
    bgr_ptr[2] = FXARGB_R(src_plt[0]);
    bgr_ptr[3] = FXARGB_B(src_plt[1]);
    bgr_ptr[4] = FXARGB_G(src_plt[1]);
    bgr_ptr[5] = FXARGB_R(src_plt[1]);
  }

  if (pSrcBitmap->IsCmykImage()) {
    AdobeCMYK_to_sRGB1(FXSYS_GetCValue(src_plt[0]), FXSYS_GetMValue(src_plt[0]),
                       FXSYS_GetYValue(src_plt[0]), FXSYS_GetKValue(src_plt[0]),
                       bgr_ptr[2], bgr_ptr[1], bgr_ptr[0]);
    AdobeCMYK_to_sRGB1(FXSYS_GetCValue(src_plt[1]), FXSYS_GetMValue(src_plt[1]),
                       FXSYS_GetYValue(src_plt[1]), FXSYS_GetKValue(src_plt[1]),
                       bgr_ptr[5], bgr_ptr[4], bgr_ptr[3]);
  }

  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row);
    for (int col = src_left; col < src_left + width; col++) {
      if (src_scan[col / 8] & (1 << (7 - col % 8))) {
        *dest_scan++ = bgr_ptr[3];
        *dest_scan++ = bgr_ptr[4];
        *dest_scan = bgr_ptr[5];
      } else {
        *dest_scan++ = bgr_ptr[0];
        *dest_scan++ = bgr_ptr[1];
        *dest_scan = bgr_ptr[2];
      }
      dest_scan += comps - 2;
    }
  }
  return true;
}

bool ConvertBuffer_8bppPlt2Rgb(FXDIB_Format dst_format,
                               uint8_t* dest_buf,
                               int dest_pitch,
                               int width,
                               int height,
                               const CFX_DIBSource* pSrcBitmap,
                               int src_left,
                               int src_top) {
  int comps = (dst_format & 0xff) / 8;
  uint32_t* src_plt = pSrcBitmap->GetPalette();
  uint32_t plt[256];
  uint8_t* bgr_ptr = (uint8_t*)plt;
  if (!pSrcBitmap->IsCmykImage()) {
    for (int i = 0; i < 256; i++) {
      *bgr_ptr++ = FXARGB_B(src_plt[i]);
      *bgr_ptr++ = FXARGB_G(src_plt[i]);
      *bgr_ptr++ = FXARGB_R(src_plt[i]);
    }
    bgr_ptr = (uint8_t*)plt;
  }

  if (pSrcBitmap->IsCmykImage()) {
    for (int i = 0; i < 256; i++) {
      AdobeCMYK_to_sRGB1(
          FXSYS_GetCValue(src_plt[i]), FXSYS_GetMValue(src_plt[i]),
          FXSYS_GetYValue(src_plt[i]), FXSYS_GetKValue(src_plt[i]), bgr_ptr[2],
          bgr_ptr[1], bgr_ptr[0]);
      bgr_ptr += 3;
    }
    bgr_ptr = (uint8_t*)plt;
  }

  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row) + src_left;
    for (int col = 0; col < width; col++) {
      uint8_t* src_pixel = bgr_ptr + 3 * (*src_scan++);
      *dest_scan++ = *src_pixel++;
      *dest_scan++ = *src_pixel++;
      *dest_scan = *src_pixel++;
      dest_scan += comps - 2;
    }
  }
  return true;
}

bool ConvertBuffer_24bppRgb2Rgb24(uint8_t* dest_buf,
                                  int dest_pitch,
                                  int width,
                                  int height,
                                  const CFX_DIBSource* pSrcBitmap,
                                  int src_left,
                                  int src_top) {
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row) + src_left * 3;
    FXSYS_memcpy(dest_scan, src_scan, width * 3);
  }
  return true;
}

bool ConvertBuffer_32bppRgb2Rgb24(uint8_t* dest_buf,
                                  int dest_pitch,
                                  int width,
                                  int height,
                                  const CFX_DIBSource* pSrcBitmap,
                                  int src_left,
                                  int src_top) {
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row) + src_left * 4;
    for (int col = 0; col < width; col++) {
      *dest_scan++ = *src_scan++;
      *dest_scan++ = *src_scan++;
      *dest_scan++ = *src_scan++;
      src_scan++;
    }
  }
  return true;
}

bool ConvertBuffer_Rgb2Rgb32(uint8_t* dest_buf,
                             int dest_pitch,
                             int width,
                             int height,
                             const CFX_DIBSource* pSrcBitmap,
                             int src_left,
                             int src_top) {
  int comps = pSrcBitmap->GetBPP() / 8;
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row) + src_left * comps;
    for (int col = 0; col < width; col++) {
      *dest_scan++ = *src_scan++;
      *dest_scan++ = *src_scan++;
      *dest_scan++ = *src_scan++;
      dest_scan++;
      src_scan += comps - 3;
    }
  }
  return true;
}

bool ConvertBuffer_32bppCmyk2Rgb32(uint8_t* dest_buf,
                                   int dest_pitch,
                                   int width,
                                   int height,
                                   const CFX_DIBSource* pSrcBitmap,
                                   int src_left,
                                   int src_top) {
  for (int row = 0; row < height; row++) {
    uint8_t* dest_scan = dest_buf + row * dest_pitch;
    const uint8_t* src_scan =
        pSrcBitmap->GetScanline(src_top + row) + src_left * 4;
    for (int col = 0; col < width; col++) {
      AdobeCMYK_to_sRGB1(src_scan[0], src_scan[1], src_scan[2], src_scan[3],
                         dest_scan[2], dest_scan[1], dest_scan[0]);
      dest_scan += 4;
      src_scan += 4;
    }
  }
  return true;
}

bool ConvertBuffer(FXDIB_Format dest_format,
                   uint8_t* dest_buf,
                   int dest_pitch,
                   int width,
                   int height,
                   const CFX_DIBSource* pSrcBitmap,
                   int src_left,
                   int src_top,
                   std::unique_ptr<uint32_t, FxFreeDeleter>* p_pal) {
  FXDIB_Format src_format = pSrcBitmap->GetFormat();
  switch (dest_format) {
    case FXDIB_Invalid:
    case FXDIB_1bppCmyk:
    case FXDIB_1bppMask:
    case FXDIB_1bppRgb:
      ASSERT(false);
      return false;
    case FXDIB_8bppMask: {
      if ((src_format & 0xff) == 1) {
        if (pSrcBitmap->GetPalette()) {
          return ConvertBuffer_1bppPlt2Gray(dest_buf, dest_pitch, width, height,
                                            pSrcBitmap, src_left, src_top);
        }
        return ConvertBuffer_1bppMask2Gray(dest_buf, dest_pitch, width, height,
                                           pSrcBitmap, src_left, src_top);
      }
      if ((src_format & 0xff) == 8) {
        if (pSrcBitmap->GetPalette()) {
          return ConvertBuffer_8bppPlt2Gray(dest_buf, dest_pitch, width, height,
                                            pSrcBitmap, src_left, src_top);
        }
        return ConvertBuffer_8bppMask2Gray(dest_buf, dest_pitch, width, height,
                                           pSrcBitmap, src_left, src_top);
      }
      if ((src_format & 0xff) >= 24) {
        return ConvertBuffer_RgbOrCmyk2Gray(dest_buf, dest_pitch, width, height,
                                            pSrcBitmap, src_left, src_top);
      }
      return false;
    }
    case FXDIB_8bppRgb:
    case FXDIB_8bppRgba: {
      if ((src_format & 0xff) == 8 && !pSrcBitmap->GetPalette()) {
        return ConvertBuffer(FXDIB_8bppMask, dest_buf, dest_pitch, width,
                             height, pSrcBitmap, src_left, src_top, p_pal);
      }
      p_pal->reset(FX_Alloc(uint32_t, 256));
      if (((src_format & 0xff) == 1 || (src_format & 0xff) == 8) &&
          pSrcBitmap->GetPalette()) {
        return ConvertBuffer_Plt2PltRgb8(dest_buf, dest_pitch, width, height,
                                         pSrcBitmap, src_left, src_top,
                                         p_pal->get());
      }
      if ((src_format & 0xff) >= 24) {
        return ConvertBuffer_Rgb2PltRgb8(dest_buf, dest_pitch, width, height,
                                         pSrcBitmap, src_left, src_top,
                                         p_pal->get());
      }
      return false;
    }
    case FXDIB_Rgb:
    case FXDIB_Rgba: {
      if ((src_format & 0xff) == 1) {
        if (pSrcBitmap->GetPalette()) {
          return ConvertBuffer_1bppPlt2Rgb(dest_format, dest_buf, dest_pitch,
                                           width, height, pSrcBitmap, src_left,
                                           src_top);
        }
        return ConvertBuffer_1bppMask2Rgb(dest_format, dest_buf, dest_pitch,
                                          width, height, pSrcBitmap, src_left,
                                          src_top);
      }
      if ((src_format & 0xff) == 8) {
        if (pSrcBitmap->GetPalette()) {
          return ConvertBuffer_8bppPlt2Rgb(dest_format, dest_buf, dest_pitch,
                                           width, height, pSrcBitmap, src_left,
                                           src_top);
        }
        return ConvertBuffer_8bppMask2Rgb(dest_format, dest_buf, dest_pitch,
                                          width, height, pSrcBitmap, src_left,
                                          src_top);
      }
      if ((src_format & 0xff) == 24) {
        return ConvertBuffer_24bppRgb2Rgb24(dest_buf, dest_pitch, width, height,
                                            pSrcBitmap, src_left, src_top);
      }
      if ((src_format & 0xff) == 32) {
        return ConvertBuffer_32bppRgb2Rgb24(dest_buf, dest_pitch, width, height,
                                            pSrcBitmap, src_left, src_top);
      }
      return false;
    }
    case FXDIB_Argb:
    case FXDIB_Rgb32: {
      if ((src_format & 0xff) == 1) {
        if (pSrcBitmap->GetPalette()) {
          return ConvertBuffer_1bppPlt2Rgb(dest_format, dest_buf, dest_pitch,
                                           width, height, pSrcBitmap, src_left,
                                           src_top);
        }
        return ConvertBuffer_1bppMask2Rgb(dest_format, dest_buf, dest_pitch,
                                          width, height, pSrcBitmap, src_left,
                                          src_top);
      }
      if ((src_format & 0xff) == 8) {
        if (pSrcBitmap->GetPalette()) {
          return ConvertBuffer_8bppPlt2Rgb(dest_format, dest_buf, dest_pitch,
                                           width, height, pSrcBitmap, src_left,
                                           src_top);
        }
        return ConvertBuffer_8bppMask2Rgb(dest_format, dest_buf, dest_pitch,
                                          width, height, pSrcBitmap, src_left,
                                          src_top);
      }
      if ((src_format & 0xff) >= 24) {
        if (src_format & 0x0400) {
          return ConvertBuffer_32bppCmyk2Rgb32(dest_buf, dest_pitch, width,
                                               height, pSrcBitmap, src_left,
                                               src_top);
        }
        return ConvertBuffer_Rgb2Rgb32(dest_buf, dest_pitch, width, height,
                                       pSrcBitmap, src_left, src_top);
      }
      return false;
    }
    default:
      return false;
  }
}

std::unique_ptr<CFX_DIBitmap> CFX_DIBSource::CloneConvert(
    FXDIB_Format dest_format) const {
  if (dest_format == GetFormat())
    return Clone(nullptr);

  std::unique_ptr<CFX_DIBitmap> pClone = pdfium::MakeUnique<CFX_DIBitmap>();
  if (!pClone->Create(m_Width, m_Height, dest_format))
    return nullptr;

  CFX_MaybeOwned<CFX_DIBitmap> pSrcAlpha;
  if (HasAlpha()) {
    if (GetFormat() == FXDIB_Argb)
      pSrcAlpha = CloneAlphaMask();
    else
      pSrcAlpha = m_pAlphaMask;

    if (!pSrcAlpha)
      return nullptr;
  }
  bool ret = true;
  if (dest_format & 0x0200) {
    if (dest_format == FXDIB_Argb) {
      ret = pSrcAlpha
                ? pClone->LoadChannel(FXDIB_Alpha, pSrcAlpha.Get(), FXDIB_Alpha)
                : pClone->LoadChannel(FXDIB_Alpha, 0xff);
    } else {
      ret = pClone->SetAlphaMask(pSrcAlpha.Get());
    }
  }
  if (!ret)
    return nullptr;

  std::unique_ptr<uint32_t, FxFreeDeleter> pal_8bpp;
  if (!ConvertBuffer(dest_format, pClone->GetBuffer(), pClone->GetPitch(),
                     m_Width, m_Height, this, 0, 0, &pal_8bpp)) {
    return nullptr;
  }
  if (pal_8bpp)
    pClone->SetPalette(pal_8bpp.get());

  return pClone;
}

bool CFX_DIBitmap::ConvertFormat(FXDIB_Format dest_format) {
  FXDIB_Format src_format = GetFormat();
  if (dest_format == src_format)
    return true;

  if (dest_format == FXDIB_8bppMask && src_format == FXDIB_8bppRgb &&
      !m_pPalette) {
    m_AlphaFlag = 1;
    return true;
  }
  if (dest_format == FXDIB_Argb && src_format == FXDIB_Rgb32) {
    m_AlphaFlag = 2;
    for (int row = 0; row < m_Height; row++) {
      uint8_t* scanline = m_pBuffer + row * m_Pitch + 3;
      for (int col = 0; col < m_Width; col++) {
        *scanline = 0xff;
        scanline += 4;
      }
    }
    return true;
  }
  int dest_bpp = dest_format & 0xff;
  int dest_pitch = (dest_bpp * m_Width + 31) / 32 * 4;
  uint8_t* dest_buf = FX_TryAlloc(uint8_t, dest_pitch * m_Height + 4);
  if (!dest_buf) {
    return false;
  }
  CFX_DIBitmap* pAlphaMask = nullptr;
  if (dest_format == FXDIB_Argb) {
    FXSYS_memset(dest_buf, 0xff, dest_pitch * m_Height + 4);
    if (m_pAlphaMask) {
      for (int row = 0; row < m_Height; row++) {
        uint8_t* pDstScanline = dest_buf + row * dest_pitch + 3;
        const uint8_t* pSrcScanline = m_pAlphaMask->GetScanline(row);
        for (int col = 0; col < m_Width; col++) {
          *pDstScanline = *pSrcScanline++;
          pDstScanline += 4;
        }
      }
    }
  } else if (dest_format & 0x0200) {
    if (src_format == FXDIB_Argb) {
      pAlphaMask = CloneAlphaMask().release();
      if (!pAlphaMask) {
        FX_Free(dest_buf);
        return false;
      }
    } else {
      if (!m_pAlphaMask) {
        if (!BuildAlphaMask()) {
          FX_Free(dest_buf);
          return false;
        }
        pAlphaMask = m_pAlphaMask;
        m_pAlphaMask = nullptr;
      } else {
        pAlphaMask = m_pAlphaMask;
      }
    }
  }
  bool ret = false;
  std::unique_ptr<uint32_t, FxFreeDeleter> pal_8bpp;
  ret = ConvertBuffer(dest_format, dest_buf, dest_pitch, m_Width, m_Height,
                      this, 0, 0, &pal_8bpp);
  if (!ret) {
    if (pAlphaMask != m_pAlphaMask)
      delete pAlphaMask;
    FX_Free(dest_buf);
    return false;
  }
  if (m_pAlphaMask && pAlphaMask != m_pAlphaMask)
    delete m_pAlphaMask;
  m_pAlphaMask = pAlphaMask;
  m_pPalette = std::move(pal_8bpp);
  if (!m_bExtBuf)
    FX_Free(m_pBuffer);
  m_bExtBuf = false;
  m_pBuffer = dest_buf;
  m_bpp = (uint8_t)dest_format;
  m_AlphaFlag = (uint8_t)(dest_format >> 8);
  m_Pitch = dest_pitch;
  return true;
}
