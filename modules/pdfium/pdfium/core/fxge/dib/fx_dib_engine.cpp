// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <limits.h>

#include <algorithm>

#include "core/fxge/dib/dib_int.h"
#include "core/fxge/fx_dib.h"
#include "third_party/base/ptr_util.h"

namespace {

bool SourceSizeWithinLimit(int width, int height) {
  const int kMaxProgressiveStretchPixels = 1000000;
  return !height || width < kMaxProgressiveStretchPixels / height;
}

FXDIB_Format GetStretchedFormat(const CFX_DIBSource& src) {
  FXDIB_Format format = src.GetFormat();
  if (format == FXDIB_1bppMask)
    return FXDIB_8bppMask;
  if (format == FXDIB_1bppRgb)
    return FXDIB_8bppRgb;
  if (format == FXDIB_8bppRgb && src.GetPalette())
    return FXDIB_Rgb;
  return format;
}

}  // namespace

CWeightTable::CWeightTable()
    : m_DestMin(0),
      m_ItemSize(0),
      m_pWeightTables(nullptr),
      m_dwWeightTablesSize(0) {}

CWeightTable::~CWeightTable() {
  FX_Free(m_pWeightTables);
}

size_t CWeightTable::GetPixelWeightSize() const {
  return m_ItemSize / sizeof(int) - 2;
}

