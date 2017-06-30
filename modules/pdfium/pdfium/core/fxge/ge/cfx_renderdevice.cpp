// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_renderdevice.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "core/fxcrt/fx_safe_types.h"
#include "core/fxge/cfx_facecache.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/ifx_renderdevicedriver.h"

#if defined _SKIA_SUPPORT_ || defined _SKIA_SUPPORT_PATHS_
#include "third_party/skia/include/core/SkTypes.h"
#endif

namespace {

void AdjustGlyphSpace(std::vector<FXTEXT_GLYPHPOS>* pGlyphAndPos) {
  ASSERT(pGlyphAndPos->size() > 1);
  std::vector<FXTEXT_GLYPHPOS>& glyphs = *pGlyphAndPos;
  bool bVertical = glyphs.back().m_Origin.x == glyphs.front().m_Origin.x;
  if (!bVertical && (glyphs.back().m_Origin.y != glyphs.front().m_Origin.y))
    return;

  for (size_t i = glyphs.size() - 1; i > 1; --i) {
    FXTEXT_GLYPHPOS& next = glyphs[i];
    int next_origin = bVertical ? next.m_Origin.y : next.m_Origin.x;
    FX_FLOAT next_origin_f = bVertical ? next.m_fOrigin.y : next.m_fOrigin.x;

    FXTEXT_GLYPHPOS& current = glyphs[i - 1];
    int& current_origin = bVertical ? current.m_Origin.y : current.m_Origin.x;
    FX_FLOAT current_origin_f =
        bVertical ? current.m_fOrigin.y : current.m_fOrigin.x;

    int space = next_origin - current_origin;
    FX_FLOAT space_f = next_origin_f - current_origin_f;
    FX_FLOAT error =
        FXSYS_fabs(space_f) - FXSYS_fabs(static_cast<FX_FLOAT>(space));
    if (error > 0.5f)
      current_origin += space > 0 ? -1 : 1;
  }
}

const uint8_t g_TextGammaAdjust[256] = {
    0,   2,   3,   4,   6,   7,   8,   10,  11,  12,  13,  15,  16,  17,  18,
    19,  21,  22,  23,  24,  25,  26,  27,  29,  30,  31,  32,  33,  34,  35,
    36,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  51,  52,
    53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,
    68,  69,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,
    84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,
    99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
    114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128,
    129, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
    143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 156,
    157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171,
    172, 173, 174, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185,
    186, 187, 188, 189, 190, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199,
    200, 201, 202, 203, 204, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
    214, 215, 216, 217, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227,
    228, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 239, 240,
    241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 250, 251, 252, 253, 254,
    255,
};

int TextGammaAdjust(int value) {
  ASSERT(value >= 0);
  ASSERT(value <= 255);
  return g_TextGammaAdjust[value];
}

int CalcAlpha(int src, int alpha) {
  return src * alpha / 255;
}

void Merge(uint8_t src, int channel, int alpha, uint8_t* dest) {
  *dest = FXDIB_ALPHA_MERGE(*dest, channel, CalcAlpha(src, alpha));
}

void MergeGammaAdjust(uint8_t src, int channel, int alpha, uint8_t* dest) {
  *dest =
      FXDIB_ALPHA_MERGE(*dest, channel, CalcAlpha(TextGammaAdjust(src), alpha));
}

void MergeGammaAdjustBgr(const uint8_t* src,
                         int r,
                         int g,
                         int b,
                         int a,
                         uint8_t* dest) {
  MergeGammaAdjust(src[0], b, a, &dest[0]);
  MergeGammaAdjust(src[1], g, a, &dest[1]);
  MergeGammaAdjust(src[2], r, a, &dest[2]);
}

void MergeGammaAdjustRgb(const uint8_t* src,
                         int r,
                         int g,
                         int b,
                         int a,
                         uint8_t* dest) {
  MergeGammaAdjust(src[2], b, a, &dest[0]);
  MergeGammaAdjust(src[1], g, a, &dest[1]);
  MergeGammaAdjust(src[0], r, a, &dest[2]);
}

int AverageRgb(const uint8_t* src) {
  return (src[0] + src[1] + src[2]) / 3;
}

uint8_t CalculateDestAlpha(uint8_t back_alpha, int src_alpha) {
  return back_alpha + src_alpha - back_alpha * src_alpha / 255;
}

void ApplyDestAlpha(uint8_t back_alpha,
                    int src_alpha,
                    int r,
                    int g,
                    int b,
                    uint8_t* dest) {
  uint8_t dest_alpha = CalculateDestAlpha(back_alpha, src_alpha);
  int alpha_ratio = src_alpha * 255 / dest_alpha;
  dest[0] = FXDIB_ALPHA_MERGE(dest[0], b, alpha_ratio);
  dest[1] = FXDIB_ALPHA_MERGE(dest[1], g, alpha_ratio);
  dest[2] = FXDIB_ALPHA_MERGE(dest[2], r, alpha_ratio);
  dest[3] = dest_alpha;
}

void NormalizeRgbDst(int src_value, int r, int g, int b, int a, uint8_t* dest) {
  int src_alpha = CalcAlpha(TextGammaAdjust(src_value), a);
  dest[0] = FXDIB_ALPHA_MERGE(dest[0], b, src_alpha);
  dest[1] = FXDIB_ALPHA_MERGE(dest[1], g, src_alpha);
  dest[2] = FXDIB_ALPHA_MERGE(dest[2], r, src_alpha);
}

void NormalizeRgbSrc(int src_value, int r, int g, int b, int a, uint8_t* dest) {
  int src_alpha = CalcAlpha(TextGammaAdjust(src_value), a);
  if (src_alpha == 0)
    return;

  dest[0] = FXDIB_ALPHA_MERGE(dest[0], b, src_alpha);
  dest[1] = FXDIB_ALPHA_MERGE(dest[1], g, src_alpha);
  dest[2] = FXDIB_ALPHA_MERGE(dest[2], r, src_alpha);
}

void NormalizeArgbDest(int src_value,
                       int r,
                       int g,
                       int b,
                       int a,
                       uint8_t* dest) {
  int src_alpha = CalcAlpha(TextGammaAdjust(src_value), a);
  uint8_t back_alpha = dest[3];
  if (back_alpha == 0) {
    FXARGB_SETDIB(dest, FXARGB_MAKE(src_alpha, r, g, b));
  } else if (src_alpha != 0) {
    ApplyDestAlpha(back_alpha, src_alpha, r, g, b, dest);
  }
}

void NormalizeArgbSrc(int src_value,
                      int r,
                      int g,
                      int b,
                      int a,
                      uint8_t* dest) {
  int src_alpha = CalcAlpha(TextGammaAdjust(src_value), a);
  if (src_alpha == 0)
    return;

  uint8_t back_alpha = dest[3];
  if (back_alpha == 0) {
    FXARGB_SETDIB(dest, FXARGB_MAKE(src_alpha, r, g, b));
  } else {
    ApplyDestAlpha(back_alpha, src_alpha, r, g, b, dest);
  }
}

void NextPixel(uint8_t** src_scan, uint8_t** dst_scan, int bpp) {
  *src_scan += 3;
  *dst_scan += bpp;
}

void SetAlpha(uint8_t* alpha) {
  alpha[3] = 255;
}

void SetAlphaDoNothing(uint8_t* alpha) {}

void DrawNormalTextHelper(CFX_DIBitmap* bitmap,
                          const CFX_DIBitmap* pGlyph,
                          int nrows,
                          int left,
                          int top,
                          int start_col,
                          int end_col,
                          bool bNormal,
                          bool bBGRStripe,
                          int x_subpixel,
                          int a,
                          int r,
                          int g,
                          int b) {
  const bool has_alpha = bitmap->GetFormat() == FXDIB_Argb;
  uint8_t* src_buf = pGlyph->GetBuffer();
  int src_pitch = pGlyph->GetPitch();
  uint8_t* dest_buf = bitmap->GetBuffer();
  int dest_pitch = bitmap->GetPitch();
  const int Bpp = has_alpha ? 4 : bitmap->GetBPP() / 8;
  auto* pNormalizeSrcFunc = has_alpha ? &NormalizeArgbSrc : &NormalizeRgbDst;
  auto* pNormalizeDstFunc = has_alpha ? &NormalizeArgbDest : &NormalizeRgbSrc;
  auto* pSetAlpha = has_alpha ? &SetAlpha : &SetAlphaDoNothing;

  for (int row = 0; row < nrows; row++) {
    int dest_row = row + top;
    if (dest_row < 0 || dest_row >= bitmap->GetHeight())
      continue;

    uint8_t* src_scan = src_buf + row * src_pitch + (start_col - left) * 3;
    uint8_t* dest_scan = dest_buf + dest_row * dest_pitch + start_col * Bpp;
    if (bBGRStripe) {
      if (x_subpixel == 0) {
        for (int col = start_col; col < end_col; col++) {
          if (has_alpha) {
            Merge(src_scan[2], r, a, &dest_scan[2]);
            Merge(src_scan[1], g, a, &dest_scan[1]);
            Merge(src_scan[0], b, a, &dest_scan[0]);
          } else {
            MergeGammaAdjustBgr(&src_scan[0], r, g, b, a, &dest_scan[0]);
          }
          pSetAlpha(dest_scan);
          NextPixel(&src_scan, &dest_scan, Bpp);
        }
      } else if (x_subpixel == 1) {
        MergeGammaAdjust(src_scan[1], r, a, &dest_scan[2]);
        MergeGammaAdjust(src_scan[0], g, a, &dest_scan[1]);
        if (start_col > left)
          MergeGammaAdjust(src_scan[-1], b, a, &dest_scan[0]);
        pSetAlpha(dest_scan);
        NextPixel(&src_scan, &dest_scan, Bpp);
        for (int col = start_col + 1; col < end_col - 1; col++) {
          MergeGammaAdjustBgr(&src_scan[-1], r, g, b, a, &dest_scan[0]);
          pSetAlpha(dest_scan);
          NextPixel(&src_scan, &dest_scan, Bpp);
        }
      } else {
        MergeGammaAdjust(src_scan[0], r, a, &dest_scan[2]);
        if (start_col > left) {
          MergeGammaAdjust(src_scan[-1], g, a, &dest_scan[1]);
          MergeGammaAdjust(src_scan[-2], b, a, &dest_scan[0]);
        }
        pSetAlpha(dest_scan);
        NextPixel(&src_scan, &dest_scan, Bpp);
        for (int col = start_col + 1; col < end_col - 1; col++) {
          MergeGammaAdjustBgr(&src_scan[-2], r, g, b, a, &dest_scan[0]);
          pSetAlpha(dest_scan);
          NextPixel(&src_scan, &dest_scan, Bpp);
        }
      }
    } else {
      if (x_subpixel == 0) {
        for (int col = start_col; col < end_col; col++) {
          if (bNormal) {
            int src_value = AverageRgb(&src_scan[0]);
            pNormalizeDstFunc(src_value, r, g, b, a, dest_scan);
          } else {
            MergeGammaAdjustRgb(&src_scan[0], r, g, b, a, &dest_scan[0]);
            pSetAlpha(dest_scan);
          }
          NextPixel(&src_scan, &dest_scan, Bpp);
        }
      } else if (x_subpixel == 1) {
        if (bNormal) {
          int src_value = start_col > left ? AverageRgb(&src_scan[-1])
                                           : (src_scan[0] + src_scan[1]) / 3;
          pNormalizeSrcFunc(src_value, r, g, b, a, dest_scan);
        } else {
          if (start_col > left)
            MergeGammaAdjust(src_scan[-1], r, a, &dest_scan[2]);
          MergeGammaAdjust(src_scan[0], g, a, &dest_scan[1]);
          MergeGammaAdjust(src_scan[1], b, a, &dest_scan[0]);
          pSetAlpha(dest_scan);
        }
        NextPixel(&src_scan, &dest_scan, Bpp);
        for (int col = start_col + 1; col < end_col; col++) {
          if (bNormal) {
            int src_value = AverageRgb(&src_scan[-1]);
            pNormalizeDstFunc(src_value, r, g, b, a, dest_scan);
          } else {
            MergeGammaAdjustRgb(&src_scan[-1], r, g, b, a, &dest_scan[0]);
            pSetAlpha(dest_scan);
          }
          NextPixel(&src_scan, &dest_scan, Bpp);
        }
      } else {
        if (bNormal) {
          int src_value =
              start_col > left ? AverageRgb(&src_scan[-2]) : src_scan[0] / 3;
          pNormalizeSrcFunc(src_value, r, g, b, a, dest_scan);
        } else {
          if (start_col > left) {
            MergeGammaAdjust(src_scan[-2], r, a, &dest_scan[2]);
            MergeGammaAdjust(src_scan[-1], g, a, &dest_scan[1]);
          }
          MergeGammaAdjust(src_scan[0], b, a, &dest_scan[0]);
          pSetAlpha(dest_scan);
        }
        NextPixel(&src_scan, &dest_scan, Bpp);
        for (int col = start_col + 1; col < end_col; col++) {
          if (bNormal) {
            int src_value = AverageRgb(&src_scan[-2]);
            pNormalizeDstFunc(src_value, r, g, b, a, dest_scan);
          } else {
            MergeGammaAdjustRgb(&src_scan[-2], r, g, b, a, &dest_scan[0]);
            pSetAlpha(dest_scan);
          }
          NextPixel(&src_scan, &dest_scan, Bpp);
        }
      }
    }
  }
}

bool ShouldDrawDeviceText(const CFX_Font* pFont, uint32_t text_flags) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  if (text_flags & FXFONT_CIDFONT)
    return false;

