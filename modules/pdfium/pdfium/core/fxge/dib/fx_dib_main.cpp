// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/fx_dib.h"

#include <limits.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "core/fxcodec/fx_codec.h"
#include "core/fxcrt/cfx_maybe_owned.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/dib/dib_int.h"
#include "core/fxge/ge/cfx_cliprgn.h"
#include "third_party/base/ptr_util.h"

void CmykDecode(uint32_t cmyk, int& c, int& m, int& y, int& k) {
  c = FXSYS_GetCValue(cmyk);
  m = FXSYS_GetMValue(cmyk);
  y = FXSYS_GetYValue(cmyk);
  k = FXSYS_GetKValue(cmyk);
}

void ArgbDecode(uint32_t argb, int& a, int& r, int& g, int& b) {
  a = FXARGB_A(argb);
  r = FXARGB_R(argb);
  g = FXARGB_G(argb);
  b = FXARGB_B(argb);
}

void ArgbDecode(uint32_t argb, int& a, FX_COLORREF& rgb) {
  a = FXARGB_A(argb);
  rgb = FXSYS_RGB(FXARGB_R(argb), FXARGB_G(argb), FXARGB_B(argb));
}

uint32_t ArgbEncode(int a, FX_COLORREF rgb) {
  return FXARGB_MAKE(a, FXSYS_GetRValue(rgb), FXSYS_GetGValue(rgb),
                     FXSYS_GetBValue(rgb));
}

CFX_DIBSource::CFX_DIBSource()
    : m_pAlphaMask(nullptr),
      m_Width(0),
      m_Height(0),
      m_bpp(0),
      m_AlphaFlag(0),
      m_Pitch(0) {}

CFX_DIBSource::~CFX_DIBSource() {
  delete m_pAlphaMask;
}

uint8_t* CFX_DIBSource::GetBuffer() const {
  return nullptr;
}

bool CFX_DIBSource::SkipToScanline(int line, IFX_Pause* pPause) const {
  return false;
}

CFX_DIBitmap::CFX_DIBitmap() {
  m_bExtBuf = false;
  m_pBuffer = nullptr;
  m_pPalette = nullptr;
#ifdef _SKIA_SUPPORT_PATHS_
  m_nFormat = Format::kCleared;
#endif
}

#define _MAX_OOM_LIMIT_ 12000000
bool CFX_DIBitmap::Create(int width,
                          int height,
                          FXDIB_Format format,
                          uint8_t* pBuffer,
                          int pitch) {
  m_pBuffer = nullptr;
  m_bpp = (uint8_t)format;
  m_AlphaFlag = (uint8_t)(format >> 8);
  m_Width = m_Height = m_Pitch = 0;
  if (width <= 0 || height <= 0 || pitch < 0) {
    return false;
  }
  if ((INT_MAX - 31) / width < (format & 0xff)) {
    return false;
  }
  if (!pitch) {
    pitch = (width * (format & 0xff) + 31) / 32 * 4;
  }
  if ((1 << 30) / pitch < height) {
    return false;
  }
  if (pBuffer) {
    m_pBuffer = pBuffer;
    m_bExtBuf = true;
  } else {
    int size = pitch * height + 4;
    int oomlimit = _MAX_OOM_LIMIT_;
    if (oomlimit >= 0 && size >= oomlimit) {
      m_pBuffer = FX_TryAlloc(uint8_t, size);
      if (!m_pBuffer) {
        return false;
      }
    } else {
      m_pBuffer = FX_Alloc(uint8_t, size);
    }
  }
  m_Width = width;
  m_Height = height;
  m_Pitch = pitch;
  if (HasAlpha() && format != FXDIB_Argb) {
    bool ret = true;
    ret = BuildAlphaMask();
    if (!ret) {
      if (!m_bExtBuf) {
        FX_Free(m_pBuffer);
        m_pBuffer = nullptr;
        m_Width = m_Height = m_Pitch = 0;
        return false;
      }
    }
  }
  return true;
}

bool CFX_DIBitmap::Copy(const CFX_DIBSource* pSrc) {
  if (m_pBuffer)
    return false;

  if (!Create(pSrc->GetWidth(), pSrc->GetHeight(), pSrc->GetFormat()))
    return false;

  SetPalette(pSrc->GetPalette());
  SetAlphaMask(pSrc->m_pAlphaMask);
  for (int row = 0; row < pSrc->GetHeight(); row++)
    FXSYS_memcpy(m_pBuffer + row * m_Pitch, pSrc->GetScanline(row), m_Pitch);

  return true;
}

CFX_DIBitmap::~CFX_DIBitmap() {
  if (!m_bExtBuf)
    FX_Free(m_pBuffer);

  m_pBuffer = nullptr;
}

uint8_t* CFX_DIBitmap::GetBuffer() const {
  return m_pBuffer;
}

const uint8_t* CFX_DIBitmap::GetScanline(int line) const {
  return m_pBuffer ? m_pBuffer + line * m_Pitch : nullptr;
}

void CFX_DIBitmap::TakeOver(CFX_DIBitmap* pSrcBitmap) {
  if (!m_bExtBuf)
    FX_Free(m_pBuffer);

  delete m_pAlphaMask;
  m_pBuffer = pSrcBitmap->m_pBuffer;
  m_pPalette = std::move(pSrcBitmap->m_pPalette);
  m_pAlphaMask = pSrcBitmap->m_pAlphaMask;
  pSrcBitmap->m_pBuffer = nullptr;
  pSrcBitmap->m_pAlphaMask = nullptr;
  m_bpp = pSrcBitmap->m_bpp;
  m_bExtBuf = pSrcBitmap->m_bExtBuf;
  m_AlphaFlag = pSrcBitmap->m_AlphaFlag;
  m_Width = pSrcBitmap->m_Width;
  m_Height = pSrcBitmap->m_Height;
  m_Pitch = pSrcBitmap->m_Pitch;
}

std::unique_ptr<CFX_DIBitmap> CFX_DIBSource::Clone(const FX_RECT* pClip) const {
  FX_RECT rect(0, 0, m_Width, m_Height);
  if (pClip) {
    rect.Intersect(*pClip);
    if (rect.IsEmpty())
      return nullptr;
  }
  auto pNewBitmap = pdfium::MakeUnique<CFX_DIBitmap>();
  if (!pNewBitmap->Create(rect.Width(), rect.Height(), GetFormat()))
    return nullptr;

  pNewBitmap->SetPalette(m_pPalette.get());
  pNewBitmap->SetAlphaMask(m_pAlphaMask, pClip);
  if (GetBPP() == 1 && rect.left % 8 != 0) {
    int left_shift = rect.left % 32;
    int right_shift = 32 - left_shift;
    int dword_count = pNewBitmap->m_Pitch / 4;
    for (int row = rect.top; row < rect.bottom; row++) {
      uint32_t* src_scan = (uint32_t*)GetScanline(row) + rect.left / 32;
      uint32_t* dest_scan = (uint32_t*)pNewBitmap->GetScanline(row - rect.top);
      for (int i = 0; i < dword_count; i++) {
        dest_scan[i] =
            (src_scan[i] << left_shift) | (src_scan[i + 1] >> right_shift);
      }
    }
  } else {
    int copy_len = (pNewBitmap->GetWidth() * pNewBitmap->GetBPP() + 7) / 8;
    if (m_Pitch < (uint32_t)copy_len)
      copy_len = m_Pitch;

    for (int row = rect.top; row < rect.bottom; row++) {
      const uint8_t* src_scan = GetScanline(row) + rect.left * m_bpp / 8;
      uint8_t* dest_scan = (uint8_t*)pNewBitmap->GetScanline(row - rect.top);
      FXSYS_memcpy(dest_scan, src_scan, copy_len);
    }
  }
  return pNewBitmap;
}