bool CWeightTable::Calc(int dest_len,
                        int dest_min,
                        int dest_max,
                        int src_len,
                        int src_min,
                        int src_max,
                        int flags) {
  FX_Free(m_pWeightTables);
  m_pWeightTables = nullptr;
  m_dwWeightTablesSize = 0;
  const double scale = (FX_FLOAT)src_len / (FX_FLOAT)dest_len;
  const double base = dest_len < 0 ? (FX_FLOAT)(src_len) : 0;
  const int ext_size = flags & FXDIB_BICUBIC_INTERPOL ? 3 : 1;
  m_ItemSize =
      sizeof(int) * 2 +
      (int)(sizeof(int) * (FXSYS_ceil(FXSYS_fabs((FX_FLOAT)scale)) + ext_size));
  m_DestMin = dest_min;
  if ((dest_max - dest_min) > (int)((1U << 30) - 4) / m_ItemSize)
    return false;

  m_dwWeightTablesSize = (dest_max - dest_min) * m_ItemSize + 4;
  m_pWeightTables = FX_TryAlloc(uint8_t, m_dwWeightTablesSize);
  if (!m_pWeightTables)
    return false;

  if ((flags & FXDIB_NOSMOOTH) != 0 || FXSYS_fabs((FX_FLOAT)scale) < 1.0f) {
    for (int dest_pixel = dest_min; dest_pixel < dest_max; dest_pixel++) {
      PixelWeight& pixel_weights = *GetPixelWeight(dest_pixel);
      double src_pos = dest_pixel * scale + scale / 2 + base;
      if (flags & FXDIB_INTERPOL) {
        pixel_weights.m_SrcStart =
            (int)FXSYS_floor((FX_FLOAT)src_pos - 1.0f / 2);
        pixel_weights.m_SrcEnd = (int)FXSYS_floor((FX_FLOAT)src_pos + 1.0f / 2);
        if (pixel_weights.m_SrcStart < src_min) {
          pixel_weights.m_SrcStart = src_min;
        }
        if (pixel_weights.m_SrcEnd >= src_max) {
          pixel_weights.m_SrcEnd = src_max - 1;
        }
        if (pixel_weights.m_SrcStart == pixel_weights.m_SrcEnd) {
          pixel_weights.m_Weights[0] = 65536;
        } else {
          pixel_weights.m_Weights[1] = FXSYS_round(
              (FX_FLOAT)(src_pos - pixel_weights.m_SrcStart - 1.0f / 2) *
              65536);
          pixel_weights.m_Weights[0] = 65536 - pixel_weights.m_Weights[1];
        }
      } else if (flags & FXDIB_BICUBIC_INTERPOL) {
        pixel_weights.m_SrcStart =
            (int)FXSYS_floor((FX_FLOAT)src_pos - 1.0f / 2);
        pixel_weights.m_SrcEnd = (int)FXSYS_floor((FX_FLOAT)src_pos + 1.0f / 2);
        int start = pixel_weights.m_SrcStart - 1;
        int end = pixel_weights.m_SrcEnd + 1;
        if (start < src_min) {
          start = src_min;
        }
        if (end >= src_max) {
          end = src_max - 1;
        }
        if (pixel_weights.m_SrcStart < src_min) {
          src_pos += src_min - pixel_weights.m_SrcStart;
          pixel_weights.m_SrcStart = src_min;
        }
        if (pixel_weights.m_SrcEnd >= src_max) {
          pixel_weights.m_SrcEnd = src_max - 1;
        }
        int weight;
        weight = FXSYS_round(
            (FX_FLOAT)(src_pos - pixel_weights.m_SrcStart - 1.0f / 2) * 256);
        if (start == end) {
          pixel_weights.m_Weights[0] =
              (SDP_Table[256 + weight] + SDP_Table[weight] +
               SDP_Table[256 - weight] + SDP_Table[512 - weight])
              << 8;
        } else if ((start == pixel_weights.m_SrcStart &&
                    (pixel_weights.m_SrcStart == pixel_weights.m_SrcEnd ||
                     end == pixel_weights.m_SrcEnd) &&
                    start < end) ||
                   (start < pixel_weights.m_SrcStart &&
                    pixel_weights.m_SrcStart == pixel_weights.m_SrcEnd &&
                    end == pixel_weights.m_SrcEnd)) {
          if (start < pixel_weights.m_SrcStart) {
            pixel_weights.m_Weights[0] = SDP_Table[256 + weight] << 8;
            pixel_weights.m_Weights[1] =
                (SDP_Table[weight] + SDP_Table[256 - weight] +
                 SDP_Table[512 - weight])
                << 8;
          } else {
            if (pixel_weights.m_SrcStart == pixel_weights.m_SrcEnd) {
              pixel_weights.m_Weights[0] =
                  (SDP_Table[256 + weight] + SDP_Table[weight] +
                   SDP_Table[256 - weight])
                  << 8;
              pixel_weights.m_Weights[1] = SDP_Table[512 - weight] << 8;
            } else {
              pixel_weights.m_Weights[0] =
                  (SDP_Table[256 + weight] + SDP_Table[weight]) << 8;
              pixel_weights.m_Weights[1] =
                  (SDP_Table[256 - weight] + SDP_Table[512 - weight]) << 8;
            }
          }
          if (pixel_weights.m_SrcStart == pixel_weights.m_SrcEnd) {
            pixel_weights.m_SrcEnd = end;
          }
          if (start < pixel_weights.m_SrcStart) {
            pixel_weights.m_SrcStart = start;
          }
        } else if (start == pixel_weights.m_SrcStart &&
                   start < pixel_weights.m_SrcEnd &&
                   pixel_weights.m_SrcEnd < end) {
          pixel_weights.m_Weights[0] =
              (SDP_Table[256 + weight] + SDP_Table[weight]) << 8;
          pixel_weights.m_Weights[1] = SDP_Table[256 - weight] << 8;
          pixel_weights.m_Weights[2] = SDP_Table[512 - weight] << 8;
          pixel_weights.m_SrcEnd = end;
        } else if (start < pixel_weights.m_SrcStart &&
                   pixel_weights.m_SrcStart < pixel_weights.m_SrcEnd &&
                   pixel_weights.m_SrcEnd == end) {
          pixel_weights.m_Weights[0] = SDP_Table[256 + weight] << 8;
          pixel_weights.m_Weights[1] = SDP_Table[weight] << 8;
          pixel_weights.m_Weights[2] =
              (SDP_Table[256 - weight] + SDP_Table[512 - weight]) << 8;
          pixel_weights.m_SrcStart = start;
        } else {
          pixel_weights.m_Weights[0] = SDP_Table[256 + weight] << 8;
          pixel_weights.m_Weights[1] = SDP_Table[weight] << 8;
          pixel_weights.m_Weights[2] = SDP_Table[256 - weight] << 8;
          pixel_weights.m_Weights[3] = SDP_Table[512 - weight] << 8;
          pixel_weights.m_SrcStart = start;
          pixel_weights.m_SrcEnd = end;
        }
      } else {
        pixel_weights.m_SrcStart = pixel_weights.m_SrcEnd =
            (int)FXSYS_floor((FX_FLOAT)src_pos);
        if (pixel_weights.m_SrcStart < src_min) {
          pixel_weights.m_SrcStart = src_min;
        }
        if (pixel_weights.m_SrcEnd >= src_max) {
          pixel_weights.m_SrcEnd = src_max - 1;
        }
        pixel_weights.m_Weights[0] = 65536;
      }
    }
    return true;
  }

  for (int dest_pixel = dest_min; dest_pixel < dest_max; dest_pixel++) {
    PixelWeight& pixel_weights = *GetPixelWeight(dest_pixel);
    double src_start = dest_pixel * scale + base;
    double src_end = src_start + scale;
    int start_i, end_i;
    if (src_start < src_end) {
      start_i = (int)FXSYS_floor((FX_FLOAT)src_start);
      end_i = (int)FXSYS_ceil((FX_FLOAT)src_end);
    } else {
      start_i = (int)FXSYS_floor((FX_FLOAT)src_end);
      end_i = (int)FXSYS_ceil((FX_FLOAT)src_start);
    }
    if (start_i < src_min) {
      start_i = src_min;
    }
    if (end_i >= src_max) {
      end_i = src_max - 1;
    }
    if (start_i > end_i) {
      if (start_i >= src_max) {
        start_i = src_max - 1;
      }
      pixel_weights.m_SrcStart = start_i;
      pixel_weights.m_SrcEnd = start_i;
      continue;
    }
    pixel_weights.m_SrcStart = start_i;
    pixel_weights.m_SrcEnd = end_i;
    for (int j = start_i; j <= end_i; j++) {
      double dest_start = ((FX_FLOAT)j - base) / scale;
      double dest_end = ((FX_FLOAT)(j + 1) - base) / scale;
      if (dest_start > dest_end) {
        double temp = dest_start;
        dest_start = dest_end;
        dest_end = temp;
      }
      double area_start = dest_start > (FX_FLOAT)(dest_pixel)
                              ? dest_start
                              : (FX_FLOAT)(dest_pixel);
      double area_end = dest_end > (FX_FLOAT)(dest_pixel + 1)
                            ? (FX_FLOAT)(dest_pixel + 1)
                            : dest_end;
      double weight = area_start >= area_end ? 0.0f : area_end - area_start;
      if (weight == 0 && j == end_i) {
        pixel_weights.m_SrcEnd--;
        break;
      }
      size_t idx = j - start_i;
      if (idx >= GetPixelWeightSize())
        return false;
      pixel_weights.m_Weights[idx] = FXSYS_round((FX_FLOAT)(weight * 65536));
    }
  }
  return true;
}