  const CFX_ByteString bsPsName = pFont->GetPsName();
  if (bsPsName.Find("+ZJHL") != -1)
    return false;

  if (bsPsName == "CNAAJI+cmex10")
    return false;
#endif
  return true;
}

}  // namespace

FXTEXT_CHARPOS::FXTEXT_CHARPOS()
    : m_GlyphIndex(0),
      m_FontCharWidth(0),
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      m_ExtGID(0),
#endif
      m_FallbackFontPosition(0),
      m_bGlyphAdjust(false),
      m_bFontStyle(false) {
}

FXTEXT_CHARPOS::FXTEXT_CHARPOS(const FXTEXT_CHARPOS&) = default;

FXTEXT_CHARPOS::~FXTEXT_CHARPOS(){};

CFX_RenderDevice::CFX_RenderDevice()
    : m_pBitmap(nullptr),
      m_Width(0),
      m_Height(0),
      m_bpp(0),
      m_RenderCaps(0),
      m_DeviceClass(0) {}

CFX_RenderDevice::~CFX_RenderDevice() {
#if defined _SKIA_SUPPORT_ || defined _SKIA_SUPPORT_PATHS_
  Flush();
#endif
}

#if defined _SKIA_SUPPORT_ || defined _SKIA_SUPPORT_PATHS_
void CFX_RenderDevice::Flush() {
  m_pDeviceDriver.reset();
}
#endif