void CFX_DIBSource::BuildPalette() {
  if (m_pPalette)
    return;

  if (GetBPP() == 1) {
    m_pPalette.reset(FX_Alloc(uint32_t, 2));
    if (IsCmykImage()) {
      m_pPalette.get()[0] = 0xff;
      m_pPalette.get()[1] = 0;
    } else {
      m_pPalette.get()[0] = 0xff000000;
      m_pPalette.get()[1] = 0xffffffff;
    }
  } else if (GetBPP() == 8) {
    m_pPalette.reset(FX_Alloc(uint32_t, 256));
    if (IsCmykImage()) {
      for (int i = 0; i < 256; i++)
        m_pPalette.get()[i] = 0xff - i;
    } else {
      for (int i = 0; i < 256; i++)
        m_pPalette.get()[i] = 0xff000000 | (i * 0x10101);
    }
  }
}

bool CFX_DIBSource::BuildAlphaMask() {
  if (m_pAlphaMask) {
    return true;
  }
  m_pAlphaMask = new CFX_DIBitmap;
  if (!m_pAlphaMask->Create(m_Width, m_Height, FXDIB_8bppMask)) {
    delete m_pAlphaMask;
    m_pAlphaMask = nullptr;
    return false;
  }
  FXSYS_memset(m_pAlphaMask->GetBuffer(), 0xff,
               m_pAlphaMask->GetHeight() * m_pAlphaMask->GetPitch());
  return true;
}

uint32_t CFX_DIBSource::GetPaletteEntry(int index) const {
  ASSERT((GetBPP() == 1 || GetBPP() == 8) && !IsAlphaMask());
  if (m_pPalette) {
    return m_pPalette.get()[index];
  }
  if (IsCmykImage()) {
    if (GetBPP() == 1) {
      return index ? 0 : 0xff;
    }
    return 0xff - index;
  }
  if (GetBPP() == 1) {
    return index ? 0xffffffff : 0xff000000;
  }
  return index * 0x10101 | 0xff000000;
}

void CFX_DIBSource::SetPaletteEntry(int index, uint32_t color) {
  ASSERT((GetBPP() == 1 || GetBPP() == 8) && !IsAlphaMask());
  if (!m_pPalette) {
    BuildPalette();
  }
  m_pPalette.get()[index] = color;
}

int CFX_DIBSource::FindPalette(uint32_t color) const {
  ASSERT((GetBPP() == 1 || GetBPP() == 8) && !IsAlphaMask());
  if (!m_pPalette) {
    if (IsCmykImage()) {
      if (GetBPP() == 1) {
        return ((uint8_t)color == 0xff) ? 0 : 1;
      }
      return 0xff - (uint8_t)color;
    }
    if (GetBPP() == 1) {
      return ((uint8_t)color == 0xff) ? 1 : 0;
    }
    return (uint8_t)color;
  }
  int palsize = (1 << GetBPP());
  for (int i = 0; i < palsize; i++)
    if (m_pPalette.get()[i] == color) {
      return i;
    }
  return -1;
}

void CFX_DIBitmap::Clear(uint32_t color) {
  if (!m_pBuffer) {
    return;
  }
  switch (GetFormat()) {
    case FXDIB_1bppMask:
      FXSYS_memset(m_pBuffer, (color & 0xff000000) ? 0xff : 0,
                   m_Pitch * m_Height);
      break;
    case FXDIB_1bppRgb: {
      int index = FindPalette(color);
      FXSYS_memset(m_pBuffer, index ? 0xff : 0, m_Pitch * m_Height);
      break;
    }
    case FXDIB_8bppMask:
      FXSYS_memset(m_pBuffer, color >> 24, m_Pitch * m_Height);
      break;
    case FXDIB_8bppRgb: {
      int index = FindPalette(color);
      FXSYS_memset(m_pBuffer, index, m_Pitch * m_Height);
      break;
    }
    case FXDIB_Rgb:
    case FXDIB_Rgba: {
      int a, r, g, b;
      ArgbDecode(color, a, r, g, b);
      if (r == g && g == b) {
        FXSYS_memset(m_pBuffer, r, m_Pitch * m_Height);
      } else {
        int byte_pos = 0;
        for (int col = 0; col < m_Width; col++) {
          m_pBuffer[byte_pos++] = b;
          m_pBuffer[byte_pos++] = g;
          m_pBuffer[byte_pos++] = r;
        }
        for (int row = 1; row < m_Height; row++) {
          FXSYS_memcpy(m_pBuffer + row * m_Pitch, m_pBuffer, m_Pitch);
        }
      }
      break;
    }
    case FXDIB_Rgb32:
    case FXDIB_Argb: {
      color = IsCmykImage() ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
#ifdef _SKIA_SUPPORT_
      if (FXDIB_Rgb32 == GetFormat() && !IsCmykImage()) {
        color |= 0xFF000000;
      }
#endif
      for (int i = 0; i < m_Width; i++) {
        ((uint32_t*)m_pBuffer)[i] = color;
      }
      for (int row = 1; row < m_Height; row++) {
        FXSYS_memcpy(m_pBuffer + row * m_Pitch, m_pBuffer, m_Pitch);
      }
      break;
    }
    default:
      break;
  }
}

void CFX_DIBSource::GetOverlapRect(int& dest_left,
                                   int& dest_top,
                                   int& width,
                                   int& height,
                                   int src_width,
                                   int src_height,
                                   int& src_left,
                                   int& src_top,
                                   const CFX_ClipRgn* pClipRgn) {
  if (width == 0 || height == 0) {
    return;
  }
  ASSERT(width > 0 && height > 0);
  if (dest_left > m_Width || dest_top > m_Height) {
    width = 0;
    height = 0;
    return;
  }
  int x_offset = dest_left - src_left;
  int y_offset = dest_top - src_top;
  FX_RECT src_rect(src_left, src_top, src_left + width, src_top + height);
  FX_RECT src_bound(0, 0, src_width, src_height);
  src_rect.Intersect(src_bound);
  FX_RECT dest_rect(src_rect.left + x_offset, src_rect.top + y_offset,
                    src_rect.right + x_offset, src_rect.bottom + y_offset);
  FX_RECT dest_bound(0, 0, m_Width, m_Height);
  dest_rect.Intersect(dest_bound);
  if (pClipRgn) {
    dest_rect.Intersect(pClipRgn->GetBox());
  }
  dest_left = dest_rect.left;
  dest_top = dest_rect.top;
  src_left = dest_left - x_offset;
  src_top = dest_top - y_offset;
  width = dest_rect.right - dest_rect.left;
  height = dest_rect.bottom - dest_rect.top;
}

bool CFX_DIBitmap::TransferBitmap(int dest_left,
                                  int dest_top,
                                  int width,
                                  int height,
                                  const CFX_DIBSource* pSrcBitmap,
                                  int src_left,
                                  int src_top) {
  if (!m_pBuffer)
    return false;

  GetOverlapRect(dest_left, dest_top, width, height, pSrcBitmap->GetWidth(),
                 pSrcBitmap->GetHeight(), src_left, src_top, nullptr);
  if (width == 0 || height == 0)
    return true;

  FXDIB_Format dest_format = GetFormat();
  FXDIB_Format src_format = pSrcBitmap->GetFormat();
  if (dest_format == src_format) {
    if (GetBPP() == 1) {
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = m_pBuffer + (dest_top + row) * m_Pitch;
        const uint8_t* src_scan = pSrcBitmap->GetScanline(src_top + row);
        for (int col = 0; col < width; col++) {
          if (src_scan[(src_left + col) / 8] &
              (1 << (7 - (src_left + col) % 8))) {
            dest_scan[(dest_left + col) / 8] |= 1
                                                << (7 - (dest_left + col) % 8);
          } else {
            dest_scan[(dest_left + col) / 8] &=
                ~(1 << (7 - (dest_left + col) % 8));
          }
        }
      }
    } else {
      int Bpp = GetBPP() / 8;
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan =
            m_pBuffer + (dest_top + row) * m_Pitch + dest_left * Bpp;
        const uint8_t* src_scan =
            pSrcBitmap->GetScanline(src_top + row) + src_left * Bpp;
        FXSYS_memcpy(dest_scan, src_scan, width * Bpp);
      }
    }
  } else {
    if (m_pPalette)
      return false;

    if (m_bpp == 8)
      dest_format = FXDIB_8bppMask;

    uint8_t* dest_buf =
        m_pBuffer + dest_top * m_Pitch + dest_left * GetBPP() / 8;
    std::unique_ptr<uint32_t, FxFreeDeleter> d_plt;
    if (!ConvertBuffer(dest_format, dest_buf, m_Pitch, width, height,
                       pSrcBitmap, src_left, src_top, &d_plt)) {
      return false;
    }
  }
  return true;
}