PixelWeight* CWeightTable::GetPixelWeight(int pixel) const {
  ASSERT(pixel >= m_DestMin);
  return reinterpret_cast<PixelWeight*>(m_pWeightTables +
                                        (pixel - m_DestMin) * m_ItemSize);
}

int* CWeightTable::GetValueFromPixelWeight(PixelWeight* pWeight,
                                           int index) const {
  if (index < pWeight->m_SrcStart)
    return nullptr;

  size_t idx = index - pWeight->m_SrcStart;
  return idx < GetPixelWeightSize() ? &pWeight->m_Weights[idx] : nullptr;
}

CStretchEngine::CStretchEngine(IFX_ScanlineComposer* pDestBitmap,
                               FXDIB_Format dest_format,
                               int dest_width,
                               int dest_height,
                               const FX_RECT& clip_rect,
                               const CFX_DIBSource* pSrcBitmap,
                               int flags) {
  m_State = 0;
  m_DestFormat = dest_format;
  m_DestBpp = dest_format & 0xff;
  m_SrcBpp = pSrcBitmap->GetFormat() & 0xff;
  m_bHasAlpha = pSrcBitmap->GetFormat() & 0x200;
  m_pSrcPalette = pSrcBitmap->GetPalette();
  m_pDestBitmap = pDestBitmap;
  m_DestWidth = dest_width;
  m_DestHeight = dest_height;
  m_pInterBuf = nullptr;
  m_pExtraAlphaBuf = nullptr;
  m_pDestMaskScanline = nullptr;
  m_DestClip = clip_rect;
  uint32_t size = clip_rect.Width();
  if (size && m_DestBpp > (int)(INT_MAX / size)) {
    return;
  }
  size *= m_DestBpp;
  if (size > INT_MAX - 31) {
    return;
  }
  size += 31;
  size = size / 32 * 4;
  m_pDestScanline = FX_TryAlloc(uint8_t, size);
  if (!m_pDestScanline) {
    return;
  }
  if (dest_format == FXDIB_Rgb32) {
    FXSYS_memset(m_pDestScanline, 255, size);
  }
  m_InterPitch = (m_DestClip.Width() * m_DestBpp + 31) / 32 * 4;
  m_ExtraMaskPitch = (m_DestClip.Width() * 8 + 31) / 32 * 4;
  m_pInterBuf = nullptr;
  m_pSource = pSrcBitmap;
  m_SrcWidth = pSrcBitmap->GetWidth();
  m_SrcHeight = pSrcBitmap->GetHeight();
  m_SrcPitch = (m_SrcWidth * m_SrcBpp + 31) / 32 * 4;
  if ((flags & FXDIB_NOSMOOTH) == 0) {
    bool bInterpol = flags & FXDIB_INTERPOL || flags & FXDIB_BICUBIC_INTERPOL;
    if (!bInterpol && FXSYS_abs(dest_width) != 0 &&
        FXSYS_abs(dest_height) / 8 < static_cast<long long>(m_SrcWidth) *
                                         m_SrcHeight / FXSYS_abs(dest_width)) {
      flags = FXDIB_INTERPOL;
    }
    m_Flags = flags;
  } else {
    m_Flags = FXDIB_NOSMOOTH;
    if (flags & FXDIB_DOWNSAMPLE) {
      m_Flags |= FXDIB_DOWNSAMPLE;
    }
  }
  double scale_x = (FX_FLOAT)m_SrcWidth / (FX_FLOAT)m_DestWidth;
  double scale_y = (FX_FLOAT)m_SrcHeight / (FX_FLOAT)m_DestHeight;
  double base_x = m_DestWidth > 0 ? 0.0f : (FX_FLOAT)(m_DestWidth);
  double base_y = m_DestHeight > 0 ? 0.0f : (FX_FLOAT)(m_DestHeight);
  double src_left = scale_x * ((FX_FLOAT)(clip_rect.left) + base_x);
  double src_right = scale_x * ((FX_FLOAT)(clip_rect.right) + base_x);
  double src_top = scale_y * ((FX_FLOAT)(clip_rect.top) + base_y);
  double src_bottom = scale_y * ((FX_FLOAT)(clip_rect.bottom) + base_y);
  if (src_left > src_right) {
    double temp = src_left;
    src_left = src_right;
    src_right = temp;
  }
  if (src_top > src_bottom) {
    double temp = src_top;
    src_top = src_bottom;
    src_bottom = temp;
  }
  m_SrcClip.left = (int)FXSYS_floor((FX_FLOAT)src_left);
  m_SrcClip.right = (int)FXSYS_ceil((FX_FLOAT)src_right);
  m_SrcClip.top = (int)FXSYS_floor((FX_FLOAT)src_top);
  m_SrcClip.bottom = (int)FXSYS_ceil((FX_FLOAT)src_bottom);
  FX_RECT src_rect(0, 0, m_SrcWidth, m_SrcHeight);
  m_SrcClip.Intersect(src_rect);
  if (m_SrcBpp == 1) {
    if (m_DestBpp == 8) {
      m_TransMethod = 1;
    } else {
      m_TransMethod = 2;
    }
  } else if (m_SrcBpp == 8) {
    if (m_DestBpp == 8) {
      if (!m_bHasAlpha) {
        m_TransMethod = 3;
      } else {
        m_TransMethod = 4;
      }
    } else {
      if (!m_bHasAlpha) {
        m_TransMethod = 5;
      } else {
        m_TransMethod = 6;
      }
    }
  } else {
    if (!m_bHasAlpha) {
      m_TransMethod = 7;
    } else {
      m_TransMethod = 8;
    }
  }
}