void CFX_RenderDevice::SetDeviceDriver(
    std::unique_ptr<IFX_RenderDeviceDriver> pDriver) {
  m_pDeviceDriver = std::move(pDriver);
  InitDeviceInfo();
}

void CFX_RenderDevice::InitDeviceInfo() {
  m_Width = m_pDeviceDriver->GetDeviceCaps(FXDC_PIXEL_WIDTH);
  m_Height = m_pDeviceDriver->GetDeviceCaps(FXDC_PIXEL_HEIGHT);
  m_bpp = m_pDeviceDriver->GetDeviceCaps(FXDC_BITS_PIXEL);
  m_RenderCaps = m_pDeviceDriver->GetDeviceCaps(FXDC_RENDER_CAPS);
  m_DeviceClass = m_pDeviceDriver->GetDeviceCaps(FXDC_DEVICE_CLASS);
  if (!m_pDeviceDriver->GetClipBox(&m_ClipBox)) {
    m_ClipBox.left = 0;
    m_ClipBox.top = 0;
    m_ClipBox.right = m_Width;
    m_ClipBox.bottom = m_Height;
  }
}

void CFX_RenderDevice::SaveState() {
  m_pDeviceDriver->SaveState();
}

void CFX_RenderDevice::RestoreState(bool bKeepSaved) {
  m_pDeviceDriver->RestoreState(bKeepSaved);
  UpdateClipBox();
}

int CFX_RenderDevice::GetDeviceCaps(int caps_id) const {
  return m_pDeviceDriver->GetDeviceCaps(caps_id);
}
CFX_Matrix CFX_RenderDevice::GetCTM() const {
  return m_pDeviceDriver->GetCTM();
}

bool CFX_RenderDevice::CreateCompatibleBitmap(CFX_DIBitmap* pDIB,
                                              int width,
                                              int height) const {
  if (m_RenderCaps & FXRC_CMYK_OUTPUT) {
    return pDIB->Create(width, height, m_RenderCaps & FXRC_ALPHA_OUTPUT
                                           ? FXDIB_Cmyka
                                           : FXDIB_Cmyk);
  }
  if (m_RenderCaps & FXRC_BYTEMASK_OUTPUT)
    return pDIB->Create(width, height, FXDIB_8bppMask);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_ || defined _SKIA_SUPPORT_PATHS_
  return pDIB->Create(width, height, m_RenderCaps & FXRC_ALPHA_OUTPUT
                                         ? FXDIB_Argb
                                         : FXDIB_Rgb32);
#else
  return pDIB->Create(
      width, height, m_RenderCaps & FXRC_ALPHA_OUTPUT ? FXDIB_Argb : FXDIB_Rgb);
#endif
}

bool CFX_RenderDevice::SetClip_PathFill(const CFX_PathData* pPathData,
                                        const CFX_Matrix* pObject2Device,
                                        int fill_mode) {
  if (!m_pDeviceDriver->SetClip_PathFill(pPathData, pObject2Device,
                                         fill_mode)) {
    return false;
  }
  UpdateClipBox();
  return true;
}