bool CFX_DIBitmap::TransferMask(int dest_left,
                                int dest_top,
                                int width,
                                int height,
                                const CFX_DIBSource* pMask,
                                uint32_t color,
                                int src_left,
                                int src_top,
                                int alpha_flag,
                                void* pIccTransform) {
  if (!m_pBuffer) {
    return false;
  }
  ASSERT(HasAlpha() && (m_bpp >= 24));
  ASSERT(pMask->IsAlphaMask());
  if (!HasAlpha() || !pMask->IsAlphaMask() || m_bpp < 24) {
    return false;
  }
  GetOverlapRect(dest_left, dest_top, width, height, pMask->GetWidth(),
                 pMask->GetHeight(), src_left, src_top, nullptr);
  if (width == 0 || height == 0) {
    return true;
  }
  int src_bpp = pMask->GetBPP();
  int alpha;
  uint32_t dst_color;
  if (alpha_flag >> 8) {
    alpha = alpha_flag & 0xff;
    dst_color = FXCMYK_TODIB(color);
  } else {
    alpha = FXARGB_A(color);
    dst_color = FXARGB_TODIB(color);
  }
  uint8_t* color_p = (uint8_t*)&dst_color;
  if (pIccTransform && CFX_GEModule::Get()->GetCodecModule() &&
      CFX_GEModule::Get()->GetCodecModule()->GetIccModule()) {
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
    color_p[3] = (uint8_t)alpha;
  }
  if (GetFormat() == FXDIB_Argb) {
    for (int row = 0; row < height; row++) {
      uint32_t* dest_pos =
          (uint32_t*)(m_pBuffer + (dest_top + row) * m_Pitch + dest_left * 4);
      const uint8_t* src_scan = pMask->GetScanline(src_top + row);
      if (src_bpp == 1) {
        for (int col = 0; col < width; col++) {
          int src_bitpos = src_left + col;
          if (src_scan[src_bitpos / 8] & (1 << (7 - src_bitpos % 8))) {
            *dest_pos = dst_color;
          } else {
            *dest_pos = 0;
          }
          dest_pos++;
        }
      } else {
        src_scan += src_left;
        dst_color = FXARGB_TODIB(dst_color);
        dst_color &= 0xffffff;
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_pos++,
                        dst_color | ((alpha * (*src_scan++) / 255) << 24));
        }
      }
    }
  } else {
    int comps = m_bpp / 8;
    for (int row = 0; row < height; row++) {
      uint8_t* dest_color_pos =
          m_pBuffer + (dest_top + row) * m_Pitch + dest_left * comps;
      uint8_t* dest_alpha_pos =
          (uint8_t*)m_pAlphaMask->GetScanline(dest_top + row) + dest_left;
      const uint8_t* src_scan = pMask->GetScanline(src_top + row);
      if (src_bpp == 1) {
        for (int col = 0; col < width; col++) {
          int src_bitpos = src_left + col;
          if (src_scan[src_bitpos / 8] & (1 << (7 - src_bitpos % 8))) {
            FXSYS_memcpy(dest_color_pos, color_p, comps);
            *dest_alpha_pos = 0xff;
          } else {
            FXSYS_memset(dest_color_pos, 0, comps);
            *dest_alpha_pos = 0;
          }
          dest_color_pos += comps;
          dest_alpha_pos++;
        }
      } else {
        src_scan += src_left;
        for (int col = 0; col < width; col++) {
          FXSYS_memcpy(dest_color_pos, color_p, comps);
          dest_color_pos += comps;
          *dest_alpha_pos++ = (alpha * (*src_scan++) / 255);
        }
      }
    }
  }
  return true;
}

void CFX_DIBSource::SetPalette(const uint32_t* pSrc) {
  static const uint32_t kPaletteSize = 256;
  if (!pSrc || GetBPP() > 8) {
    m_pPalette.reset();
    return;
  }
  uint32_t pal_size = 1 << GetBPP();
  if (!m_pPalette)
    m_pPalette.reset(FX_Alloc(uint32_t, pal_size));
  pal_size = std::min(pal_size, kPaletteSize);
  FXSYS_memcpy(m_pPalette.get(), pSrc, pal_size * sizeof(uint32_t));
}

void CFX_DIBSource::GetPalette(uint32_t* pal, int alpha) const {
  ASSERT(GetBPP() <= 8 && !IsCmykImage());
  if (GetBPP() == 1) {
    pal[0] = ((m_pPalette ? m_pPalette.get()[0] : 0xff000000) & 0xffffff) |
             (alpha << 24);
    pal[1] = ((m_pPalette ? m_pPalette.get()[1] : 0xffffffff) & 0xffffff) |
             (alpha << 24);
    return;
  }
  if (m_pPalette) {
    for (int i = 0; i < 256; i++) {
      pal[i] = (m_pPalette.get()[i] & 0x00ffffff) | (alpha << 24);
    }
  } else {
    for (int i = 0; i < 256; i++) {
      pal[i] = (i * 0x10101) | (alpha << 24);
    }
  }
}

std::unique_ptr<CFX_DIBitmap> CFX_DIBSource::CloneAlphaMask(
    const FX_RECT* pClip) const {
  ASSERT(GetFormat() == FXDIB_Argb);
  FX_RECT rect(0, 0, m_Width, m_Height);
  if (pClip) {
    rect.Intersect(*pClip);
    if (rect.IsEmpty())
      return nullptr;
  }
  auto pMask = pdfium::MakeUnique<CFX_DIBitmap>();
  if (!pMask->Create(rect.Width(), rect.Height(), FXDIB_8bppMask))
    return nullptr;

  for (int row = rect.top; row < rect.bottom; row++) {
    const uint8_t* src_scan = GetScanline(row) + rect.left * 4 + 3;
    uint8_t* dest_scan =
        const_cast<uint8_t*>(pMask->GetScanline(row - rect.top));
    for (int col = rect.left; col < rect.right; col++) {
      *dest_scan++ = *src_scan;
      src_scan += 4;
    }
  }
  return pMask;
}

bool CFX_DIBSource::SetAlphaMask(const CFX_DIBSource* pAlphaMask,
                                 const FX_RECT* pClip) {
  if (!HasAlpha() || GetFormat() == FXDIB_Argb)
    return false;

  if (!pAlphaMask) {
    m_pAlphaMask->Clear(0xff000000);
    return true;
  }
  FX_RECT rect(0, 0, pAlphaMask->m_Width, pAlphaMask->m_Height);
  if (pClip) {
    rect.Intersect(*pClip);
    if (rect.IsEmpty() || rect.Width() != m_Width ||
        rect.Height() != m_Height) {
      return false;
    }
  } else {
    if (pAlphaMask->m_Width != m_Width || pAlphaMask->m_Height != m_Height)
      return false;
  }
  for (int row = 0; row < m_Height; row++) {
    FXSYS_memcpy(const_cast<uint8_t*>(m_pAlphaMask->GetScanline(row)),
                 pAlphaMask->GetScanline(row + rect.top) + rect.left,
                 m_pAlphaMask->m_Pitch);
  }
  return true;
}