CStretchEngine::~CStretchEngine() {
  FX_Free(m_pDestScanline);
  FX_Free(m_pInterBuf);
  FX_Free(m_pExtraAlphaBuf);
  FX_Free(m_pDestMaskScanline);
}

bool CStretchEngine::Continue(IFX_Pause* pPause) {
  while (m_State == 1) {
    if (ContinueStretchHorz(pPause)) {
      return true;
    }
    m_State = 2;
    StretchVert();
  }
  return false;
}

bool CStretchEngine::StartStretchHorz() {
  if (m_DestWidth == 0 || m_InterPitch == 0 || !m_pDestScanline)
    return false;

  if (m_SrcClip.Height() == 0 ||
      m_SrcClip.Height() > (1 << 29) / m_InterPitch) {
    return false;
  }

  m_pInterBuf = FX_TryAlloc(unsigned char, m_SrcClip.Height() * m_InterPitch);
  if (!m_pInterBuf)
    return false;

  if (m_pSource && m_bHasAlpha && m_pSource->m_pAlphaMask) {
    m_pExtraAlphaBuf =
        FX_Alloc2D(unsigned char, m_SrcClip.Height(), m_ExtraMaskPitch);
    uint32_t size = (m_DestClip.Width() * 8 + 31) / 32 * 4;
    m_pDestMaskScanline = FX_TryAlloc(unsigned char, size);
    if (!m_pDestMaskScanline)
      return false;
  }
  bool ret =
      m_WeightTable.Calc(m_DestWidth, m_DestClip.left, m_DestClip.right,
                         m_SrcWidth, m_SrcClip.left, m_SrcClip.right, m_Flags);
  if (!ret)
    return false;

  m_CurRow = m_SrcClip.top;
  m_State = 1;
  return true;
}