bool CFX_RenderDevice::SetClip_PathStroke(
    const CFX_PathData* pPathData,
    const CFX_Matrix* pObject2Device,
    const CFX_GraphStateData* pGraphState) {
  if (!m_pDeviceDriver->SetClip_PathStroke(pPathData, pObject2Device,
                                           pGraphState)) {
    return false;
  }
  UpdateClipBox();
  return true;
}

bool CFX_RenderDevice::SetClip_Rect(const FX_RECT& rect) {
  CFX_PathData path;
  path.AppendRect(rect.left, rect.bottom, rect.right, rect.top);
  if (!SetClip_PathFill(&path, nullptr, FXFILL_WINDING))
    return false;

  UpdateClipBox();
  return true;
}

void CFX_RenderDevice::UpdateClipBox() {
  if (m_pDeviceDriver->GetClipBox(&m_ClipBox))
    return;
  m_ClipBox.left = 0;
  m_ClipBox.top = 0;
  m_ClipBox.right = m_Width;
  m_ClipBox.bottom = m_Height;
}

bool CFX_RenderDevice::DrawPathWithBlend(const CFX_PathData* pPathData,
                                         const CFX_Matrix* pObject2Device,
                                         const CFX_GraphStateData* pGraphState,
                                         uint32_t fill_color,
                                         uint32_t stroke_color,
                                         int fill_mode,
                                         int blend_type) {
  uint8_t stroke_alpha = pGraphState ? FXARGB_A(stroke_color) : 0;
  uint8_t fill_alpha = (fill_mode & 3) ? FXARGB_A(fill_color) : 0;
  const std::vector<FX_PATHPOINT>& pPoints = pPathData->GetPoints();
  if (stroke_alpha == 0 && pPoints.size() == 2) {
    CFX_PointF pos1 = pPoints[0].m_Point;
    CFX_PointF pos2 = pPoints[1].m_Point;
    if (pObject2Device) {
      pos1 = pObject2Device->Transform(pos1);
      pos2 = pObject2Device->Transform(pos2);
    }
    DrawCosmeticLine(pos1.x, pos1.y, pos2.x, pos2.y, fill_color, fill_mode,
                     blend_type);
    return true;
  }

  if ((pPoints.size() == 5 || pPoints.size() == 4) && stroke_alpha == 0) {
    CFX_FloatRect rect_f;
    if (!(fill_mode & FXFILL_RECT_AA) &&
        pPathData->IsRect(pObject2Device, &rect_f)) {
      FX_RECT rect_i = rect_f.GetOuterRect();

      // Depending on the top/bottom, left/right values of the rect it's
      // possible to overflow the Width() and Height() calculations. Check that
      // the rect will have valid dimension before continuing.
      if (!rect_i.Valid())
        return false;

      int width = (int)FXSYS_ceil(rect_f.right - rect_f.left);
      if (width < 1) {
        width = 1;
        if (rect_i.left == rect_i.right)
          rect_i.right++;
      }
      int height = (int)FXSYS_ceil(rect_f.top - rect_f.bottom);
      if (height < 1) {
        height = 1;
        if (rect_i.bottom == rect_i.top)
          rect_i.bottom++;
      }
      if (rect_i.Width() >= width + 1) {
        if (rect_f.left - (FX_FLOAT)(rect_i.left) >
            (FX_FLOAT)(rect_i.right) - rect_f.right) {
          rect_i.left++;
        } else {
          rect_i.right--;
        }
      }
      if (rect_i.Height() >= height + 1) {
        if (rect_f.top - (FX_FLOAT)(rect_i.top) >
            (FX_FLOAT)(rect_i.bottom) - rect_f.bottom) {
          rect_i.top++;
        } else {
          rect_i.bottom--;
        }
      }
      if (FillRectWithBlend(&rect_i, fill_color, blend_type))
        return true;
    }
  }
  if ((fill_mode & 3) && stroke_alpha == 0 && !(fill_mode & FX_FILL_STROKE) &&
      !(fill_mode & FX_FILL_TEXT_MODE)) {
    CFX_PathData newPath;
    bool bThin = false;
    bool setIdentity = false;
    if (pPathData->GetZeroAreaPath(pObject2Device,
                                   !!m_pDeviceDriver->GetDriverType(), &newPath,
                                   &bThin, &setIdentity)) {
      CFX_GraphStateData graphState;
      graphState.m_LineWidth = 0.0f;

      uint32_t strokecolor = fill_color;
      if (bThin)
        strokecolor = (((fill_alpha >> 2) << 24) | (strokecolor & 0x00ffffff));

      const CFX_Matrix* pMatrix = nullptr;
      if (pObject2Device && !pObject2Device->IsIdentity() && !setIdentity)
        pMatrix = pObject2Device;

      int smooth_path = FX_ZEROAREA_FILL;
      if (fill_mode & FXFILL_NOPATHSMOOTH)
        smooth_path |= FXFILL_NOPATHSMOOTH;

      m_pDeviceDriver->DrawPath(&newPath, pMatrix, &graphState, 0, strokecolor,
                                smooth_path, blend_type);
    }
  }
  if ((fill_mode & 3) && fill_alpha && stroke_alpha < 0xff &&
      (fill_mode & FX_FILL_STROKE)) {
    if (m_RenderCaps & FXRC_FILLSTROKE_PATH) {
      return m_pDeviceDriver->DrawPath(pPathData, pObject2Device, pGraphState,
                                       fill_color, stroke_color, fill_mode,
                                       blend_type);
    }
    return DrawFillStrokePath(pPathData, pObject2Device, pGraphState,
                              fill_color, stroke_color, fill_mode, blend_type);
  }
  return m_pDeviceDriver->DrawPath(pPathData, pObject2Device, pGraphState,
                                   fill_color, stroke_color, fill_mode,
                                   blend_type);
}