const int g_ChannelOffset[] = {0, 2, 1, 0, 0, 1, 2, 3, 3};
bool CFX_DIBitmap::LoadChannel(FXDIB_Channel destChannel,
                               CFX_DIBSource* pSrcBitmap,
                               FXDIB_Channel srcChannel) {
  if (!m_pBuffer)
    return false;

  CFX_MaybeOwned<CFX_DIBSource> pSrcClone(pSrcBitmap);
  int srcOffset;
  if (srcChannel == FXDIB_Alpha) {
    if (!pSrcBitmap->HasAlpha() && !pSrcBitmap->IsAlphaMask())
      return false;

    if (pSrcBitmap->GetBPP() == 1) {
      pSrcClone = pSrcBitmap->CloneConvert(FXDIB_8bppMask);
      if (!pSrcClone)
        return false;
    }
    srcOffset = pSrcBitmap->GetFormat() == FXDIB_Argb ? 3 : 0;
  } else {
    if (pSrcBitmap->IsAlphaMask())
      return false;

    if (pSrcBitmap->GetBPP() < 24) {
      if (pSrcBitmap->IsCmykImage()) {
        pSrcClone = pSrcBitmap->CloneConvert(static_cast<FXDIB_Format>(
            (pSrcBitmap->GetFormat() & 0xff00) | 0x20));
      } else {
        pSrcClone = pSrcBitmap->CloneConvert(static_cast<FXDIB_Format>(
            (pSrcBitmap->GetFormat() & 0xff00) | 0x18));
      }
      if (!pSrcClone)
        return false;
    }
    srcOffset = g_ChannelOffset[srcChannel];
  }
  int destOffset = 0;
  if (destChannel == FXDIB_Alpha) {
    if (IsAlphaMask()) {
      if (!ConvertFormat(FXDIB_8bppMask))
        return false;
    } else {
      if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyka : FXDIB_Argb))
        return false;

      if (GetFormat() == FXDIB_Argb)
        destOffset = 3;
    }
  } else {
    if (IsAlphaMask())
      return false;

    if (GetBPP() < 24) {
      if (HasAlpha()) {
        if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyka : FXDIB_Argb))
          return false;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      } else if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyk : FXDIB_Rgb32)) {
#else
      } else if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyk : FXDIB_Rgb)) {
#endif
        return false;
      }
    }
    destOffset = g_ChannelOffset[destChannel];
  }
  if (srcChannel == FXDIB_Alpha && pSrcClone->m_pAlphaMask) {
    CFX_MaybeOwned<CFX_DIBSource> pAlphaMask(pSrcClone->m_pAlphaMask);
    if (pSrcClone->GetWidth() != m_Width ||
        pSrcClone->GetHeight() != m_Height) {
      if (pAlphaMask) {
        pAlphaMask = pAlphaMask->StretchTo(m_Width, m_Height);
        if (!pAlphaMask)
          return false;
      }
    }
    pSrcClone = std::move(pAlphaMask);
    srcOffset = 0;
  } else if (pSrcClone->GetWidth() != m_Width ||
             pSrcClone->GetHeight() != m_Height) {
    std::unique_ptr<CFX_DIBitmap> pSrcMatched =
        pSrcClone->StretchTo(m_Width, m_Height);
    if (!pSrcMatched)
      return false;

    pSrcClone = std::move(pSrcMatched);
  }
  CFX_DIBitmap* pDst = this;
  if (destChannel == FXDIB_Alpha && m_pAlphaMask) {
    pDst = m_pAlphaMask;
    destOffset = 0;
  }
  int srcBytes = pSrcClone->GetBPP() / 8;
  int destBytes = pDst->GetBPP() / 8;
  for (int row = 0; row < m_Height; row++) {
    uint8_t* dest_pos = (uint8_t*)pDst->GetScanline(row) + destOffset;
    const uint8_t* src_pos = pSrcClone->GetScanline(row) + srcOffset;
    for (int col = 0; col < m_Width; col++) {
      *dest_pos = *src_pos;
      dest_pos += destBytes;
      src_pos += srcBytes;
    }
  }
  return true;
}

bool CFX_DIBitmap::LoadChannel(FXDIB_Channel destChannel, int value) {
  if (!m_pBuffer) {
    return false;
  }
  int destOffset;
  if (destChannel == FXDIB_Alpha) {
    if (IsAlphaMask()) {
      if (!ConvertFormat(FXDIB_8bppMask)) {
        return false;
      }
      destOffset = 0;
    } else {
      destOffset = 0;
      if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyka : FXDIB_Argb)) {
        return false;
      }
      if (GetFormat() == FXDIB_Argb) {
        destOffset = 3;
      }
    }
  } else {
    if (IsAlphaMask()) {
      return false;
    }
    if (GetBPP() < 24) {
      if (HasAlpha()) {
        if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyka : FXDIB_Argb)) {
          return false;
        }
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      } else if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyk : FXDIB_Rgb)) {
#else
      } else if (!ConvertFormat(IsCmykImage() ? FXDIB_Cmyk : FXDIB_Rgb32)) {
#endif
        return false;
      }
    }
    destOffset = g_ChannelOffset[destChannel];
  }
  int Bpp = GetBPP() / 8;
  if (Bpp == 1) {
    FXSYS_memset(m_pBuffer, value, m_Height * m_Pitch);
    return true;
  }
  if (destChannel == FXDIB_Alpha && m_pAlphaMask) {
    FXSYS_memset(m_pAlphaMask->GetBuffer(), value,
                 m_pAlphaMask->GetHeight() * m_pAlphaMask->GetPitch());
    return true;
  }
  for (int row = 0; row < m_Height; row++) {
    uint8_t* scan_line = m_pBuffer + row * m_Pitch + destOffset;
    for (int col = 0; col < m_Width; col++) {
      *scan_line = value;
      scan_line += Bpp;
    }
  }
  return true;
}

bool CFX_DIBitmap::MultiplyAlpha(CFX_DIBSource* pSrcBitmap) {
  if (!m_pBuffer)
    return false;

  ASSERT(pSrcBitmap->IsAlphaMask());
  if (!pSrcBitmap->IsAlphaMask())
    return false;

  if (!IsAlphaMask() && !HasAlpha())
    return LoadChannel(FXDIB_Alpha, pSrcBitmap, FXDIB_Alpha);

  CFX_MaybeOwned<CFX_DIBitmap> pSrcClone(
      static_cast<CFX_DIBitmap*>(pSrcBitmap));
  if (pSrcBitmap->GetWidth() != m_Width ||
      pSrcBitmap->GetHeight() != m_Height) {
    pSrcClone = pSrcBitmap->StretchTo(m_Width, m_Height);
    if (!pSrcClone)
      return false;
  }
  if (IsAlphaMask()) {
    if (!ConvertFormat(FXDIB_8bppMask))
      return false;

    for (int row = 0; row < m_Height; row++) {
      uint8_t* dest_scan = m_pBuffer + m_Pitch * row;
      uint8_t* src_scan = pSrcClone->m_pBuffer + pSrcClone->m_Pitch * row;
      if (pSrcClone->GetBPP() == 1) {
        for (int col = 0; col < m_Width; col++) {
          if (!((1 << (7 - col % 8)) & src_scan[col / 8]))
            dest_scan[col] = 0;
        }
      } else {
        for (int col = 0; col < m_Width; col++) {
          *dest_scan = (*dest_scan) * src_scan[col] / 255;
          dest_scan++;
        }
      }
    }
  } else {
    if (GetFormat() == FXDIB_Argb) {
      if (pSrcClone->GetBPP() == 1)
        return false;

      for (int row = 0; row < m_Height; row++) {
        uint8_t* dest_scan = m_pBuffer + m_Pitch * row + 3;
        uint8_t* src_scan = pSrcClone->m_pBuffer + pSrcClone->m_Pitch * row;
        for (int col = 0; col < m_Width; col++) {
          *dest_scan = (*dest_scan) * src_scan[col] / 255;
          dest_scan += 4;
        }
      }
    } else {
      m_pAlphaMask->MultiplyAlpha(pSrcClone.Get());
    }
  }
  return true;
}