bool CStretchEngine::ContinueStretchHorz(IFX_Pause* pPause) {
  if (!m_DestWidth)
    return false;

  if (m_pSource->SkipToScanline(m_CurRow, pPause))
    return true;

  int Bpp = m_DestBpp / 8;
  static const int kStrechPauseRows = 10;
  int rows_to_go = kStrechPauseRows;
  for (; m_CurRow < m_SrcClip.bottom; m_CurRow++) {
    if (rows_to_go == 0) {
      if (pPause && pPause->NeedToPauseNow())
        return true;

      rows_to_go = kStrechPauseRows;
    }

    const uint8_t* src_scan = m_pSource->GetScanline(m_CurRow);
    uint8_t* dest_scan =
        m_pInterBuf + (m_CurRow - m_SrcClip.top) * m_InterPitch;
    const uint8_t* src_scan_mask = nullptr;
    uint8_t* dest_scan_mask = nullptr;
    if (m_pExtraAlphaBuf) {
      src_scan_mask = m_pSource->m_pAlphaMask->GetScanline(m_CurRow);
      dest_scan_mask =
          m_pExtraAlphaBuf + (m_CurRow - m_SrcClip.top) * m_ExtraMaskPitch;
    }
    switch (m_TransMethod) {
      case 1:
      case 2: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          PixelWeight* pPixelWeights = m_WeightTable.GetPixelWeight(col);
          int dest_a = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight =
                m_WeightTable.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return false;

            int pixel_weight = *pWeight;
            if (src_scan[j / 8] & (1 << (7 - j % 8))) {
              dest_a += pixel_weight * 255;
            }
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_a = dest_a < 0 ? 0 : dest_a > 16711680 ? 16711680 : dest_a;
          }
          *dest_scan++ = (uint8_t)(dest_a >> 16);
        }
        break;
      }
      case 3: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          PixelWeight* pPixelWeights = m_WeightTable.GetPixelWeight(col);
          int dest_a = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight =
                m_WeightTable.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return false;

            int pixel_weight = *pWeight;
            dest_a += pixel_weight * src_scan[j];
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_a = dest_a < 0 ? 0 : dest_a > 16711680 ? 16711680 : dest_a;
          }
          *dest_scan++ = (uint8_t)(dest_a >> 16);
        }
        break;
      }
      case 4: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          PixelWeight* pPixelWeights = m_WeightTable.GetPixelWeight(col);
          int dest_a = 0, dest_r = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight =
                m_WeightTable.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return false;

            int pixel_weight = *pWeight;
            pixel_weight = pixel_weight * src_scan_mask[j] / 255;
            dest_r += pixel_weight * src_scan[j];
            dest_a += pixel_weight;
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_r = dest_r < 0 ? 0 : dest_r > 16711680 ? 16711680 : dest_r;
            dest_a = dest_a < 0 ? 0 : dest_a > 65536 ? 65536 : dest_a;
          }
          *dest_scan++ = (uint8_t)(dest_r >> 16);
          *dest_scan_mask++ = (uint8_t)((dest_a * 255) >> 16);
        }
        break;
      }
      case 5: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          PixelWeight* pPixelWeights = m_WeightTable.GetPixelWeight(col);
          int dest_r_y = 0, dest_g_m = 0, dest_b_c = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight =
                m_WeightTable.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return false;

            int pixel_weight = *pWeight;
            unsigned long argb_cmyk = m_pSrcPalette[src_scan[j]];
            if (m_DestFormat == FXDIB_Rgb) {
              dest_r_y += pixel_weight * (uint8_t)(argb_cmyk >> 16);
              dest_g_m += pixel_weight * (uint8_t)(argb_cmyk >> 8);
              dest_b_c += pixel_weight * (uint8_t)argb_cmyk;
            } else {
              dest_b_c += pixel_weight * (uint8_t)(argb_cmyk >> 24);
              dest_g_m += pixel_weight * (uint8_t)(argb_cmyk >> 16);
              dest_r_y += pixel_weight * (uint8_t)(argb_cmyk >> 8);
            }
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_r_y =
                dest_r_y < 0 ? 0 : dest_r_y > 16711680 ? 16711680 : dest_r_y;
            dest_g_m =
                dest_g_m < 0 ? 0 : dest_g_m > 16711680 ? 16711680 : dest_g_m;
            dest_b_c =
                dest_b_c < 0 ? 0 : dest_b_c > 16711680 ? 16711680 : dest_b_c;
          }
          *dest_scan++ = (uint8_t)(dest_b_c >> 16);
          *dest_scan++ = (uint8_t)(dest_g_m >> 16);
          *dest_scan++ = (uint8_t)(dest_r_y >> 16);
        }
        break;
      }
      case 6: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          PixelWeight* pPixelWeights = m_WeightTable.GetPixelWeight(col);
          int dest_a = 0, dest_r_y = 0, dest_g_m = 0, dest_b_c = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight =
                m_WeightTable.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return false;

            int pixel_weight = *pWeight;
            pixel_weight = pixel_weight * src_scan_mask[j] / 255;
            unsigned long argb_cmyk = m_pSrcPalette[src_scan[j]];
            if (m_DestFormat == FXDIB_Rgba) {
              dest_r_y += pixel_weight * (uint8_t)(argb_cmyk >> 16);
              dest_g_m += pixel_weight * (uint8_t)(argb_cmyk >> 8);
              dest_b_c += pixel_weight * (uint8_t)argb_cmyk;
            } else {
              dest_b_c += pixel_weight * (uint8_t)(argb_cmyk >> 24);
              dest_g_m += pixel_weight * (uint8_t)(argb_cmyk >> 16);
              dest_r_y += pixel_weight * (uint8_t)(argb_cmyk >> 8);
            }
            dest_a += pixel_weight;
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_b_c =
                dest_b_c < 0 ? 0 : dest_b_c > 16711680 ? 16711680 : dest_b_c;
            dest_g_m =
                dest_g_m < 0 ? 0 : dest_g_m > 16711680 ? 16711680 : dest_g_m;
            dest_r_y =
                dest_r_y < 0 ? 0 : dest_r_y > 16711680 ? 16711680 : dest_r_y;
            dest_a = dest_a < 0 ? 0 : dest_a > 65536 ? 65536 : dest_a;
          }
          *dest_scan++ = (uint8_t)(dest_b_c >> 16);
          *dest_scan++ = (uint8_t)(dest_g_m >> 16);
          *dest_scan++ = (uint8_t)(dest_r_y >> 16);
          *dest_scan_mask++ = (uint8_t)((dest_a * 255) >> 16);
        }
        break;
      }
      case 7: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          PixelWeight* pPixelWeights = m_WeightTable.GetPixelWeight(col);
          int dest_r_y = 0, dest_g_m = 0, dest_b_c = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight =
                m_WeightTable.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return false;

            int pixel_weight = *pWeight;
            const uint8_t* src_pixel = src_scan + j * Bpp;
            dest_b_c += pixel_weight * (*src_pixel++);
            dest_g_m += pixel_weight * (*src_pixel++);
            dest_r_y += pixel_weight * (*src_pixel);
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_b_c =
                dest_b_c < 0 ? 0 : dest_b_c > 16711680 ? 16711680 : dest_b_c;
            dest_g_m =
                dest_g_m < 0 ? 0 : dest_g_m > 16711680 ? 16711680 : dest_g_m;
            dest_r_y =
                dest_r_y < 0 ? 0 : dest_r_y > 16711680 ? 16711680 : dest_r_y;
          }
          *dest_scan++ = (uint8_t)((dest_b_c) >> 16);
          *dest_scan++ = (uint8_t)((dest_g_m) >> 16);
          *dest_scan++ = (uint8_t)((dest_r_y) >> 16);
          dest_scan += Bpp - 3;
        }
        break;
      }
      case 8: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          PixelWeight* pPixelWeights = m_WeightTable.GetPixelWeight(col);
          int dest_a = 0, dest_r_y = 0, dest_g_m = 0, dest_b_c = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight =
                m_WeightTable.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return false;

            int pixel_weight = *pWeight;
            const uint8_t* src_pixel = src_scan + j * Bpp;
            if (m_DestFormat == FXDIB_Argb) {
              pixel_weight = pixel_weight * src_pixel[3] / 255;
            } else {
              pixel_weight = pixel_weight * src_scan_mask[j] / 255;
            }
            dest_b_c += pixel_weight * (*src_pixel++);
            dest_g_m += pixel_weight * (*src_pixel++);
            dest_r_y += pixel_weight * (*src_pixel);
            dest_a += pixel_weight;
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_r_y =
                dest_r_y < 0 ? 0 : dest_r_y > 16711680 ? 16711680 : dest_r_y;
            dest_g_m =
                dest_g_m < 0 ? 0 : dest_g_m > 16711680 ? 16711680 : dest_g_m;
            dest_b_c =
                dest_b_c < 0 ? 0 : dest_b_c > 16711680 ? 16711680 : dest_b_c;
            dest_a = dest_a < 0 ? 0 : dest_a > 65536 ? 65536 : dest_a;
          }
          *dest_scan++ = (uint8_t)((dest_b_c) >> 16);
          *dest_scan++ = (uint8_t)((dest_g_m) >> 16);
          *dest_scan++ = (uint8_t)((dest_r_y) >> 16);
          if (m_DestFormat == FXDIB_Argb) {
            *dest_scan = (uint8_t)((dest_a * 255) >> 16);
          }
          if (dest_scan_mask) {
            *dest_scan_mask++ = (uint8_t)((dest_a * 255) >> 16);
          }
          dest_scan += Bpp - 3;
        }
        break;
      }
    }
    rows_to_go--;
  }
  return false;
}