// This can be removed once PDFium entirely relies on Skia
bool CFX_RenderDevice::DrawFillStrokePath(const CFX_PathData* pPathData,
                                          const CFX_Matrix* pObject2Device,
                                          const CFX_GraphStateData* pGraphState,
                                          uint32_t fill_color,
                                          uint32_t stroke_color,
                                          int fill_mode,
                                          int blend_type) {
  if (!(m_RenderCaps & FXRC_GET_BITS))
    return false;
  CFX_FloatRect bbox;
  if (pGraphState) {
    bbox = pPathData->GetBoundingBox(pGraphState->m_LineWidth,
                                     pGraphState->m_MiterLimit);
  } else {
    bbox = pPathData->GetBoundingBox();
  }
  if (pObject2Device)
    pObject2Device->TransformRect(bbox);

  CFX_Matrix ctm = GetCTM();
  FX_FLOAT fScaleX = FXSYS_fabs(ctm.a);
  FX_FLOAT fScaleY = FXSYS_fabs(ctm.d);
  FX_RECT rect = bbox.GetOuterRect();
  CFX_DIBitmap bitmap, Backdrop;
  if (!CreateCompatibleBitmap(&bitmap, FXSYS_round(rect.Width() * fScaleX),
                              FXSYS_round(rect.Height() * fScaleY))) {
    return false;
  }
  if (bitmap.HasAlpha()) {
    bitmap.Clear(0);
    Backdrop.Copy(&bitmap);
  } else {
    if (!m_pDeviceDriver->GetDIBits(&bitmap, rect.left, rect.top))
      return false;
    Backdrop.Copy(&bitmap);
  }
  CFX_FxgeDevice bitmap_device;
  bitmap_device.Attach(&bitmap, false, &Backdrop, true);
  CFX_Matrix matrix;
  if (pObject2Device)
    matrix = *pObject2Device;
  matrix.Translate(-rect.left, -rect.top);
  matrix.Concat(CFX_Matrix(fScaleX, 0, 0, fScaleY, 0, 0));
  if (!bitmap_device.GetDeviceDriver()->DrawPath(
          pPathData, &matrix, pGraphState, fill_color, stroke_color, fill_mode,
          blend_type)) {
    return false;
  }
#if defined _SKIA_SUPPORT_ || defined _SKIA_SUPPORT_PATHS_
  bitmap_device.GetDeviceDriver()->Flush();
#endif
  FX_RECT src_rect(0, 0, FXSYS_round(rect.Width() * fScaleX),
                   FXSYS_round(rect.Height() * fScaleY));
  return m_pDeviceDriver->SetDIBits(&bitmap, 0, &src_rect, rect.left, rect.top,
                                    FXDIB_BLEND_NORMAL);
}

bool CFX_RenderDevice::SetPixel(int x, int y, uint32_t color) {
  if (m_pDeviceDriver->SetPixel(x, y, color))
    return true;

  FX_RECT rect(x, y, x + 1, y + 1);
  return FillRectWithBlend(&rect, color, FXDIB_BLEND_NORMAL);
}

bool CFX_RenderDevice::FillRectWithBlend(const FX_RECT* pRect,
                                         uint32_t fill_color,
                                         int blend_type) {
  if (m_pDeviceDriver->FillRectWithBlend(pRect, fill_color, blend_type))
    return true;

  if (!(m_RenderCaps & FXRC_GET_BITS))
    return false;

  CFX_DIBitmap bitmap;
  if (!CreateCompatibleBitmap(&bitmap, pRect->Width(), pRect->Height()))
    return false;

  if (!m_pDeviceDriver->GetDIBits(&bitmap, pRect->left, pRect->top))
    return false;

  if (!bitmap.CompositeRect(0, 0, pRect->Width(), pRect->Height(), fill_color,
                            0, nullptr)) {
    return false;
  }
  FX_RECT src_rect(0, 0, pRect->Width(), pRect->Height());
  m_pDeviceDriver->SetDIBits(&bitmap, 0, &src_rect, pRect->left, pRect->top,
                             FXDIB_BLEND_NORMAL);
  return true;
}

bool CFX_RenderDevice::DrawCosmeticLine(FX_FLOAT x1,
                                        FX_FLOAT y1,
                                        FX_FLOAT x2,
                                        FX_FLOAT y2,
                                        uint32_t color,
                                        int fill_mode,
                                        int blend_type) {
  if ((color >= 0xff000000) &&
      m_pDeviceDriver->DrawCosmeticLine(x1, y1, x2, y2, color, blend_type)) {
    return true;
  }
  CFX_GraphStateData graph_state;
  CFX_PathData path;
  path.AppendPoint(CFX_PointF(x1, y1), FXPT_TYPE::MoveTo, false);
  path.AppendPoint(CFX_PointF(x2, y2), FXPT_TYPE::LineTo, false);
  return m_pDeviceDriver->DrawPath(&path, nullptr, &graph_state, 0, color,
                                   fill_mode, blend_type);
}

bool CFX_RenderDevice::GetDIBits(CFX_DIBitmap* pBitmap, int left, int top) {
  if (!(m_RenderCaps & FXRC_GET_BITS))
    return false;
  return m_pDeviceDriver->GetDIBits(pBitmap, left, top);
}

CFX_DIBitmap* CFX_RenderDevice::GetBackDrop() {
  return m_pDeviceDriver->GetBackDrop();
}