bool CFX_DIBitmap::GetGrayData(void* pIccTransform) {
  if (!m_pBuffer) {
    return false;
  }
  switch (GetFormat()) {
    case FXDIB_1bppRgb: {
      if (!m_pPalette) {
        return false;
      }
      uint8_t gray[2];
      for (int i = 0; i < 2; i++) {
        int r = static_cast<uint8_t>(m_pPalette.get()[i] >> 16);
        int g = static_cast<uint8_t>(m_pPalette.get()[i] >> 8);
        int b = static_cast<uint8_t>(m_pPalette.get()[i]);
        gray[i] = static_cast<uint8_t>(FXRGB2GRAY(r, g, b));
      }
      CFX_DIBitmap* pMask = new CFX_DIBitmap;
      if (!pMask->Create(m_Width, m_Height, FXDIB_8bppMask)) {
        delete pMask;
        return false;
      }
      FXSYS_memset(pMask->GetBuffer(), gray[0], pMask->GetPitch() * m_Height);
      for (int row = 0; row < m_Height; row++) {
        uint8_t* src_pos = m_pBuffer + row * m_Pitch;
        uint8_t* dest_pos = (uint8_t*)pMask->GetScanline(row);
        for (int col = 0; col < m_Width; col++) {
          if (src_pos[col / 8] & (1 << (7 - col % 8))) {
            *dest_pos = gray[1];
          }
          dest_pos++;
        }
      }
      TakeOver(pMask);
      delete pMask;
      break;
    }
    case FXDIB_8bppRgb: {
      if (!m_pPalette) {
        return false;
      }
      uint8_t gray[256];
      for (int i = 0; i < 256; i++) {
        int r = static_cast<uint8_t>(m_pPalette.get()[i] >> 16);
        int g = static_cast<uint8_t>(m_pPalette.get()[i] >> 8);
        int b = static_cast<uint8_t>(m_pPalette.get()[i]);
        gray[i] = static_cast<uint8_t>(FXRGB2GRAY(r, g, b));
      }
      CFX_DIBitmap* pMask = new CFX_DIBitmap;
      if (!pMask->Create(m_Width, m_Height, FXDIB_8bppMask)) {
        delete pMask;
        return false;
      }
      for (int row = 0; row < m_Height; row++) {
        uint8_t* dest_pos = pMask->GetBuffer() + row * pMask->GetPitch();
        uint8_t* src_pos = m_pBuffer + row * m_Pitch;
        for (int col = 0; col < m_Width; col++) {
          *dest_pos++ = gray[*src_pos++];
        }
      }
      TakeOver(pMask);
      delete pMask;
      break;
    }
    case FXDIB_Rgb: {
      CFX_DIBitmap* pMask = new CFX_DIBitmap;
      if (!pMask->Create(m_Width, m_Height, FXDIB_8bppMask)) {
        delete pMask;
        return false;
      }
      for (int row = 0; row < m_Height; row++) {
        uint8_t* src_pos = m_pBuffer + row * m_Pitch;
        uint8_t* dest_pos = pMask->GetBuffer() + row * pMask->GetPitch();
        for (int col = 0; col < m_Width; col++) {
          *dest_pos++ = FXRGB2GRAY(src_pos[2], src_pos[1], *src_pos);
          src_pos += 3;
        }
      }
      TakeOver(pMask);
      delete pMask;
      break;
    }
    case FXDIB_Rgb32: {
      CFX_DIBitmap* pMask = new CFX_DIBitmap;
      if (!pMask->Create(m_Width, m_Height, FXDIB_8bppMask)) {
        delete pMask;
        return false;
      }
      for (int row = 0; row < m_Height; row++) {
        uint8_t* src_pos = m_pBuffer + row * m_Pitch;
        uint8_t* dest_pos = pMask->GetBuffer() + row * pMask->GetPitch();
        for (int col = 0; col < m_Width; col++) {
          *dest_pos++ = FXRGB2GRAY(src_pos[2], src_pos[1], *src_pos);
          src_pos += 4;
        }
      }
      TakeOver(pMask);
      delete pMask;
      break;
    }
    default:
      return false;
  }
  return true;
}

bool CFX_DIBitmap::MultiplyAlpha(int alpha) {
  if (!m_pBuffer) {
    return false;
  }
  switch (GetFormat()) {
    case FXDIB_1bppMask:
      if (!ConvertFormat(FXDIB_8bppMask)) {
        return false;
      }
      MultiplyAlpha(alpha);
      break;
    case FXDIB_8bppMask: {
      for (int row = 0; row < m_Height; row++) {
        uint8_t* scan_line = m_pBuffer + row * m_Pitch;
        for (int col = 0; col < m_Width; col++) {
          scan_line[col] = scan_line[col] * alpha / 255;
        }
      }
      break;
    }
    case FXDIB_Argb: {
      for (int row = 0; row < m_Height; row++) {
        uint8_t* scan_line = m_pBuffer + row * m_Pitch + 3;
        for (int col = 0; col < m_Width; col++) {
          *scan_line = (*scan_line) * alpha / 255;
          scan_line += 4;
        }
      }
      break;
    }
    default:
      if (HasAlpha()) {
        m_pAlphaMask->MultiplyAlpha(alpha);
      } else if (IsCmykImage()) {
        if (!ConvertFormat((FXDIB_Format)(GetFormat() | 0x0200))) {
          return false;
        }
        m_pAlphaMask->MultiplyAlpha(alpha);
      } else {
        if (!ConvertFormat(FXDIB_Argb)) {
          return false;
        }
        MultiplyAlpha(alpha);
      }
      break;
  }
  return true;
}

uint32_t CFX_DIBitmap::GetPixel(int x, int y) const {
  if (!m_pBuffer) {
    return 0;
  }
  uint8_t* pos = m_pBuffer + y * m_Pitch + x * GetBPP() / 8;
  switch (GetFormat()) {
    case FXDIB_1bppMask: {
      if ((*pos) & (1 << (7 - x % 8))) {
        return 0xff000000;
      }
      return 0;
    }
    case FXDIB_1bppRgb: {
      if ((*pos) & (1 << (7 - x % 8))) {
        return m_pPalette ? m_pPalette.get()[1] : 0xffffffff;
      }
      return m_pPalette ? m_pPalette.get()[0] : 0xff000000;
    }
    case FXDIB_8bppMask:
      return (*pos) << 24;
    case FXDIB_8bppRgb:
      return m_pPalette ? m_pPalette.get()[*pos]
                        : (0xff000000 | ((*pos) * 0x10101));
    case FXDIB_Rgb:
    case FXDIB_Rgba:
    case FXDIB_Rgb32:
      return FXARGB_GETDIB(pos) | 0xff000000;
    case FXDIB_Argb:
      return FXARGB_GETDIB(pos);
    default:
      break;
  }
  return 0;
}