void CStretchEngine::StretchVert() {
  if (m_DestHeight == 0)
    return;

  CWeightTable table;
  bool ret = table.Calc(m_DestHeight, m_DestClip.top, m_DestClip.bottom,
                        m_SrcHeight, m_SrcClip.top, m_SrcClip.bottom, m_Flags);
  if (!ret)
    return;

  const int DestBpp = m_DestBpp / 8;
  for (int row = m_DestClip.top; row < m_DestClip.bottom; row++) {
    unsigned char* dest_scan = m_pDestScanline;
    unsigned char* dest_scan_mask = m_pDestMaskScanline;
    PixelWeight* pPixelWeights = table.GetPixelWeight(row);
    switch (m_TransMethod) {
      case 1:
      case 2:
      case 3: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          unsigned char* src_scan =
              m_pInterBuf + (col - m_DestClip.left) * DestBpp;
          int dest_a = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight = table.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return;

            int pixel_weight = *pWeight;
            dest_a +=
                pixel_weight * src_scan[(j - m_SrcClip.top) * m_InterPitch];
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_a = dest_a < 0 ? 0 : dest_a > 16711680 ? 16711680 : dest_a;
          }
          *dest_scan = (uint8_t)(dest_a >> 16);
          dest_scan += DestBpp;
        }
        break;
      }
      case 4: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          unsigned char* src_scan =
              m_pInterBuf + (col - m_DestClip.left) * DestBpp;
          unsigned char* src_scan_mask =
              m_pExtraAlphaBuf + (col - m_DestClip.left);
          int dest_a = 0, dest_k = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight = table.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return;

            int pixel_weight = *pWeight;
            dest_k +=
                pixel_weight * src_scan[(j - m_SrcClip.top) * m_InterPitch];
            dest_a += pixel_weight *
                      src_scan_mask[(j - m_SrcClip.top) * m_ExtraMaskPitch];
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_k = dest_k < 0 ? 0 : dest_k > 16711680 ? 16711680 : dest_k;
            dest_a = dest_a < 0 ? 0 : dest_a > 16711680 ? 16711680 : dest_a;
          }
          *dest_scan = (uint8_t)(dest_k >> 16);
          dest_scan += DestBpp;
          *dest_scan_mask++ = (uint8_t)(dest_a >> 16);
        }
        break;
      }
      case 5:
      case 7: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          unsigned char* src_scan =
              m_pInterBuf + (col - m_DestClip.left) * DestBpp;
          int dest_r_y = 0, dest_g_m = 0, dest_b_c = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight = table.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return;

            int pixel_weight = *pWeight;
            const uint8_t* src_pixel =
                src_scan + (j - m_SrcClip.top) * m_InterPitch;
            dest_b_c += pixel_weight * (*src_pixel++);
            dest_g_m += pixel_weight * (*src_pixel++);
            dest_r_y += pixel_weight * (*src_pixel);
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_r_y =
                dest_r_y < 0 ? 0 : dest_r_y > 16711680 ? 16711680 : dest_r_y;
            dest_g_m =
                dest_g_m < 0 ? 0 : dest_g_m > 16711680 ? 16711680 : dest_g_m;
            dest_b_c =
                dest_b_c < 0 ? 0 : dest_b_c > 16711680 ? 16711680 : dest_b_c;
          }
          dest_scan[0] = (uint8_t)((dest_b_c) >> 16);
          dest_scan[1] = (uint8_t)((dest_g_m) >> 16);
          dest_scan[2] = (uint8_t)((dest_r_y) >> 16);
          dest_scan += DestBpp;
        }
        break;
      }
      case 6:
      case 8: {
        for (int col = m_DestClip.left; col < m_DestClip.right; col++) {
          unsigned char* src_scan =
              m_pInterBuf + (col - m_DestClip.left) * DestBpp;
          unsigned char* src_scan_mask = nullptr;
          if (m_DestFormat != FXDIB_Argb) {
            src_scan_mask = m_pExtraAlphaBuf + (col - m_DestClip.left);
          }
          int dest_a = 0, dest_r_y = 0, dest_g_m = 0, dest_b_c = 0;
          for (int j = pPixelWeights->m_SrcStart; j <= pPixelWeights->m_SrcEnd;
               j++) {
            int* pWeight = table.GetValueFromPixelWeight(pPixelWeights, j);
            if (!pWeight)
              return;

            int pixel_weight = *pWeight;
            const uint8_t* src_pixel =
                src_scan + (j - m_SrcClip.top) * m_InterPitch;
            int mask_v = 255;
            if (src_scan_mask) {
              mask_v = src_scan_mask[(j - m_SrcClip.top) * m_ExtraMaskPitch];
            }
            dest_b_c += pixel_weight * (*src_pixel++);
            dest_g_m += pixel_weight * (*src_pixel++);
            dest_r_y += pixel_weight * (*src_pixel);
            if (m_DestFormat == FXDIB_Argb) {
              dest_a += pixel_weight * (*(src_pixel + 1));
            } else {
              dest_a += pixel_weight * mask_v;
            }
          }
          if (m_Flags & FXDIB_BICUBIC_INTERPOL) {
            dest_r_y =
                dest_r_y < 0 ? 0 : dest_r_y > 16711680 ? 16711680 : dest_r_y;
            dest_g_m =
                dest_g_m < 0 ? 0 : dest_g_m > 16711680 ? 16711680 : dest_g_m;
            dest_b_c =
                dest_b_c < 0 ? 0 : dest_b_c > 16711680 ? 16711680 : dest_b_c;
            dest_a = dest_a < 0 ? 0 : dest_a > 16711680 ? 16711680 : dest_a;
          }
          if (dest_a) {
            int r = ((uint32_t)dest_r_y) * 255 / dest_a;
            int g = ((uint32_t)dest_g_m) * 255 / dest_a;
            int b = ((uint32_t)dest_b_c) * 255 / dest_a;
            dest_scan[0] = b > 255 ? 255 : b < 0 ? 0 : b;
            dest_scan[1] = g > 255 ? 255 : g < 0 ? 0 : g;
            dest_scan[2] = r > 255 ? 255 : r < 0 ? 0 : r;
          }
          if (m_DestFormat == FXDIB_Argb) {
            dest_scan[3] = (uint8_t)((dest_a) >> 16);
          } else {
            *dest_scan_mask = (uint8_t)((dest_a) >> 16);
          }
          dest_scan += DestBpp;
          if (dest_scan_mask) {
            dest_scan_mask++;
          }
        }
        break;
      }
    }
    m_pDestBitmap->ComposeScanline(row - m_DestClip.top, m_pDestScanline,
                                   m_pDestMaskScanline);
  }
}