bool CFX_RenderDevice::SetDIBitsWithBlend(const CFX_DIBSource* pBitmap,
                                          int left,
                                          int top,
                                          int blend_mode) {
  ASSERT(!pBitmap->IsAlphaMask());
  CFX_Matrix ctm = GetCTM();
  FX_FLOAT fScaleX = FXSYS_fabs(ctm.a);
  FX_FLOAT fScaleY = FXSYS_fabs(ctm.d);
  FX_RECT dest_rect(left, top,
                    FXSYS_round(left + pBitmap->GetWidth() / fScaleX),
                    FXSYS_round(top + pBitmap->GetHeight() / fScaleY));
  dest_rect.Intersect(m_ClipBox);
  if (dest_rect.IsEmpty())
    return true;
  FX_RECT src_rect(dest_rect.left - left, dest_rect.top - top,
                   dest_rect.left - left + dest_rect.Width(),
                   dest_rect.top - top + dest_rect.Height());
  src_rect.left = FXSYS_round(src_rect.left * fScaleX);
  src_rect.top = FXSYS_round(src_rect.top * fScaleY);
  src_rect.right = FXSYS_round(src_rect.right * fScaleX);
  src_rect.bottom = FXSYS_round(src_rect.bottom * fScaleY);
  if ((blend_mode != FXDIB_BLEND_NORMAL && !(m_RenderCaps & FXRC_BLEND_MODE)) ||
      (pBitmap->HasAlpha() && !(m_RenderCaps & FXRC_ALPHA_IMAGE))) {
    if (!(m_RenderCaps & FXRC_GET_BITS))
      return false;
    int bg_pixel_width = FXSYS_round(dest_rect.Width() * fScaleX);
    int bg_pixel_height = FXSYS_round(dest_rect.Height() * fScaleY);
    CFX_DIBitmap background;
    if (!background.Create(
            bg_pixel_width, bg_pixel_height,
            (m_RenderCaps & FXRC_CMYK_OUTPUT) ? FXDIB_Cmyk : FXDIB_Rgb32)) {
      return false;
    }
    if (!m_pDeviceDriver->GetDIBits(&background, dest_rect.left,
                                    dest_rect.top)) {
      return false;
    }
    if (!background.CompositeBitmap(0, 0, bg_pixel_width, bg_pixel_height,
                                    pBitmap, src_rect.left, src_rect.top,
                                    blend_mode, nullptr, false, nullptr)) {
      return false;
    }
    FX_RECT rect(0, 0, bg_pixel_width, bg_pixel_height);
    return m_pDeviceDriver->SetDIBits(&background, 0, &rect, dest_rect.left,
                                      dest_rect.top, FXDIB_BLEND_NORMAL);
  }
  return m_pDeviceDriver->SetDIBits(pBitmap, 0, &src_rect, dest_rect.left,
                                    dest_rect.top, blend_mode);
}

bool CFX_RenderDevice::StretchDIBitsWithFlagsAndBlend(
    const CFX_DIBSource* pBitmap,
    int left,
    int top,
    int dest_width,
    int dest_height,
    uint32_t flags,
    int blend_mode) {
  FX_RECT dest_rect(left, top, left + dest_width, top + dest_height);
  FX_RECT clip_box = m_ClipBox;
  clip_box.Intersect(dest_rect);
  if (clip_box.IsEmpty())
    return true;
  return m_pDeviceDriver->StretchDIBits(pBitmap, 0, left, top, dest_width,
                                        dest_height, &clip_box, flags,
                                        blend_mode);
}

bool CFX_RenderDevice::SetBitMask(const CFX_DIBSource* pBitmap,
                                  int left,
                                  int top,
                                  uint32_t argb) {
  FX_RECT src_rect(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight());
  return m_pDeviceDriver->SetDIBits(pBitmap, argb, &src_rect, left, top,
                                    FXDIB_BLEND_NORMAL);
}

bool CFX_RenderDevice::StretchBitMask(const CFX_DIBSource* pBitmap,
                                      int left,
                                      int top,
                                      int dest_width,
                                      int dest_height,
                                      uint32_t color) {
  return StretchBitMaskWithFlags(pBitmap, left, top, dest_width, dest_height,
                                 color, 0);
}

bool CFX_RenderDevice::StretchBitMaskWithFlags(const CFX_DIBSource* pBitmap,
                                               int left,
                                               int top,
                                               int dest_width,
                                               int dest_height,
                                               uint32_t argb,
                                               uint32_t flags) {
  FX_RECT dest_rect(left, top, left + dest_width, top + dest_height);
  FX_RECT clip_box = m_ClipBox;
  clip_box.Intersect(dest_rect);
  return m_pDeviceDriver->StretchDIBits(pBitmap, argb, left, top, dest_width,
                                        dest_height, &clip_box, flags,
                                        FXDIB_BLEND_NORMAL);
}

bool CFX_RenderDevice::StartDIBitsWithBlend(const CFX_DIBSource* pBitmap,
                                            int bitmap_alpha,
                                            uint32_t argb,
                                            const CFX_Matrix* pMatrix,
                                            uint32_t flags,
                                            void*& handle,
                                            int blend_mode) {
  return m_pDeviceDriver->StartDIBits(pBitmap, bitmap_alpha, argb, pMatrix,
                                      flags, handle, blend_mode);
}

bool CFX_RenderDevice::ContinueDIBits(void* handle, IFX_Pause* pPause) {
  return m_pDeviceDriver->ContinueDIBits(handle, pPause);
}

void CFX_RenderDevice::CancelDIBits(void* handle) {
  m_pDeviceDriver->CancelDIBits(handle);
}

#ifdef _SKIA_SUPPORT_
void CFX_RenderDevice::DebugVerifyBitmapIsPreMultiplied() const {
  SkASSERT(0);
}

bool CFX_RenderDevice::SetBitsWithMask(const CFX_DIBSource* pBitmap,
                                       const CFX_DIBSource* pMask,
                                       int left,
                                       int top,
                                       int bitmap_alpha,
                                       int blend_type) {
  return m_pDeviceDriver->SetBitsWithMask(pBitmap, pMask, left, top,
                                          bitmap_alpha, blend_type);
}
#endif