void CFX_DIBitmap::SetPixel(int x, int y, uint32_t color) {
  if (!m_pBuffer) {
    return;
  }
  if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) {
    return;
  }
  uint8_t* pos = m_pBuffer + y * m_Pitch + x * GetBPP() / 8;
  switch (GetFormat()) {
    case FXDIB_1bppMask:
      if (color >> 24) {
        *pos |= 1 << (7 - x % 8);
      } else {
        *pos &= ~(1 << (7 - x % 8));
      }
      break;
    case FXDIB_1bppRgb:
      if (m_pPalette) {
        if (color == m_pPalette.get()[1]) {
          *pos |= 1 << (7 - x % 8);
        } else {
          *pos &= ~(1 << (7 - x % 8));
        }
      } else {
        if (color == 0xffffffff) {
          *pos |= 1 << (7 - x % 8);
        } else {
          *pos &= ~(1 << (7 - x % 8));
        }
      }
      break;
    case FXDIB_8bppMask:
      *pos = (uint8_t)(color >> 24);
      break;
    case FXDIB_8bppRgb: {
      if (m_pPalette) {
        for (int i = 0; i < 256; i++) {
          if (m_pPalette.get()[i] == color) {
            *pos = (uint8_t)i;
            return;
          }
        }
        *pos = 0;
      } else {
        *pos = FXRGB2GRAY(FXARGB_R(color), FXARGB_G(color), FXARGB_B(color));
      }
      break;
    }
    case FXDIB_Rgb:
    case FXDIB_Rgb32: {
      int alpha = FXARGB_A(color);
      pos[0] = (FXARGB_B(color) * alpha + pos[0] * (255 - alpha)) / 255;
      pos[1] = (FXARGB_G(color) * alpha + pos[1] * (255 - alpha)) / 255;
      pos[2] = (FXARGB_R(color) * alpha + pos[2] * (255 - alpha)) / 255;
      break;
    }
    case FXDIB_Rgba: {
      pos[0] = FXARGB_B(color);
      pos[1] = FXARGB_G(color);
      pos[2] = FXARGB_R(color);
      break;
    }
    case FXDIB_Argb:
      FXARGB_SETDIB(pos, color);
      break;
    default:
      break;
  }
}

void CFX_DIBitmap::DownSampleScanline(int line,
                                      uint8_t* dest_scan,
                                      int dest_bpp,
                                      int dest_width,
                                      bool bFlipX,
                                      int clip_left,
                                      int clip_width) const {
  if (!m_pBuffer) {
    return;
  }
  int src_Bpp = m_bpp / 8;
  uint8_t* scanline = m_pBuffer + line * m_Pitch;
  if (src_Bpp == 0) {
    for (int i = 0; i < clip_width; i++) {
      uint32_t dest_x = clip_left + i;
      uint32_t src_x = dest_x * m_Width / dest_width;
      if (bFlipX) {
        src_x = m_Width - src_x - 1;
      }
      src_x %= m_Width;
      dest_scan[i] = (scanline[src_x / 8] & (1 << (7 - src_x % 8))) ? 255 : 0;
    }
  } else if (src_Bpp == 1) {
    for (int i = 0; i < clip_width; i++) {
      uint32_t dest_x = clip_left + i;
      uint32_t src_x = dest_x * m_Width / dest_width;
      if (bFlipX) {
        src_x = m_Width - src_x - 1;
      }
      src_x %= m_Width;
      int dest_pos = i;
      if (m_pPalette) {
        if (!IsCmykImage()) {
          dest_pos *= 3;
          FX_ARGB argb = m_pPalette.get()[scanline[src_x]];
          dest_scan[dest_pos] = FXARGB_B(argb);
          dest_scan[dest_pos + 1] = FXARGB_G(argb);
          dest_scan[dest_pos + 2] = FXARGB_R(argb);
        } else {
          dest_pos *= 4;
          FX_CMYK cmyk = m_pPalette.get()[scanline[src_x]];
          dest_scan[dest_pos] = FXSYS_GetCValue(cmyk);
          dest_scan[dest_pos + 1] = FXSYS_GetMValue(cmyk);
          dest_scan[dest_pos + 2] = FXSYS_GetYValue(cmyk);
          dest_scan[dest_pos + 3] = FXSYS_GetKValue(cmyk);
        }
      } else {
        dest_scan[dest_pos] = scanline[src_x];
      }
    }
  } else {
    for (int i = 0; i < clip_width; i++) {
      uint32_t dest_x = clip_left + i;
      uint32_t src_x =
          bFlipX ? (m_Width - dest_x * m_Width / dest_width - 1) * src_Bpp
                 : (dest_x * m_Width / dest_width) * src_Bpp;
      src_x %= m_Width * src_Bpp;
      int dest_pos = i * src_Bpp;
      for (int b = 0; b < src_Bpp; b++) {
        dest_scan[dest_pos + b] = scanline[src_x + b];
      }
    }
  }
}

// TODO(weili): Split this function into two for handling CMYK and RGB
// colors separately.
bool CFX_DIBitmap::ConvertColorScale(uint32_t forecolor, uint32_t backcolor) {
  ASSERT(!IsAlphaMask());
  if (!m_pBuffer || IsAlphaMask()) {
    return false;
  }
  // Values used for CMYK colors.
  int fc = 0;
  int fm = 0;
  int fy = 0;
  int fk = 0;
  int bc = 0;
  int bm = 0;
  int by = 0;
  int bk = 0;
  // Values used for RGB colors.
  int fr = 0;
  int fg = 0;
  int fb = 0;
  int br = 0;
  int bg = 0;
  int bb = 0;
  bool isCmykImage = IsCmykImage();
  if (isCmykImage) {
    fc = FXSYS_GetCValue(forecolor);
    fm = FXSYS_GetMValue(forecolor);
    fy = FXSYS_GetYValue(forecolor);
    fk = FXSYS_GetKValue(forecolor);
    bc = FXSYS_GetCValue(backcolor);
    bm = FXSYS_GetMValue(backcolor);
    by = FXSYS_GetYValue(backcolor);
    bk = FXSYS_GetKValue(backcolor);
  } else {
    fr = FXSYS_GetRValue(forecolor);
    fg = FXSYS_GetGValue(forecolor);
    fb = FXSYS_GetBValue(forecolor);
    br = FXSYS_GetRValue(backcolor);
    bg = FXSYS_GetGValue(backcolor);
    bb = FXSYS_GetBValue(backcolor);
  }
  if (m_bpp <= 8) {
    if (isCmykImage) {
      if (forecolor == 0xff && backcolor == 0 && !m_pPalette) {
        return true;
      }
    } else if (forecolor == 0 && backcolor == 0xffffff && !m_pPalette) {
      return true;
    }
    if (!m_pPalette) {
      BuildPalette();
    }
    int size = 1 << m_bpp;
    if (isCmykImage) {
      for (int i = 0; i < size; i++) {
        uint8_t b, g, r;
        AdobeCMYK_to_sRGB1(FXSYS_GetCValue(m_pPalette.get()[i]),
                           FXSYS_GetMValue(m_pPalette.get()[i]),
                           FXSYS_GetYValue(m_pPalette.get()[i]),
                           FXSYS_GetKValue(m_pPalette.get()[i]), r, g, b);
        int gray = 255 - FXRGB2GRAY(r, g, b);
        m_pPalette.get()[i] = CmykEncode(
            bc + (fc - bc) * gray / 255, bm + (fm - bm) * gray / 255,
            by + (fy - by) * gray / 255, bk + (fk - bk) * gray / 255);
      }
    } else {
      for (int i = 0; i < size; i++) {
        int gray = FXRGB2GRAY(FXARGB_R(m_pPalette.get()[i]),
                              FXARGB_G(m_pPalette.get()[i]),
                              FXARGB_B(m_pPalette.get()[i]));
        m_pPalette.get()[i] = FXARGB_MAKE(0xff, br + (fr - br) * gray / 255,
                                          bg + (fg - bg) * gray / 255,
                                          bb + (fb - bb) * gray / 255);
      }
    }
    return true;
  }
  if (isCmykImage) {
    if (forecolor == 0xff && backcolor == 0x00) {
      for (int row = 0; row < m_Height; row++) {
        uint8_t* scanline = m_pBuffer + row * m_Pitch;
        for (int col = 0; col < m_Width; col++) {
          uint8_t b, g, r;
          AdobeCMYK_to_sRGB1(scanline[0], scanline[1], scanline[2], scanline[3],
                             r, g, b);
          *scanline++ = 0;
          *scanline++ = 0;
          *scanline++ = 0;
          *scanline++ = 255 - FXRGB2GRAY(r, g, b);
        }
      }
      return true;
    }
  } else if (forecolor == 0 && backcolor == 0xffffff) {
    for (int row = 0; row < m_Height; row++) {
      uint8_t* scanline = m_pBuffer + row * m_Pitch;
      int gap = m_bpp / 8 - 2;
      for (int col = 0; col < m_Width; col++) {
        int gray = FXRGB2GRAY(scanline[2], scanline[1], scanline[0]);
        *scanline++ = gray;
        *scanline++ = gray;
        *scanline = gray;
        scanline += gap;
      }
    }
    return true;
  }
  if (isCmykImage) {
    for (int row = 0; row < m_Height; row++) {
      uint8_t* scanline = m_pBuffer + row * m_Pitch;
      for (int col = 0; col < m_Width; col++) {
        uint8_t b, g, r;
        AdobeCMYK_to_sRGB1(scanline[0], scanline[1], scanline[2], scanline[3],
                           r, g, b);
        int gray = 255 - FXRGB2GRAY(r, g, b);
        *scanline++ = bc + (fc - bc) * gray / 255;
        *scanline++ = bm + (fm - bm) * gray / 255;
        *scanline++ = by + (fy - by) * gray / 255;
        *scanline++ = bk + (fk - bk) * gray / 255;
      }
    }
  } else {
    for (int row = 0; row < m_Height; row++) {
      uint8_t* scanline = m_pBuffer + row * m_Pitch;
      int gap = m_bpp / 8 - 2;
      for (int col = 0; col < m_Width; col++) {
        int gray = FXRGB2GRAY(scanline[2], scanline[1], scanline[0]);
        *scanline++ = bb + (fb - bb) * gray / 255;
        *scanline++ = bg + (fg - bg) * gray / 255;
        *scanline = br + (fr - br) * gray / 255;
        scanline += gap;
      }
    }
  }
  return true;
}