CFX_ImageStretcher::CFX_ImageStretcher(IFX_ScanlineComposer* pDest,
                                       const CFX_DIBSource* pSource,
                                       int dest_width,
                                       int dest_height,
                                       const FX_RECT& bitmap_rect,
                                       uint32_t flags)
    : m_pDest(pDest),
      m_pSource(pSource),
      m_Flags(flags),
      m_bFlipX(false),
      m_bFlipY(false),
      m_DestWidth(dest_width),
      m_DestHeight(dest_height),
      m_ClipRect(bitmap_rect),
      m_DestFormat(GetStretchedFormat(*pSource)),
      m_DestBPP(m_DestFormat & 0xff),
      m_LineIndex(0) {}

CFX_ImageStretcher::~CFX_ImageStretcher() {
}

bool CFX_ImageStretcher::Start() {
  if (m_DestWidth == 0 || m_DestHeight == 0)
    return false;

  if (m_pSource->GetFormat() == FXDIB_1bppRgb && m_pSource->GetPalette()) {
    FX_ARGB pal[256];
    int a0, r0, g0, b0, a1, r1, g1, b1;
    ArgbDecode(m_pSource->GetPaletteEntry(0), a0, r0, g0, b0);
    ArgbDecode(m_pSource->GetPaletteEntry(1), a1, r1, g1, b1);
    for (int i = 0; i < 256; i++) {
      int a = a0 + (a1 - a0) * i / 255;
      int r = r0 + (r1 - r0) * i / 255;
      int g = g0 + (g1 - g0) * i / 255;
      int b = b0 + (b1 - b0) * i / 255;
      pal[i] = ArgbEncode(a, r, g, b);
    }
    if (!m_pDest->SetInfo(m_ClipRect.Width(), m_ClipRect.Height(), m_DestFormat,
                          pal)) {
      return false;
    }
  } else if (m_pSource->GetFormat() == FXDIB_1bppCmyk &&
             m_pSource->GetPalette()) {
    FX_CMYK pal[256];
    int c0, m0, y0, k0, c1, m1, y1, k1;
    CmykDecode(m_pSource->GetPaletteEntry(0), c0, m0, y0, k0);
    CmykDecode(m_pSource->GetPaletteEntry(1), c1, m1, y1, k1);
    for (int i = 0; i < 256; i++) {
      int c = c0 + (c1 - c0) * i / 255;
      int m = m0 + (m1 - m0) * i / 255;
      int y = y0 + (y1 - y0) * i / 255;
      int k = k0 + (k1 - k0) * i / 255;
      pal[i] = CmykEncode(c, m, y, k);
    }
    if (!m_pDest->SetInfo(m_ClipRect.Width(), m_ClipRect.Height(), m_DestFormat,
                          pal)) {
      return false;
    }
  } else if (!m_pDest->SetInfo(m_ClipRect.Width(), m_ClipRect.Height(),
                               m_DestFormat, nullptr)) {
    return false;
  }

  if (m_Flags & FXDIB_DOWNSAMPLE)
    return StartQuickStretch();
  return StartStretch();
}