bool CFX_RenderDevice::DrawNormalText(int nChars,
                                      const FXTEXT_CHARPOS* pCharPos,
                                      CFX_Font* pFont,
                                      FX_FLOAT font_size,
                                      const CFX_Matrix* pText2Device,
                                      uint32_t fill_color,
                                      uint32_t text_flags) {
  int nativetext_flags = text_flags;
  if (m_DeviceClass != FXDC_DISPLAY) {
    if (!(text_flags & FXTEXT_PRINTGRAPHICTEXT)) {
      if (ShouldDrawDeviceText(pFont, text_flags) &&
          m_pDeviceDriver->DrawDeviceText(nChars, pCharPos, pFont, pText2Device,
                                          font_size, fill_color)) {
        return true;
      }
    }
    if (FXARGB_A(fill_color) < 255)
      return false;
  } else if (!(text_flags & FXTEXT_NO_NATIVETEXT)) {
    if (ShouldDrawDeviceText(pFont, text_flags) &&
        m_pDeviceDriver->DrawDeviceText(nChars, pCharPos, pFont, pText2Device,
                                        font_size, fill_color)) {
      return true;
    }
  }
  CFX_Matrix char2device;
  CFX_Matrix text2Device;
  if (pText2Device) {
    char2device = *pText2Device;
    text2Device = *pText2Device;
  }

  char2device.Scale(font_size, -font_size);
  if (FXSYS_fabs(char2device.a) + FXSYS_fabs(char2device.b) > 50 * 1.0f ||
      ((m_DeviceClass == FXDC_PRINTER) &&
       !(text_flags & FXTEXT_PRINTIMAGETEXT))) {
    if (pFont->GetFace()) {
      int nPathFlags =
          (text_flags & FXTEXT_NOSMOOTH) == 0 ? 0 : FXFILL_NOPATHSMOOTH;
      return DrawTextPath(nChars, pCharPos, pFont, font_size, pText2Device,
                          nullptr, nullptr, fill_color, 0, nullptr, nPathFlags);
    }
  }
  int anti_alias = FXFT_RENDER_MODE_MONO;
  bool bNormal = false;
  if ((text_flags & FXTEXT_NOSMOOTH) == 0) {
    if (m_DeviceClass == FXDC_DISPLAY && m_bpp > 1) {
      if (!CFX_GEModule::Get()->GetFontMgr()->FTLibrarySupportsHinting()) {
        // Some Freetype implementations (like the one packaged with Fedora) do
        // not support hinting due to patents 6219025, 6239783, 6307566,
        // 6225973, 6243070, 6393145, 6421054, 6282327, and 6624828; the latest
        // one expires 10/7/19.  This makes LCD antialiasing very ugly, so we
        // instead fall back on NORMAL antialiasing.
        anti_alias = FXFT_RENDER_MODE_NORMAL;
      } else if ((m_RenderCaps & (FXRC_ALPHA_OUTPUT | FXRC_CMYK_OUTPUT))) {
        anti_alias = FXFT_RENDER_MODE_LCD;
        bNormal = true;
      } else if (m_bpp < 16) {
        anti_alias = FXFT_RENDER_MODE_NORMAL;
      } else {
        anti_alias = FXFT_RENDER_MODE_LCD;

        bool bClearType = false;
        if (pFont->GetFace())
          bClearType = !!(text_flags & FXTEXT_CLEARTYPE);
        bNormal = !bClearType;
      }
    }
  }
  std::vector<FXTEXT_GLYPHPOS> glyphs(nChars);
  CFX_Matrix matrixCTM = GetCTM();
  FX_FLOAT scale_x = FXSYS_fabs(matrixCTM.a);
  FX_FLOAT scale_y = FXSYS_fabs(matrixCTM.d);
  CFX_Matrix deviceCtm = char2device;
  CFX_Matrix m(scale_x, 0, 0, scale_y, 0, 0);
  deviceCtm.Concat(m);
  text2Device.Concat(m);

  for (size_t i = 0; i < glyphs.size(); ++i) {
    FXTEXT_GLYPHPOS& glyph = glyphs[i];
    const FXTEXT_CHARPOS& charpos = pCharPos[i];

    glyph.m_fOrigin = text2Device.Transform(charpos.m_Origin);
    if (anti_alias < FXFT_RENDER_MODE_LCD)
      glyph.m_Origin.x = FXSYS_round(glyph.m_fOrigin.x);
    else
      glyph.m_Origin.x = static_cast<int>(FXSYS_floor(glyph.m_fOrigin.x));
    glyph.m_Origin.y = FXSYS_round(glyph.m_fOrigin.y);

    if (charpos.m_bGlyphAdjust) {
      CFX_Matrix new_matrix(
          charpos.m_AdjustMatrix[0], charpos.m_AdjustMatrix[1],
          charpos.m_AdjustMatrix[2], charpos.m_AdjustMatrix[3], 0, 0);
      new_matrix.Concat(deviceCtm);
      glyph.m_pGlyph = pFont->LoadGlyphBitmap(
          charpos.m_GlyphIndex, charpos.m_bFontStyle, &new_matrix,
          charpos.m_FontCharWidth, anti_alias, nativetext_flags);
    } else {
      glyph.m_pGlyph = pFont->LoadGlyphBitmap(
          charpos.m_GlyphIndex, charpos.m_bFontStyle, &deviceCtm,
          charpos.m_FontCharWidth, anti_alias, nativetext_flags);
    }
  }
  if (anti_alias < FXFT_RENDER_MODE_LCD && glyphs.size() > 1)
    AdjustGlyphSpace(&glyphs);

  FX_RECT bmp_rect1 = FXGE_GetGlyphsBBox(glyphs, anti_alias);
  if (scale_x > 1 && scale_y > 1) {
    bmp_rect1.left--;
    bmp_rect1.top--;
    bmp_rect1.right++;
    bmp_rect1.bottom++;
  }
  FX_RECT bmp_rect(FXSYS_round((FX_FLOAT)(bmp_rect1.left) / scale_x),
                   FXSYS_round((FX_FLOAT)(bmp_rect1.top) / scale_y),
                   FXSYS_round((FX_FLOAT)bmp_rect1.right / scale_x),
                   FXSYS_round((FX_FLOAT)bmp_rect1.bottom / scale_y));
  bmp_rect.Intersect(m_ClipBox);
  if (bmp_rect.IsEmpty())
    return true;
  int pixel_width = FXSYS_round(bmp_rect.Width() * scale_x);
  int pixel_height = FXSYS_round(bmp_rect.Height() * scale_y);
  int pixel_left = FXSYS_round(bmp_rect.left * scale_x);
  int pixel_top = FXSYS_round(bmp_rect.top * scale_y);
  if (anti_alias == FXFT_RENDER_MODE_MONO) {
    CFX_DIBitmap bitmap;
    if (!bitmap.Create(pixel_width, pixel_height, FXDIB_1bppMask))
      return false;
    bitmap.Clear(0);
    for (const FXTEXT_GLYPHPOS& glyph : glyphs) {
      if (!glyph.m_pGlyph)
        continue;
      const CFX_DIBitmap* pGlyph = &glyph.m_pGlyph->m_Bitmap;
      bitmap.TransferBitmap(
          glyph.m_Origin.x + glyph.m_pGlyph->m_Left - pixel_left,
          glyph.m_Origin.y - glyph.m_pGlyph->m_Top - pixel_top,
          pGlyph->GetWidth(), pGlyph->GetHeight(), pGlyph, 0, 0);
    }
    return SetBitMask(&bitmap, bmp_rect.left, bmp_rect.top, fill_color);
  }
  CFX_DIBitmap bitmap;
  if (m_bpp == 8) {
    if (!bitmap.Create(pixel_width, pixel_height, FXDIB_8bppMask))
      return false;
  } else {
    if (!CreateCompatibleBitmap(&bitmap, pixel_width, pixel_height))
      return false;
  }
  if (!bitmap.HasAlpha() && !bitmap.IsAlphaMask()) {
    bitmap.Clear(0xFFFFFFFF);
    if (!GetDIBits(&bitmap, bmp_rect.left, bmp_rect.top))
      return false;
  } else {
    bitmap.Clear(0);
    if (bitmap.m_pAlphaMask)
      bitmap.m_pAlphaMask->Clear(0);
  }
  int dest_width = pixel_width;
  int a = 0;
  int r = 0;
  int g = 0;
  int b = 0;
  if (anti_alias == FXFT_RENDER_MODE_LCD)
    ArgbDecode(fill_color, a, r, g, b);

  for (const FXTEXT_GLYPHPOS& glyph : glyphs) {
    if (!glyph.m_pGlyph)
      continue;

    pdfium::base::CheckedNumeric<int> left = glyph.m_Origin.x;
    left += glyph.m_pGlyph->m_Left;
    left -= pixel_left;
    if (!left.IsValid())
      return false;

    pdfium::base::CheckedNumeric<int> top = glyph.m_Origin.y;
    top -= glyph.m_pGlyph->m_Top;
    top -= pixel_top;
    if (!top.IsValid())
      return false;

    const CFX_DIBitmap* pGlyph = &glyph.m_pGlyph->m_Bitmap;
    int ncols = pGlyph->GetWidth();
    int nrows = pGlyph->GetHeight();
    if (anti_alias == FXFT_RENDER_MODE_NORMAL) {
      if (!bitmap.CompositeMask(left.ValueOrDie(), top.ValueOrDie(), ncols,
                                nrows, pGlyph, fill_color, 0, 0,
                                FXDIB_BLEND_NORMAL, nullptr, false, 0,
                                nullptr)) {
        return false;
      }
      continue;
    }
    bool bBGRStripe = !!(text_flags & FXTEXT_BGR_STRIPE);
    ncols /= 3;
    int x_subpixel = static_cast<int>(glyph.m_fOrigin.x * 3) % 3;
    int start_col =
        pdfium::base::ValueOrDieForType<int>(pdfium::base::CheckMax(left, 0));
    pdfium::base::CheckedNumeric<int> end_col_safe = left;
    end_col_safe += ncols;
    if (!end_col_safe.IsValid())
      return false;

    int end_col =
        std::min(static_cast<int>(end_col_safe.ValueOrDie<int>()), dest_width);
    if (start_col >= end_col)
      continue;

    DrawNormalTextHelper(&bitmap, pGlyph, nrows, left.ValueOrDie(),
                         top.ValueOrDie(), start_col, end_col, bNormal,
                         bBGRStripe, x_subpixel, a, r, g, b);
  }
  if (bitmap.IsAlphaMask())
    SetBitMask(&bitmap, bmp_rect.left, bmp_rect.top, fill_color);
  else
    SetDIBits(&bitmap, bmp_rect.left, bmp_rect.top);
  return true;
}