std::unique_ptr<CFX_DIBitmap> CFX_DIBSource::FlipImage(bool bXFlip,
                                                       bool bYFlip) const {
  auto pFlipped = pdfium::MakeUnique<CFX_DIBitmap>();
  if (!pFlipped->Create(m_Width, m_Height, GetFormat()))
    return nullptr;

  pFlipped->SetPalette(m_pPalette.get());
  uint8_t* pDestBuffer = pFlipped->GetBuffer();
  int Bpp = m_bpp / 8;
  for (int row = 0; row < m_Height; row++) {
    const uint8_t* src_scan = GetScanline(row);
    uint8_t* dest_scan =
        pDestBuffer + m_Pitch * (bYFlip ? (m_Height - row - 1) : row);
    if (!bXFlip) {
      FXSYS_memcpy(dest_scan, src_scan, m_Pitch);
      continue;
    }
    if (m_bpp == 1) {
      FXSYS_memset(dest_scan, 0, m_Pitch);
      for (int col = 0; col < m_Width; col++)
        if (src_scan[col / 8] & (1 << (7 - col % 8))) {
          int dest_col = m_Width - col - 1;
          dest_scan[dest_col / 8] |= (1 << (7 - dest_col % 8));
        }
    } else {
      dest_scan += (m_Width - 1) * Bpp;
      if (Bpp == 1) {
        for (int col = 0; col < m_Width; col++) {
          *dest_scan = *src_scan;
          dest_scan--;
          src_scan++;
        }
      } else if (Bpp == 3) {
        for (int col = 0; col < m_Width; col++) {
          dest_scan[0] = src_scan[0];
          dest_scan[1] = src_scan[1];
          dest_scan[2] = src_scan[2];
          dest_scan -= 3;
          src_scan += 3;
        }
      } else {
        ASSERT(Bpp == 4);
        for (int col = 0; col < m_Width; col++) {
          *(uint32_t*)dest_scan = *(uint32_t*)src_scan;
          dest_scan -= 4;
          src_scan += 4;
        }
      }
    }
  }
  if (m_pAlphaMask) {
    pDestBuffer = pFlipped->m_pAlphaMask->GetBuffer();
    uint32_t dest_pitch = pFlipped->m_pAlphaMask->GetPitch();
    for (int row = 0; row < m_Height; row++) {
      const uint8_t* src_scan = m_pAlphaMask->GetScanline(row);
      uint8_t* dest_scan =
          pDestBuffer + dest_pitch * (bYFlip ? (m_Height - row - 1) : row);
      if (!bXFlip) {
        FXSYS_memcpy(dest_scan, src_scan, dest_pitch);
        continue;
      }
      dest_scan += (m_Width - 1);
      for (int col = 0; col < m_Width; col++) {
        *dest_scan = *src_scan;
        dest_scan--;
        src_scan++;
      }
    }
  }
  return pFlipped;
}

CFX_DIBExtractor::CFX_DIBExtractor(const CFX_DIBSource* pSrc) {
  if (pSrc->GetBuffer()) {
    m_pBitmap = pdfium::MakeUnique<CFX_DIBitmap>();
    if (!m_pBitmap->Create(pSrc->GetWidth(), pSrc->GetHeight(),
                           pSrc->GetFormat(), pSrc->GetBuffer())) {
      m_pBitmap.reset();
      return;
    }
    m_pBitmap->SetPalette(pSrc->GetPalette());
    m_pBitmap->SetAlphaMask(pSrc->m_pAlphaMask);
  } else {
    m_pBitmap = pSrc->Clone();
  }
}

CFX_DIBExtractor::~CFX_DIBExtractor() {}

CFX_FilteredDIB::CFX_FilteredDIB() : m_pSrc(nullptr) {}

CFX_FilteredDIB::~CFX_FilteredDIB() {
  if (m_bAutoDropSrc) {
    delete m_pSrc;
  }
}

void CFX_FilteredDIB::LoadSrc(const CFX_DIBSource* pSrc, bool bAutoDropSrc) {
  m_pSrc = pSrc;
  m_bAutoDropSrc = bAutoDropSrc;
  m_Width = pSrc->GetWidth();
  m_Height = pSrc->GetHeight();
  FXDIB_Format format = GetDestFormat();
  m_bpp = (uint8_t)format;
  m_AlphaFlag = (uint8_t)(format >> 8);
  m_Pitch = (m_Width * (format & 0xff) + 31) / 32 * 4;
  m_pPalette.reset(GetDestPalette());
  m_Scanline.resize(m_Pitch);
}

const uint8_t* CFX_FilteredDIB::GetScanline(int line) const {
  TranslateScanline(m_pSrc->GetScanline(line), &m_Scanline);
  return m_Scanline.data();
}

void CFX_FilteredDIB::DownSampleScanline(int line,
                                         uint8_t* dest_scan,
                                         int dest_bpp,
                                         int dest_width,
                                         bool bFlipX,
                                         int clip_left,
                                         int clip_width) const {
  m_pSrc->DownSampleScanline(line, dest_scan, dest_bpp, dest_width, bFlipX,
                             clip_left, clip_width);
  TranslateDownSamples(dest_scan, dest_scan, clip_width, dest_bpp);
}