bool CFX_ImageStretcher::Continue(IFX_Pause* pPause) {
  if (m_Flags & FXDIB_DOWNSAMPLE)
    return ContinueQuickStretch(pPause);
  return ContinueStretch(pPause);
}

bool CFX_ImageStretcher::StartStretch() {
  m_pStretchEngine = pdfium::MakeUnique<CStretchEngine>(
      m_pDest, m_DestFormat, m_DestWidth, m_DestHeight, m_ClipRect, m_pSource,
      m_Flags);
  m_pStretchEngine->StartStretchHorz();
  if (SourceSizeWithinLimit(m_pSource->GetWidth(), m_pSource->GetHeight())) {
    m_pStretchEngine->Continue(nullptr);
    return false;
  }
  return true;
}

bool CFX_ImageStretcher::ContinueStretch(IFX_Pause* pPause) {
  return m_pStretchEngine && m_pStretchEngine->Continue(pPause);
}

bool CFX_ImageStretcher::StartQuickStretch() {
  if (m_DestWidth < 0) {
    m_bFlipX = true;
    m_DestWidth = -m_DestWidth;
  }
  if (m_DestHeight < 0) {
    m_bFlipY = true;
    m_DestHeight = -m_DestHeight;
  }
  uint32_t size = m_ClipRect.Width();
  if (size && m_DestBPP > (int)(INT_MAX / size)) {
    return false;
  }
  size *= m_DestBPP;
  m_pScanline.reset(FX_Alloc(uint8_t, (size / 8 + 3) / 4 * 4));
  if (m_pSource->m_pAlphaMask)
    m_pMaskScanline.reset(FX_Alloc(uint8_t, (m_ClipRect.Width() + 3) / 4 * 4));

  if (SourceSizeWithinLimit(m_pSource->GetWidth(), m_pSource->GetHeight())) {
    ContinueQuickStretch(nullptr);
    return false;
  }
  return true;
}

bool CFX_ImageStretcher::ContinueQuickStretch(IFX_Pause* pPause) {
  if (!m_pScanline)
    return false;

  int result_width = m_ClipRect.Width();
  int result_height = m_ClipRect.Height();
  int src_height = m_pSource->GetHeight();
  for (; m_LineIndex < result_height; m_LineIndex++) {
    int dest_y;
    int src_y;
    if (m_bFlipY) {
      dest_y = result_height - m_LineIndex - 1;
      src_y = (m_DestHeight - (dest_y + m_ClipRect.top) - 1) * src_height /
              m_DestHeight;
    } else {
      dest_y = m_LineIndex;
      src_y = (dest_y + m_ClipRect.top) * src_height / m_DestHeight;
    }
    src_y = std::max(std::min(src_y, src_height - 1), 0);

    if (m_pSource->SkipToScanline(src_y, pPause))
      return true;

    m_pSource->DownSampleScanline(src_y, m_pScanline.get(), m_DestBPP,
                                  m_DestWidth, m_bFlipX, m_ClipRect.left,
                                  result_width);
    if (m_pMaskScanline) {
      m_pSource->m_pAlphaMask->DownSampleScanline(
          src_y, m_pMaskScanline.get(), 1, m_DestWidth, m_bFlipX,
          m_ClipRect.left, result_width);
    }
    m_pDest->ComposeScanline(dest_y, m_pScanline.get(), m_pMaskScanline.get());
  }
  return false;
}