bool CFX_RenderDevice::DrawTextPath(int nChars,
                                    const FXTEXT_CHARPOS* pCharPos,
                                    CFX_Font* pFont,
                                    FX_FLOAT font_size,
                                    const CFX_Matrix* pText2User,
                                    const CFX_Matrix* pUser2Device,
                                    const CFX_GraphStateData* pGraphState,
                                    uint32_t fill_color,
                                    FX_ARGB stroke_color,
                                    CFX_PathData* pClippingPath,
                                    int nFlag) {
  for (int iChar = 0; iChar < nChars; iChar++) {
    const FXTEXT_CHARPOS& charpos = pCharPos[iChar];
    CFX_Matrix matrix;
    if (charpos.m_bGlyphAdjust) {
      matrix = CFX_Matrix(charpos.m_AdjustMatrix[0], charpos.m_AdjustMatrix[1],
                          charpos.m_AdjustMatrix[2], charpos.m_AdjustMatrix[3],
                          0, 0);
    }
    matrix.Concat(CFX_Matrix(font_size, 0, 0, font_size, charpos.m_Origin.x,
                             charpos.m_Origin.y));
    const CFX_PathData* pPath =
        pFont->LoadGlyphPath(charpos.m_GlyphIndex, charpos.m_FontCharWidth);
    if (!pPath)
      continue;

    matrix.Concat(*pText2User);

    CFX_PathData TransformedPath(*pPath);
    TransformedPath.Transform(&matrix);
    if (fill_color || stroke_color) {
      int fill_mode = nFlag;
      if (fill_color)
        fill_mode |= FXFILL_WINDING;
      fill_mode |= FX_FILL_TEXT_MODE;
      if (!DrawPathWithBlend(&TransformedPath, pUser2Device, pGraphState,
                             fill_color, stroke_color, fill_mode,
                             FXDIB_BLEND_NORMAL)) {
        return false;
      }
    }
    if (pClippingPath)
      pClippingPath->Append(&TransformedPath, pUser2Device);
  }
  return true;
}