CFX_ImageRenderer::CFX_ImageRenderer() {
  m_Status = 0;
  m_pIccTransform = nullptr;
  m_bRgbByteOrder = false;
  m_BlendType = FXDIB_BLEND_NORMAL;
}

CFX_ImageRenderer::~CFX_ImageRenderer() {}

bool CFX_ImageRenderer::Start(CFX_DIBitmap* pDevice,
                              const CFX_ClipRgn* pClipRgn,
                              const CFX_DIBSource* pSource,
                              int bitmap_alpha,
                              uint32_t mask_color,
                              const CFX_Matrix* pMatrix,
                              uint32_t dib_flags,
                              bool bRgbByteOrder,
                              int alpha_flag,
                              void* pIccTransform,
                              int blend_type) {
  m_Matrix = *pMatrix;
  CFX_FloatRect image_rect_f = m_Matrix.GetUnitRect();
  FX_RECT image_rect = image_rect_f.GetOuterRect();
  m_ClipBox = pClipRgn ? pClipRgn->GetBox() : FX_RECT(0, 0, pDevice->GetWidth(),
                                                      pDevice->GetHeight());
  m_ClipBox.Intersect(image_rect);
  if (m_ClipBox.IsEmpty())
    return false;

  m_pDevice = pDevice;
  m_pClipRgn = pClipRgn;
  m_MaskColor = mask_color;
  m_BitmapAlpha = bitmap_alpha;
  m_Matrix = *pMatrix;
  m_Flags = dib_flags;
  m_AlphaFlag = alpha_flag;
  m_pIccTransform = pIccTransform;
  m_bRgbByteOrder = bRgbByteOrder;
  m_BlendType = blend_type;

  if ((FXSYS_fabs(m_Matrix.b) >= 0.5f || m_Matrix.a == 0) ||
      (FXSYS_fabs(m_Matrix.c) >= 0.5f || m_Matrix.d == 0)) {
    if (FXSYS_fabs(m_Matrix.a) < FXSYS_fabs(m_Matrix.b) / 20 &&
        FXSYS_fabs(m_Matrix.d) < FXSYS_fabs(m_Matrix.c) / 20 &&
        FXSYS_fabs(m_Matrix.a) < 0.5f && FXSYS_fabs(m_Matrix.d) < 0.5f) {
      int dest_width = image_rect.Width();
      int dest_height = image_rect.Height();
      FX_RECT bitmap_clip = m_ClipBox;
      bitmap_clip.Offset(-image_rect.left, -image_rect.top);
      bitmap_clip = FXDIB_SwapClipBox(bitmap_clip, dest_width, dest_height,
                                      m_Matrix.c > 0, m_Matrix.b < 0);
      m_Composer.Compose(pDevice, pClipRgn, bitmap_alpha, mask_color, m_ClipBox,
                         true, m_Matrix.c > 0, m_Matrix.b < 0, m_bRgbByteOrder,
                         alpha_flag, pIccTransform, m_BlendType);
      m_Stretcher = pdfium::MakeUnique<CFX_ImageStretcher>(
          &m_Composer, pSource, dest_height, dest_width, bitmap_clip,
          dib_flags);
      if (!m_Stretcher->Start())
        return false;

      m_Status = 1;
      return true;
    }
    m_Status = 2;
    m_pTransformer.reset(
        new CFX_ImageTransformer(pSource, &m_Matrix, dib_flags, &m_ClipBox));
    m_pTransformer->Start();
    return true;
  }

  int dest_width = image_rect.Width();
  if (m_Matrix.a < 0)
    dest_width = -dest_width;

  int dest_height = image_rect.Height();
  if (m_Matrix.d > 0)
    dest_height = -dest_height;

  if (dest_width == 0 || dest_height == 0)
    return false;

  FX_RECT bitmap_clip = m_ClipBox;
  bitmap_clip.Offset(-image_rect.left, -image_rect.top);
  m_Composer.Compose(pDevice, pClipRgn, bitmap_alpha, mask_color, m_ClipBox,
                     false, false, false, m_bRgbByteOrder, alpha_flag,
                     pIccTransform, m_BlendType);
  m_Status = 1;
  m_Stretcher = pdfium::MakeUnique<CFX_ImageStretcher>(
      &m_Composer, pSource, dest_width, dest_height, bitmap_clip, dib_flags);
  return m_Stretcher->Start();
}

bool CFX_ImageRenderer::Continue(IFX_Pause* pPause) {
  if (m_Status == 1)
    return m_Stretcher->Continue(pPause);

  if (m_Status == 2) {
    if (m_pTransformer->Continue(pPause))
      return true;

    std::unique_ptr<CFX_DIBitmap> pBitmap(m_pTransformer->DetachBitmap());
    if (!pBitmap || !pBitmap->GetBuffer())
      return false;

    if (pBitmap->IsAlphaMask()) {
      if (m_BitmapAlpha != 255) {
        if (m_AlphaFlag >> 8) {
          m_AlphaFlag =
              (((uint8_t)((m_AlphaFlag & 0xff) * m_BitmapAlpha / 255)) |
               ((m_AlphaFlag >> 8) << 8));
        } else {
          m_MaskColor = FXARGB_MUL_ALPHA(m_MaskColor, m_BitmapAlpha);
        }
      }
      m_pDevice->CompositeMask(
          m_pTransformer->result().left, m_pTransformer->result().top,
          pBitmap->GetWidth(), pBitmap->GetHeight(), pBitmap.get(), m_MaskColor,
          0, 0, m_BlendType, m_pClipRgn, m_bRgbByteOrder, m_AlphaFlag,
          m_pIccTransform);
    } else {
      if (m_BitmapAlpha != 255)
        pBitmap->MultiplyAlpha(m_BitmapAlpha);
      m_pDevice->CompositeBitmap(
          m_pTransformer->result().left, m_pTransformer->result().top,
          pBitmap->GetWidth(), pBitmap->GetHeight(), pBitmap.get(), 0, 0,
          m_BlendType, m_pClipRgn, m_bRgbByteOrder, m_pIccTransform);
    }
    return false;
  }
  return false;
}

CFX_BitmapStorer::CFX_BitmapStorer() {
}

CFX_BitmapStorer::~CFX_BitmapStorer() {
}

std::unique_ptr<CFX_DIBitmap> CFX_BitmapStorer::Detach() {
  return std::move(m_pBitmap);
}

void CFX_BitmapStorer::Replace(std::unique_ptr<CFX_DIBitmap> pBitmap) {
  m_pBitmap = std::move(pBitmap);
}

void CFX_BitmapStorer::ComposeScanline(int line,
                                       const uint8_t* scanline,
                                       const uint8_t* scan_extra_alpha) {
  uint8_t* dest_buf = const_cast<uint8_t*>(m_pBitmap->GetScanline(line));
  uint8_t* dest_alpha_buf =
      m_pBitmap->m_pAlphaMask
          ? const_cast<uint8_t*>(m_pBitmap->m_pAlphaMask->GetScanline(line))
          : nullptr;
  if (dest_buf)
    FXSYS_memcpy(dest_buf, scanline, m_pBitmap->GetPitch());

  if (dest_alpha_buf) {
    FXSYS_memcpy(dest_alpha_buf, scan_extra_alpha,
                 m_pBitmap->m_pAlphaMask->GetPitch());
  }
}

bool CFX_BitmapStorer::SetInfo(int width,
                               int height,
                               FXDIB_Format src_format,
                               uint32_t* pSrcPalette) {
  m_pBitmap = pdfium::MakeUnique<CFX_DIBitmap>();
  if (!m_pBitmap->Create(width, height, src_format)) {
    m_pBitmap.reset();
    return false;
  }
  if (pSrcPalette)
    m_pBitmap->SetPalette(pSrcPalette);
  return true;
}
