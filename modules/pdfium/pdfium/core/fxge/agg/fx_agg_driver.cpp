// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/agg/fx_agg_driver.h"

#include <algorithm>
#include <utility>

#include "core/fxcodec/fx_codec.h"
#include "core/fxcrt/fx_memory.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/dib/dib_int.h"
#include "core/fxge/ge/cfx_cliprgn.h"
#include "core/fxge/ge/fx_text_int.h"
#include "core/fxge/ifx_renderdevicedriver.h"
#include "third_party/agg23/agg_conv_dash.h"
#include "third_party/agg23/agg_conv_stroke.h"
#include "third_party/agg23/agg_curves.h"
#include "third_party/agg23/agg_path_storage.h"
#include "third_party/agg23/agg_pixfmt_gray.h"
#include "third_party/agg23/agg_rasterizer_scanline_aa.h"
#include "third_party/agg23/agg_renderer_scanline.h"
#include "third_party/agg23/agg_scanline_u.h"
#include "third_party/base/ptr_util.h"

namespace {

CFX_PointF HardClip(const CFX_PointF& pos) {
  return CFX_PointF(std::max(std::min(pos.x, 50000.0f), -50000.0f),
                    std::max(std::min(pos.y, 50000.0f), -50000.0f));
}

void RgbByteOrderSetPixel(CFX_DIBitmap* pBitmap, int x, int y, uint32_t argb) {
  if (x < 0 || x >= pBitmap->GetWidth() || y < 0 || y >= pBitmap->GetHeight())
    return;

  uint8_t* pos = (uint8_t*)pBitmap->GetBuffer() + y * pBitmap->GetPitch() +
                 x * pBitmap->GetBPP() / 8;
  if (pBitmap->GetFormat() == FXDIB_Argb) {
    FXARGB_SETRGBORDERDIB(pos, argb);
    return;
  }

  int alpha = FXARGB_A(argb);
  pos[0] = (FXARGB_R(argb) * alpha + pos[0] * (255 - alpha)) / 255;
  pos[1] = (FXARGB_G(argb) * alpha + pos[1] * (255 - alpha)) / 255;
  pos[2] = (FXARGB_B(argb) * alpha + pos[2] * (255 - alpha)) / 255;
}

void RgbByteOrderCompositeRect(CFX_DIBitmap* pBitmap,
                               int left,
                               int top,
                               int width,
                               int height,
                               FX_ARGB argb) {
  int src_alpha = FXARGB_A(argb);
  if (src_alpha == 0)
    return;

  FX_RECT rect(left, top, left + width, top + height);
  rect.Intersect(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight());
  width = rect.Width();
  int src_r = FXARGB_R(argb);
  int src_g = FXARGB_G(argb);
  int src_b = FXARGB_B(argb);
  int Bpp = pBitmap->GetBPP() / 8;
  bool bAlpha = pBitmap->HasAlpha();
  int dib_argb = FXARGB_TOBGRORDERDIB(argb);
  uint8_t* pBuffer = pBitmap->GetBuffer();
  if (src_alpha == 255) {
    for (int row = rect.top; row < rect.bottom; row++) {
      uint8_t* dest_scan =
          pBuffer + row * pBitmap->GetPitch() + rect.left * Bpp;
      if (Bpp == 4) {
        uint32_t* scan = (uint32_t*)dest_scan;
        for (int col = 0; col < width; col++)
          *scan++ = dib_argb;
      } else {
        for (int col = 0; col < width; col++) {
          *dest_scan++ = src_r;
          *dest_scan++ = src_g;
          *dest_scan++ = src_b;
        }
      }
    }
    return;
  }
  for (int row = rect.top; row < rect.bottom; row++) {
    uint8_t* dest_scan = pBuffer + row * pBitmap->GetPitch() + rect.left * Bpp;
    if (bAlpha) {
      for (int col = 0; col < width; col++) {
        uint8_t back_alpha = dest_scan[3];
        if (back_alpha == 0) {
          FXARGB_SETRGBORDERDIB(dest_scan,
                                FXARGB_MAKE(src_alpha, src_r, src_g, src_b));
          dest_scan += 4;
          continue;
        }
        uint8_t dest_alpha =
            back_alpha + src_alpha - back_alpha * src_alpha / 255;
        dest_scan[3] = dest_alpha;
        int alpha_ratio = src_alpha * 255 / dest_alpha;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, alpha_ratio);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, alpha_ratio);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, alpha_ratio);
        dest_scan += 2;
      }
    } else {
      for (int col = 0; col < width; col++) {
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, src_alpha);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, src_alpha);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, src_alpha);
        dest_scan++;
        if (Bpp == 4)
          dest_scan++;
      }
    }
  }
}

void RgbByteOrderTransferBitmap(CFX_DIBitmap* pBitmap,
                                int dest_left,
                                int dest_top,
                                int width,
                                int height,
                                const CFX_DIBSource* pSrcBitmap,
                                int src_left,
                                int src_top) {
  if (!pBitmap)
    return;

  pBitmap->GetOverlapRect(dest_left, dest_top, width, height,
                          pSrcBitmap->GetWidth(), pSrcBitmap->GetHeight(),
                          src_left, src_top, nullptr);
  if (width == 0 || height == 0)
    return;

  int Bpp = pBitmap->GetBPP() / 8;
  FXDIB_Format dest_format = pBitmap->GetFormat();
  FXDIB_Format src_format = pSrcBitmap->GetFormat();
  int pitch = pBitmap->GetPitch();
  uint8_t* buffer = pBitmap->GetBuffer();
  if (dest_format == src_format) {
    for (int row = 0; row < height; row++) {
      uint8_t* dest_scan = buffer + (dest_top + row) * pitch + dest_left * Bpp;
      uint8_t* src_scan =
          (uint8_t*)pSrcBitmap->GetScanline(src_top + row) + src_left * Bpp;
      if (Bpp == 4) {
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, FXARGB_MAKE(src_scan[3], src_scan[0],
                                               src_scan[1], src_scan[2]));
          dest_scan += 4;
          src_scan += 4;
        }
      } else {
        for (int col = 0; col < width; col++) {
          *dest_scan++ = src_scan[2];
          *dest_scan++ = src_scan[1];
          *dest_scan++ = src_scan[0];
          src_scan += 3;
        }
      }
    }
    return;
  }

  uint8_t* dest_buf = buffer + dest_top * pitch + dest_left * Bpp;
  if (dest_format == FXDIB_Rgb) {
    if (src_format == FXDIB_Rgb32) {
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = dest_buf + row * pitch;
        uint8_t* src_scan =
            (uint8_t*)pSrcBitmap->GetScanline(src_top + row) + src_left * 4;
        for (int col = 0; col < width; col++) {
          *dest_scan++ = src_scan[2];
          *dest_scan++ = src_scan[1];
          *dest_scan++ = src_scan[0];
          src_scan += 4;
        }
      }
    } else {
      ASSERT(false);
    }
    return;
  }

  if (dest_format == FXDIB_Argb || dest_format == FXDIB_Rgb32) {
    if (src_format == FXDIB_Rgb) {
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = (uint8_t*)(dest_buf + row * pitch);
        uint8_t* src_scan =
            (uint8_t*)pSrcBitmap->GetScanline(src_top + row) + src_left * 3;
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, FXARGB_MAKE(0xff, src_scan[0], src_scan[1],
                                               src_scan[2]));
          dest_scan += 4;
          src_scan += 3;
        }
      }
    } else if (src_format == FXDIB_Rgb32) {
      ASSERT(dest_format == FXDIB_Argb);
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = dest_buf + row * pitch;
        uint8_t* src_scan =
            (uint8_t*)(pSrcBitmap->GetScanline(src_top + row) + src_left * 4);
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, FXARGB_MAKE(0xff, src_scan[0], src_scan[1],
                                               src_scan[2]));
          src_scan += 4;
          dest_scan += 4;
        }
      }
    }
    return;
  }

  ASSERT(false);
}

FX_ARGB DefaultCMYK2ARGB(FX_CMYK cmyk, uint8_t alpha) {
  uint8_t r, g, b;
  AdobeCMYK_to_sRGB1(FXSYS_GetCValue(cmyk), FXSYS_GetMValue(cmyk),
                     FXSYS_GetYValue(cmyk), FXSYS_GetKValue(cmyk), r, g, b);
  return ArgbEncode(alpha, r, g, b);
}

bool DibSetPixel(CFX_DIBitmap* pDevice,
                 int x,
                 int y,
                 uint32_t color,
                 int alpha_flag,
                 void* pIccTransform) {
  bool bObjCMYK = !!FXGETFLAG_COLORTYPE(alpha_flag);
  int alpha = bObjCMYK ? FXGETFLAG_ALPHA_FILL(alpha_flag) : FXARGB_A(color);
  if (pIccTransform) {
    CCodec_IccModule* pIccModule =
        CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
    color = bObjCMYK ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
    pIccModule->TranslateScanline(pIccTransform, (uint8_t*)&color,
                                  (uint8_t*)&color, 1);
    color = bObjCMYK ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
    if (!pDevice->IsCmykImage()) {
      color = (color & 0xffffff) | (alpha << 24);
    }
  } else {
    if (pDevice->IsCmykImage()) {
      if (!bObjCMYK)
        return false;
    } else {
      if (bObjCMYK)
        color = DefaultCMYK2ARGB(color, alpha);
    }
  }
  pDevice->SetPixel(x, y, color);
  if (pDevice->m_pAlphaMask) {
    pDevice->m_pAlphaMask->SetPixel(x, y, alpha << 24);
  }
  return true;
}

}  // namespace

void CAgg_PathData::BuildPath(const CFX_PathData* pPathData,
                              const CFX_Matrix* pObject2Device) {
  const std::vector<FX_PATHPOINT>& pPoints = pPathData->GetPoints();
  for (size_t i = 0; i < pPoints.size(); i++) {
    CFX_PointF pos = pPoints[i].m_Point;
    if (pObject2Device)
      pos = pObject2Device->Transform(pos);

    pos = HardClip(pos);
    FXPT_TYPE point_type = pPoints[i].m_Type;
    if (point_type == FXPT_TYPE::MoveTo) {
      m_PathData.move_to(pos.x, pos.y);
    } else if (point_type == FXPT_TYPE::LineTo) {
      if (pPoints[i - 1].IsTypeAndOpen(FXPT_TYPE::MoveTo) &&
          (i == pPoints.size() - 1 ||
           pPoints[i + 1].IsTypeAndOpen(FXPT_TYPE::MoveTo)) &&
          pPoints[i].m_Point == pPoints[i - 1].m_Point) {
        pos.x += 1;
      }
      m_PathData.line_to(pos.x, pos.y);
    } else if (point_type == FXPT_TYPE::BezierTo) {
      CFX_PointF pos0 = pPoints[i - 1].m_Point;
      CFX_PointF pos2 = pPoints[i + 1].m_Point;
      CFX_PointF pos3 = pPoints[i + 2].m_Point;
      if (pObject2Device) {
        pos0 = pObject2Device->Transform(pos0);
        pos2 = pObject2Device->Transform(pos2);
        pos3 = pObject2Device->Transform(pos3);
      }
      pos0 = HardClip(pos0);
      pos2 = HardClip(pos2);
      pos3 = HardClip(pos3);
      agg::curve4 curve(pos0.x, pos0.y, pos.x, pos.y, pos2.x, pos2.y, pos3.x,
                        pos3.y);
      i += 2;
      m_PathData.add_path_curve(curve);
    }
    if (pPoints[i].m_CloseFigure)
      m_PathData.end_poly();
  }
}

namespace agg {

template <class BaseRenderer>
class renderer_scanline_aa_offset {
 public:
  typedef BaseRenderer base_ren_type;
  typedef typename base_ren_type::color_type color_type;
  renderer_scanline_aa_offset(base_ren_type& ren, unsigned left, unsigned top)
      : m_ren(&ren), m_left(left), m_top(top) {}
  void color(const color_type& c) { m_color = c; }
  const color_type& color() const { return m_color; }
  void prepare(unsigned) {}
  template <class Scanline>
  void render(const Scanline& sl) {
    int y = sl.y();
    unsigned num_spans = sl.num_spans();
    typename Scanline::const_iterator span = sl.begin();
    for (;;) {
      int x = span->x;
      if (span->len > 0) {
        m_ren->blend_solid_hspan(x - m_left, y - m_top, (unsigned)span->len,
                                 m_color, span->covers);
      } else {
        m_ren->blend_hline(x - m_left, y - m_top, (unsigned)(x - span->len - 1),
                           m_color, *(span->covers));
      }
      if (--num_spans == 0) {
        break;
      }
      ++span;
    }
  }

 private:
  base_ren_type* m_ren;
  color_type m_color;
  unsigned m_left, m_top;
};

}  // namespace agg

static void RasterizeStroke(agg::rasterizer_scanline_aa& rasterizer,
                            agg::path_storage& path_data,
                            const CFX_Matrix* pObject2Device,
                            const CFX_GraphStateData* pGraphState,
                            FX_FLOAT scale = 1.0f,
                            bool bStrokeAdjust = false,
                            bool bTextMode = false) {
  agg::line_cap_e cap;
  switch (pGraphState->m_LineCap) {
    case CFX_GraphStateData::LineCapRound:
      cap = agg::round_cap;
      break;
    case CFX_GraphStateData::LineCapSquare:
      cap = agg::square_cap;
      break;
    default:
      cap = agg::butt_cap;
      break;
  }
  agg::line_join_e join;
  switch (pGraphState->m_LineJoin) {
    case CFX_GraphStateData::LineJoinRound:
      join = agg::round_join;
      break;
    case CFX_GraphStateData::LineJoinBevel:
      join = agg::bevel_join;
      break;
    default:
      join = agg::miter_join_revert;
      break;
  }
  FX_FLOAT width = pGraphState->m_LineWidth * scale;
  FX_FLOAT unit = 1.f;
  if (pObject2Device) {
    unit =
        1.0f / ((pObject2Device->GetXUnit() + pObject2Device->GetYUnit()) / 2);
  }
  if (width < unit) {
    width = unit;
  }
  if (pGraphState->m_DashArray) {
    typedef agg::conv_dash<agg::path_storage> dash_converter;
    dash_converter dash(path_data);
    for (int i = 0; i < (pGraphState->m_DashCount + 1) / 2; i++) {
      FX_FLOAT on = pGraphState->m_DashArray[i * 2];
      if (on <= 0.000001f) {
        on = 1.0f / 10;
      }
      FX_FLOAT off = i * 2 + 1 == pGraphState->m_DashCount
                         ? on
                         : pGraphState->m_DashArray[i * 2 + 1];
      if (off < 0) {
        off = 0;
      }
      dash.add_dash(on * scale, off * scale);
    }
    dash.dash_start(pGraphState->m_DashPhase * scale);
    typedef agg::conv_stroke<dash_converter> dash_stroke;
    dash_stroke stroke(dash);
    stroke.line_join(join);
    stroke.line_cap(cap);
    stroke.miter_limit(pGraphState->m_MiterLimit);
    stroke.width(width);
    rasterizer.add_path_transformed(stroke, pObject2Device);
  } else {
    agg::conv_stroke<agg::path_storage> stroke(path_data);
    stroke.line_join(join);
    stroke.line_cap(cap);
    stroke.miter_limit(pGraphState->m_MiterLimit);
    stroke.width(width);
    rasterizer.add_path_transformed(stroke, pObject2Device);
  }
}

CFX_AggDeviceDriver::CFX_AggDeviceDriver(CFX_DIBitmap* pBitmap,
                                         bool bRgbByteOrder,
                                         CFX_DIBitmap* pOriDevice,
                                         bool bGroupKnockout)
    : m_pBitmap(pBitmap),
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      m_pPlatformGraphics(nullptr),
#endif
      m_FillFlags(0),
      m_bRgbByteOrder(bRgbByteOrder),
      m_pOriDevice(pOriDevice),
      m_bGroupKnockout(bGroupKnockout) {
  InitPlatform();
}

CFX_AggDeviceDriver::~CFX_AggDeviceDriver() {
  DestroyPlatform();
}

uint8_t* CFX_AggDeviceDriver::GetBuffer() const {
  return m_pBitmap->GetBuffer();
}

#if _FXM_PLATFORM_ != _FXM_PLATFORM_APPLE_
void CFX_AggDeviceDriver::InitPlatform() {}

void CFX_AggDeviceDriver::DestroyPlatform() {}

bool CFX_AggDeviceDriver::DrawDeviceText(int nChars,
                                         const FXTEXT_CHARPOS* pCharPos,
                                         CFX_Font* pFont,
                                         const CFX_Matrix* pObject2Device,
                                         FX_FLOAT font_size,
                                         uint32_t color) {
  return false;
}
#endif  // _FXM_PLATFORM_ != _FXM_PLATFORM_APPLE_

int CFX_AggDeviceDriver::GetDeviceCaps(int caps_id) const {
  switch (caps_id) {
    case FXDC_DEVICE_CLASS:
      return FXDC_DISPLAY;
    case FXDC_PIXEL_WIDTH:
      return m_pBitmap->GetWidth();
    case FXDC_PIXEL_HEIGHT:
      return m_pBitmap->GetHeight();
    case FXDC_BITS_PIXEL:
      return m_pBitmap->GetBPP();
    case FXDC_HORZ_SIZE:
    case FXDC_VERT_SIZE:
      return 0;
    case FXDC_RENDER_CAPS: {
      int flags = FXRC_GET_BITS | FXRC_ALPHA_PATH | FXRC_ALPHA_IMAGE |
                  FXRC_BLEND_MODE | FXRC_SOFT_CLIP;
      if (m_pBitmap->HasAlpha()) {
        flags |= FXRC_ALPHA_OUTPUT;
      } else if (m_pBitmap->IsAlphaMask()) {
        if (m_pBitmap->GetBPP() == 1) {
          flags |= FXRC_BITMASK_OUTPUT;
        } else {
          flags |= FXRC_BYTEMASK_OUTPUT;
        }
      }
      if (m_pBitmap->IsCmykImage()) {
        flags |= FXRC_CMYK_OUTPUT;
      }
      return flags;
    }
  }
  return 0;
}

void CFX_AggDeviceDriver::SaveState() {
  std::unique_ptr<CFX_ClipRgn> pClip;
  if (m_pClipRgn)
    pClip = pdfium::MakeUnique<CFX_ClipRgn>(*m_pClipRgn);
  m_StateStack.push_back(std::move(pClip));
}

void CFX_AggDeviceDriver::RestoreState(bool bKeepSaved) {
  m_pClipRgn.reset();

  if (m_StateStack.empty())
    return;

  if (bKeepSaved) {
    if (m_StateStack.back())
      m_pClipRgn = pdfium::MakeUnique<CFX_ClipRgn>(*m_StateStack.back());
  } else {
    m_pClipRgn = std::move(m_StateStack.back());
    m_StateStack.pop_back();
  }
}

void CFX_AggDeviceDriver::SetClipMask(agg::rasterizer_scanline_aa& rasterizer) {
  FX_RECT path_rect(rasterizer.min_x(), rasterizer.min_y(),
                    rasterizer.max_x() + 1, rasterizer.max_y() + 1);
  path_rect.Intersect(m_pClipRgn->GetBox());
  CFX_DIBitmapRef mask;
  CFX_DIBitmap* pThisLayer = mask.Emplace();
  pThisLayer->Create(path_rect.Width(), path_rect.Height(), FXDIB_8bppMask);
  pThisLayer->Clear(0);
  agg::rendering_buffer raw_buf(pThisLayer->GetBuffer(), pThisLayer->GetWidth(),
                                pThisLayer->GetHeight(),
                                pThisLayer->GetPitch());
  agg::pixfmt_gray8 pixel_buf(raw_buf);
  agg::renderer_base<agg::pixfmt_gray8> base_buf(pixel_buf);
  agg::renderer_scanline_aa_offset<agg::renderer_base<agg::pixfmt_gray8> >
      final_render(base_buf, path_rect.left, path_rect.top);
  final_render.color(agg::gray8(255));
  agg::scanline_u8 scanline;
  agg::render_scanlines(rasterizer, scanline, final_render,
                        (m_FillFlags & FXFILL_NOPATHSMOOTH) != 0);
  m_pClipRgn->IntersectMaskF(path_rect.left, path_rect.top, mask);
}

bool CFX_AggDeviceDriver::SetClip_PathFill(const CFX_PathData* pPathData,
                                           const CFX_Matrix* pObject2Device,
                                           int fill_mode) {
  m_FillFlags = fill_mode;
  if (!m_pClipRgn) {
    m_pClipRgn = pdfium::MakeUnique<CFX_ClipRgn>(
        GetDeviceCaps(FXDC_PIXEL_WIDTH), GetDeviceCaps(FXDC_PIXEL_HEIGHT));
  }
  size_t size = pPathData->GetPoints().size();
  if (size == 5 || size == 4) {
    CFX_FloatRect rectf;
    if (pPathData->IsRect(pObject2Device, &rectf)) {
      rectf.Intersect(
          CFX_FloatRect(0, 0, (FX_FLOAT)GetDeviceCaps(FXDC_PIXEL_WIDTH),
                        (FX_FLOAT)GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
      FX_RECT rect = rectf.GetOuterRect();
      m_pClipRgn->IntersectRect(rect);
      return true;
    }
  }
  CAgg_PathData path_data;
  path_data.BuildPath(pPathData, pObject2Device);
  path_data.m_PathData.end_poly();
  agg::rasterizer_scanline_aa rasterizer;
  rasterizer.clip_box(0.0f, 0.0f, (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_WIDTH)),
                      (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
  rasterizer.add_path(path_data.m_PathData);
  rasterizer.filling_rule((fill_mode & 3) == FXFILL_WINDING
                              ? agg::fill_non_zero
                              : agg::fill_even_odd);
  SetClipMask(rasterizer);
  return true;
}

bool CFX_AggDeviceDriver::SetClip_PathStroke(
    const CFX_PathData* pPathData,
    const CFX_Matrix* pObject2Device,
    const CFX_GraphStateData* pGraphState) {
  if (!m_pClipRgn) {
    m_pClipRgn = pdfium::MakeUnique<CFX_ClipRgn>(
        GetDeviceCaps(FXDC_PIXEL_WIDTH), GetDeviceCaps(FXDC_PIXEL_HEIGHT));
  }
  CAgg_PathData path_data;
  path_data.BuildPath(pPathData, nullptr);
  agg::rasterizer_scanline_aa rasterizer;
  rasterizer.clip_box(0.0f, 0.0f, (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_WIDTH)),
                      (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
  RasterizeStroke(rasterizer, path_data.m_PathData, pObject2Device,
                  pGraphState);
  rasterizer.filling_rule(agg::fill_non_zero);
  SetClipMask(rasterizer);
  return true;
}

class CFX_Renderer {
 private:
  int m_Alpha, m_Red, m_Green, m_Blue, m_Gray;
  uint32_t m_Color;
  bool m_bFullCover;
  bool m_bRgbByteOrder;
  CFX_DIBitmap* m_pOriDevice;
  FX_RECT m_ClipBox;
  const CFX_DIBitmap* m_pClipMask;
  CFX_DIBitmap* m_pDevice;
  const CFX_ClipRgn* m_pClipRgn;
  void (CFX_Renderer::*composite_span)(uint8_t*,
                                       int,
                                       int,
                                       int,
                                       uint8_t*,
                                       int,
                                       int,
                                       uint8_t*,
                                       uint8_t*);

 public:
  void prepare(unsigned) {}

  void CompositeSpan(uint8_t* dest_scan,
                     uint8_t* ori_scan,
                     int Bpp,
                     bool bDestAlpha,
                     int span_left,
                     int span_len,
                     uint8_t* cover_scan,
                     int clip_left,
                     int clip_right,
                     uint8_t* clip_scan) {
    ASSERT(!m_pDevice->IsCmykImage());
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    if (Bpp) {
      dest_scan += col_start * Bpp;
      ori_scan += col_start * Bpp;
    } else {
      dest_scan += col_start / 8;
      ori_scan += col_start / 8;
    }
    if (m_bRgbByteOrder) {
      if (Bpp == 4 && bDestAlpha) {
        for (int col = col_start; col < col_end; col++) {
          int src_alpha;
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
          uint8_t dest_alpha =
              ori_scan[3] + src_alpha - ori_scan[3] * src_alpha / 255;
          dest_scan[3] = dest_alpha;
          int alpha_ratio = src_alpha * 255 / dest_alpha;
          if (m_bFullCover) {
            *dest_scan++ = FXDIB_ALPHA_MERGE(*ori_scan++, m_Red, alpha_ratio);
            *dest_scan++ = FXDIB_ALPHA_MERGE(*ori_scan++, m_Green, alpha_ratio);
            *dest_scan++ = FXDIB_ALPHA_MERGE(*ori_scan++, m_Blue, alpha_ratio);
            dest_scan++;
            ori_scan++;
          } else {
            int r = FXDIB_ALPHA_MERGE(*ori_scan++, m_Red, alpha_ratio);
            int g = FXDIB_ALPHA_MERGE(*ori_scan++, m_Green, alpha_ratio);
            int b = FXDIB_ALPHA_MERGE(*ori_scan++, m_Blue, alpha_ratio);
            ori_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, r, cover_scan[col]);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, g, cover_scan[col]);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, b, cover_scan[col]);
            dest_scan += 2;
          }
        }
        return;
      }
      if (Bpp == 3 || Bpp == 4) {
        for (int col = col_start; col < col_end; col++) {
          int src_alpha;
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
          int r = FXDIB_ALPHA_MERGE(*ori_scan++, m_Red, src_alpha);
          int g = FXDIB_ALPHA_MERGE(*ori_scan++, m_Green, src_alpha);
          int b = FXDIB_ALPHA_MERGE(*ori_scan, m_Blue, src_alpha);
          ori_scan += Bpp - 2;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, r, cover_scan[col]);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, g, cover_scan[col]);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, b, cover_scan[col]);
          dest_scan += Bpp - 2;
        }
      }
      return;
    }
    if (Bpp == 4 && bDestAlpha) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * clip_scan[col] / 255;
        } else {
          src_alpha = m_Alpha;
        }
        int src_alpha_covered = src_alpha * cover_scan[col] / 255;
        if (src_alpha_covered == 0) {
          dest_scan += 4;
          continue;
        }
        if (cover_scan[col] == 255) {
          dest_scan[3] = src_alpha_covered;
          *dest_scan++ = m_Blue;
          *dest_scan++ = m_Green;
          *dest_scan = m_Red;
          dest_scan += 2;
          continue;
        } else {
          if (dest_scan[3] == 0) {
            dest_scan[3] = src_alpha_covered;
            *dest_scan++ = m_Blue;
            *dest_scan++ = m_Green;
            *dest_scan = m_Red;
            dest_scan += 2;
            continue;
          }
          uint8_t cover = cover_scan[col];
          dest_scan[3] = FXDIB_ALPHA_MERGE(dest_scan[3], src_alpha, cover);
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, cover);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, cover);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, cover);
          dest_scan += 2;
        }
      }
      return;
    }
    if (Bpp == 3 || Bpp == 4) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * clip_scan[col] / 255;
        } else {
          src_alpha = m_Alpha;
        }
        if (m_bFullCover) {
          *dest_scan++ = FXDIB_ALPHA_MERGE(*ori_scan++, m_Blue, src_alpha);
          *dest_scan++ = FXDIB_ALPHA_MERGE(*ori_scan++, m_Green, src_alpha);
          *dest_scan = FXDIB_ALPHA_MERGE(*ori_scan, m_Red, src_alpha);
          dest_scan += Bpp - 2;
          ori_scan += Bpp - 2;
          continue;
        }
        int b = FXDIB_ALPHA_MERGE(*ori_scan++, m_Blue, src_alpha);
        int g = FXDIB_ALPHA_MERGE(*ori_scan++, m_Green, src_alpha);
        int r = FXDIB_ALPHA_MERGE(*ori_scan, m_Red, src_alpha);
        ori_scan += Bpp - 2;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, b, cover_scan[col]);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, g, cover_scan[col]);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, r, cover_scan[col]);
        dest_scan += Bpp - 2;
        continue;
      }
      return;
    }
    if (Bpp == 1) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * clip_scan[col] / 255;
        } else {
          src_alpha = m_Alpha;
        }
        if (m_bFullCover) {
          *dest_scan = FXDIB_ALPHA_MERGE(*ori_scan++, m_Gray, src_alpha);
        } else {
          int gray = FXDIB_ALPHA_MERGE(*ori_scan++, m_Gray, src_alpha);
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, cover_scan[col]);
          dest_scan++;
        }
      }
    } else {
      int index = 0;
      if (m_pDevice->GetPalette()) {
        for (int i = 0; i < 2; i++) {
          if (FXARGB_TODIB(m_pDevice->GetPalette()[i]) == m_Color) {
            index = i;
          }
        }
      } else {
        index = ((uint8_t)m_Color == 0xff) ? 1 : 0;
      }
      uint8_t* dest_scan1 = dest_scan;
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
        } else {
          src_alpha = m_Alpha * cover_scan[col] / 255;
        }
        if (src_alpha) {
          if (!index) {
            *dest_scan1 &= ~(1 << (7 - (col + span_left) % 8));
          } else {
            *dest_scan1 |= 1 << (7 - (col + span_left) % 8);
          }
        }
        dest_scan1 = dest_scan + (span_left % 8 + col - col_start + 1) / 8;
      }
    }
  }

  void CompositeSpan1bpp(uint8_t* dest_scan,
                         int Bpp,
                         int span_left,
                         int span_len,
                         uint8_t* cover_scan,
                         int clip_left,
                         int clip_right,
                         uint8_t* clip_scan,
                         uint8_t* dest_extra_alpha_scan) {
    ASSERT(!m_bRgbByteOrder);
    ASSERT(!m_pDevice->IsCmykImage());
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    dest_scan += col_start / 8;
    int index = 0;
    if (m_pDevice->GetPalette()) {
      for (int i = 0; i < 2; i++) {
        if (FXARGB_TODIB(m_pDevice->GetPalette()[i]) == m_Color) {
          index = i;
        }
      }
    } else {
      index = ((uint8_t)m_Color == 0xff) ? 1 : 0;
    }
    uint8_t* dest_scan1 = dest_scan;
    for (int col = col_start; col < col_end; col++) {
      int src_alpha;
      if (clip_scan) {
        src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
      } else {
        src_alpha = m_Alpha * cover_scan[col] / 255;
      }
      if (src_alpha) {
        if (!index) {
          *dest_scan1 &= ~(1 << (7 - (col + span_left) % 8));
        } else {
          *dest_scan1 |= 1 << (7 - (col + span_left) % 8);
        }
      }
      dest_scan1 = dest_scan + (span_left % 8 + col - col_start + 1) / 8;
    }
  }

  void CompositeSpanGray(uint8_t* dest_scan,
                         int Bpp,
                         int span_left,
                         int span_len,
                         uint8_t* cover_scan,
                         int clip_left,
                         int clip_right,
                         uint8_t* clip_scan,
                         uint8_t* dest_extra_alpha_scan) {
    ASSERT(!m_bRgbByteOrder);
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    dest_scan += col_start;
    if (dest_extra_alpha_scan) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (m_bFullCover) {
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
        } else {
          if (clip_scan) {
            src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
          } else {
            src_alpha = m_Alpha * cover_scan[col] / 255;
          }
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *dest_scan = m_Gray;
            *dest_extra_alpha_scan = m_Alpha;
          } else {
            uint8_t dest_alpha = (*dest_extra_alpha_scan) + src_alpha -
                                 (*dest_extra_alpha_scan) * src_alpha / 255;
            *dest_extra_alpha_scan++ = dest_alpha;
            int alpha_ratio = src_alpha * 255 / dest_alpha;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Gray, alpha_ratio);
            dest_scan++;
            continue;
          }
        }
        dest_extra_alpha_scan++;
        dest_scan++;
      }
    } else {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
        } else {
          src_alpha = m_Alpha * cover_scan[col] / 255;
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *dest_scan = m_Gray;
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Gray, src_alpha);
          }
        }
        dest_scan++;
      }
    }
  }

  void CompositeSpanARGB(uint8_t* dest_scan,
                         int Bpp,
                         int span_left,
                         int span_len,
                         uint8_t* cover_scan,
                         int clip_left,
                         int clip_right,
                         uint8_t* clip_scan,
                         uint8_t* dest_extra_alpha_scan) {
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    dest_scan += col_start * Bpp;
    if (m_bRgbByteOrder) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (m_bFullCover) {
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
        } else {
          if (clip_scan) {
            src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
          } else {
            src_alpha = m_Alpha * cover_scan[col] / 255;
          }
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *(uint32_t*)dest_scan = m_Color;
          } else {
            uint8_t dest_alpha =
                dest_scan[3] + src_alpha - dest_scan[3] * src_alpha / 255;
            dest_scan[3] = dest_alpha;
            int alpha_ratio = src_alpha * 255 / dest_alpha;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, alpha_ratio);
            dest_scan += 2;
            continue;
          }
        }
        dest_scan += 4;
      }
      return;
    }
    for (int col = col_start; col < col_end; col++) {
      int src_alpha;
      if (m_bFullCover) {
        if (clip_scan) {
          src_alpha = m_Alpha * clip_scan[col] / 255;
        } else {
          src_alpha = m_Alpha;
        }
      } else {
        if (clip_scan) {
          src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
        } else {
          src_alpha = m_Alpha * cover_scan[col] / 255;
        }
      }
      if (src_alpha) {
        if (src_alpha == 255) {
          *(uint32_t*)dest_scan = m_Color;
        } else {
          if (dest_scan[3] == 0) {
            dest_scan[3] = src_alpha;
            *dest_scan++ = m_Blue;
            *dest_scan++ = m_Green;
            *dest_scan = m_Red;
            dest_scan += 2;
            continue;
          }
          uint8_t dest_alpha =
              dest_scan[3] + src_alpha - dest_scan[3] * src_alpha / 255;
          dest_scan[3] = dest_alpha;
          int alpha_ratio = src_alpha * 255 / dest_alpha;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, alpha_ratio);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, alpha_ratio);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, alpha_ratio);
          dest_scan += 2;
          continue;
        }
      }
      dest_scan += Bpp;
    }
  }

  void CompositeSpanRGB(uint8_t* dest_scan,
                        int Bpp,
                        int span_left,
                        int span_len,
                        uint8_t* cover_scan,
                        int clip_left,
                        int clip_right,
                        uint8_t* clip_scan,
                        uint8_t* dest_extra_alpha_scan) {
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    dest_scan += col_start * Bpp;
    if (m_bRgbByteOrder) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
        } else {
          src_alpha = m_Alpha * cover_scan[col] / 255;
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            if (Bpp == 4) {
              *(uint32_t*)dest_scan = m_Color;
            } else if (Bpp == 3) {
              *dest_scan++ = m_Red;
              *dest_scan++ = m_Green;
              *dest_scan++ = m_Blue;
              continue;
            }
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, src_alpha);
            dest_scan += Bpp - 2;
            continue;
          }
        }
        dest_scan += Bpp;
      }
      return;
    }
    if (Bpp == 3 && dest_extra_alpha_scan) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (m_bFullCover) {
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
        } else {
          if (clip_scan) {
            src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
          } else {
            src_alpha = m_Alpha * cover_scan[col] / 255;
          }
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *dest_scan++ = (uint8_t)m_Blue;
            *dest_scan++ = (uint8_t)m_Green;
            *dest_scan++ = (uint8_t)m_Red;
            *dest_extra_alpha_scan++ = (uint8_t)m_Alpha;
            continue;
          } else {
            uint8_t dest_alpha = (*dest_extra_alpha_scan) + src_alpha -
                                 (*dest_extra_alpha_scan) * src_alpha / 255;
            *dest_extra_alpha_scan++ = dest_alpha;
            int alpha_ratio = src_alpha * 255 / dest_alpha;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, alpha_ratio);
            dest_scan++;
            continue;
          }
        }
        dest_extra_alpha_scan++;
        dest_scan += Bpp;
      }
    } else {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (m_bFullCover) {
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
        } else {
          if (clip_scan) {
            src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
          } else {
            src_alpha = m_Alpha * cover_scan[col] / 255;
          }
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            if (Bpp == 4) {
              *(uint32_t*)dest_scan = m_Color;
            } else if (Bpp == 3) {
              *dest_scan++ = m_Blue;
              *dest_scan++ = m_Green;
              *dest_scan++ = m_Red;
              continue;
            }
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, src_alpha);
            dest_scan += Bpp - 2;
            continue;
          }
        }
        dest_scan += Bpp;
      }
    }
  }

  void CompositeSpanCMYK(uint8_t* dest_scan,
                         int Bpp,
                         int span_left,
                         int span_len,
                         uint8_t* cover_scan,
                         int clip_left,
                         int clip_right,
                         uint8_t* clip_scan,
                         uint8_t* dest_extra_alpha_scan) {
    ASSERT(!m_bRgbByteOrder);
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    dest_scan += col_start * 4;
    if (dest_extra_alpha_scan) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (m_bFullCover) {
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
        } else {
          if (clip_scan) {
            src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
          } else {
            src_alpha = m_Alpha * cover_scan[col] / 255;
          }
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *(FX_CMYK*)dest_scan = m_Color;
            *dest_extra_alpha_scan = (uint8_t)m_Alpha;
          } else {
            uint8_t dest_alpha = (*dest_extra_alpha_scan) + src_alpha -
                                 (*dest_extra_alpha_scan) * src_alpha / 255;
            *dest_extra_alpha_scan++ = dest_alpha;
            int alpha_ratio = src_alpha * 255 / dest_alpha;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Gray, alpha_ratio);
            dest_scan++;
            continue;
          }
        }
        dest_extra_alpha_scan++;
        dest_scan += 4;
      }
    } else {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
        } else {
          src_alpha = m_Alpha * cover_scan[col] / 255;
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *(FX_CMYK*)dest_scan = m_Color;
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Gray, src_alpha);
            dest_scan++;
            continue;
          }
        }
        dest_scan += 4;
      }
    }
  }

  template <class Scanline>
  void render(const Scanline& sl) {
    if (!m_pOriDevice && !composite_span) {
      return;
    }
    int y = sl.y();
    if (y < m_ClipBox.top || y >= m_ClipBox.bottom) {
      return;
    }
    uint8_t* dest_scan = m_pDevice->GetBuffer() + m_pDevice->GetPitch() * y;
    uint8_t* dest_scan_extra_alpha = nullptr;
    CFX_DIBitmap* pAlphaMask = m_pDevice->m_pAlphaMask;
    if (pAlphaMask) {
      dest_scan_extra_alpha =
          pAlphaMask->GetBuffer() + pAlphaMask->GetPitch() * y;
    }
    uint8_t* ori_scan = nullptr;
    if (m_pOriDevice) {
      ori_scan = m_pOriDevice->GetBuffer() + m_pOriDevice->GetPitch() * y;
    }
    int Bpp = m_pDevice->GetBPP() / 8;
    bool bDestAlpha = m_pDevice->HasAlpha() || m_pDevice->IsAlphaMask();
    unsigned num_spans = sl.num_spans();
    typename Scanline::const_iterator span = sl.begin();
    while (1) {
      int x = span->x;
      ASSERT(span->len > 0);
      uint8_t* dest_pos = nullptr;
      uint8_t* dest_extra_alpha_pos = nullptr;
      uint8_t* ori_pos = nullptr;
      if (Bpp) {
        ori_pos = ori_scan ? ori_scan + x * Bpp : nullptr;
        dest_pos = dest_scan + x * Bpp;
        dest_extra_alpha_pos =
            dest_scan_extra_alpha ? dest_scan_extra_alpha + x : nullptr;
      } else {
        dest_pos = dest_scan + x / 8;
        ori_pos = ori_scan ? ori_scan + x / 8 : nullptr;
      }
      uint8_t* clip_pos = nullptr;
      if (m_pClipMask) {
        clip_pos = m_pClipMask->GetBuffer() +
                   (y - m_ClipBox.top) * m_pClipMask->GetPitch() + x -
                   m_ClipBox.left;
      }
      if (ori_pos) {
        CompositeSpan(dest_pos, ori_pos, Bpp, bDestAlpha, x, span->len,
                      span->covers, m_ClipBox.left, m_ClipBox.right, clip_pos);
      } else {
        (this->*composite_span)(dest_pos, Bpp, x, span->len, span->covers,
                                m_ClipBox.left, m_ClipBox.right, clip_pos,
                                dest_extra_alpha_pos);
      }
      if (--num_spans == 0) {
        break;
      }
      ++span;
    }
  }

  bool Init(CFX_DIBitmap* pDevice,
            CFX_DIBitmap* pOriDevice,
            const CFX_ClipRgn* pClipRgn,
            uint32_t color,
            bool bFullCover,
            bool bRgbByteOrder,
            int alpha_flag = 0,
            void* pIccTransform = nullptr) {
    m_pDevice = pDevice;
    m_pClipRgn = pClipRgn;
    composite_span = nullptr;
    m_bRgbByteOrder = bRgbByteOrder;
    m_pOriDevice = pOriDevice;
    if (m_pClipRgn) {
      m_ClipBox = m_pClipRgn->GetBox();
    } else {
      m_ClipBox.left = m_ClipBox.top = 0;
      m_ClipBox.right = m_pDevice->GetWidth();
      m_ClipBox.bottom = m_pDevice->GetHeight();
    }
    m_pClipMask = nullptr;
    if (m_pClipRgn && m_pClipRgn->GetType() == CFX_ClipRgn::MaskF) {
      m_pClipMask = m_pClipRgn->GetMask().GetObject();
    }
    m_bFullCover = bFullCover;
    bool bObjectCMYK = !!FXGETFLAG_COLORTYPE(alpha_flag);
    bool bDeviceCMYK = pDevice->IsCmykImage();
    m_Alpha = bObjectCMYK ? FXGETFLAG_ALPHA_FILL(alpha_flag) : FXARGB_A(color);
    CCodec_IccModule* pIccModule = nullptr;
    if (!CFX_GEModule::Get()->GetCodecModule() ||
        !CFX_GEModule::Get()->GetCodecModule()->GetIccModule()) {
      pIccTransform = nullptr;
    } else {
      pIccModule = CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
    }
    if (m_pDevice->GetBPP() == 8) {
      ASSERT(!m_bRgbByteOrder);
      composite_span = &CFX_Renderer::CompositeSpanGray;
      if (m_pDevice->IsAlphaMask()) {
        m_Gray = 255;
      } else {
        if (pIccTransform) {
          uint8_t gray;
          color = bObjectCMYK ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
          pIccModule->TranslateScanline(pIccTransform, &gray,
                                        (const uint8_t*)&color, 1);
          m_Gray = gray;
        } else {
          if (bObjectCMYK) {
            uint8_t r, g, b;
            AdobeCMYK_to_sRGB1(FXSYS_GetCValue(color), FXSYS_GetMValue(color),
                               FXSYS_GetYValue(color), FXSYS_GetKValue(color),
                               r, g, b);
            m_Gray = FXRGB2GRAY(r, g, b);
          } else {
            m_Gray =
                FXRGB2GRAY(FXARGB_R(color), FXARGB_G(color), FXARGB_B(color));
          }
        }
      }
      return true;
    }
    if (bDeviceCMYK) {
      ASSERT(!m_bRgbByteOrder);
      composite_span = &CFX_Renderer::CompositeSpanCMYK;
      if (bObjectCMYK) {
        m_Color = FXCMYK_TODIB(color);
        if (pIccTransform) {
          pIccModule->TranslateScanline(pIccTransform, (uint8_t*)&m_Color,
                                        (const uint8_t*)&m_Color, 1);
        }
      } else {
        if (!pIccTransform) {
          return false;
        }
        color = FXARGB_TODIB(color);
        pIccModule->TranslateScanline(pIccTransform, (uint8_t*)&m_Color,
                                      (const uint8_t*)&color, 1);
      }
      m_Red = ((uint8_t*)&m_Color)[0];
      m_Green = ((uint8_t*)&m_Color)[1];
      m_Blue = ((uint8_t*)&m_Color)[2];
      m_Gray = ((uint8_t*)&m_Color)[3];
    } else {
      composite_span = (pDevice->GetFormat() == FXDIB_Argb)
                           ? &CFX_Renderer::CompositeSpanARGB
                           : &CFX_Renderer::CompositeSpanRGB;
      if (pIccTransform) {
        color = bObjectCMYK ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
        pIccModule->TranslateScanline(pIccTransform, (uint8_t*)&m_Color,
                                      (const uint8_t*)&color, 1);
        ((uint8_t*)&m_Color)[3] = m_Alpha;
        m_Red = ((uint8_t*)&m_Color)[2];
        m_Green = ((uint8_t*)&m_Color)[1];
        m_Blue = ((uint8_t*)&m_Color)[0];
        if (m_bRgbByteOrder) {
          m_Color = FXARGB_TODIB(m_Color);
          m_Color = FXARGB_TOBGRORDERDIB(m_Color);
        }
      } else {
        if (bObjectCMYK) {
          uint8_t r, g, b;
          AdobeCMYK_to_sRGB1(FXSYS_GetCValue(color), FXSYS_GetMValue(color),
                             FXSYS_GetYValue(color), FXSYS_GetKValue(color), r,
                             g, b);
          m_Color = FXARGB_MAKE(m_Alpha, r, g, b);
          if (m_bRgbByteOrder) {
            m_Color = FXARGB_TOBGRORDERDIB(m_Color);
          } else {
            m_Color = FXARGB_TODIB(m_Color);
          }
          m_Red = r;
          m_Green = g;
          m_Blue = b;
        } else {
          if (m_bRgbByteOrder) {
            m_Color = FXARGB_TOBGRORDERDIB(color);
          } else {
            m_Color = FXARGB_TODIB(color);
          }
          ArgbDecode(color, m_Alpha, m_Red, m_Green, m_Blue);
        }
      }
    }
    if (m_pDevice->GetBPP() == 1) {
      composite_span = &CFX_Renderer::CompositeSpan1bpp;
    }
    return true;
  }
};

int CFX_AggDeviceDriver::GetDriverType() const {
  return 1;
}

bool CFX_AggDeviceDriver::RenderRasterizer(
    agg::rasterizer_scanline_aa& rasterizer,
    uint32_t color,
    bool bFullCover,
    bool bGroupKnockout,
    int alpha_flag,
    void* pIccTransform) {
  CFX_DIBitmap* pt = bGroupKnockout ? m_pOriDevice : nullptr;
  CFX_Renderer render;
  if (!render.Init(m_pBitmap, pt, m_pClipRgn.get(), color, bFullCover,
                   m_bRgbByteOrder, alpha_flag, pIccTransform)) {
    return false;
  }
  agg::scanline_u8 scanline;
  agg::render_scanlines(rasterizer, scanline, render,
                        (m_FillFlags & FXFILL_NOPATHSMOOTH) != 0);
  return true;
}

bool CFX_AggDeviceDriver::DrawPath(const CFX_PathData* pPathData,
                                   const CFX_Matrix* pObject2Device,
                                   const CFX_GraphStateData* pGraphState,
                                   uint32_t fill_color,
                                   uint32_t stroke_color,
                                   int fill_mode,
                                   int blend_type) {
  if (blend_type != FXDIB_BLEND_NORMAL)
    return false;

  if (!GetBuffer())
    return true;

  m_FillFlags = fill_mode;
  if ((fill_mode & 3) && fill_color) {
    CAgg_PathData path_data;
    path_data.BuildPath(pPathData, pObject2Device);
    agg::rasterizer_scanline_aa rasterizer;
    rasterizer.clip_box(0.0f, 0.0f, (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_WIDTH)),
                        (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
    rasterizer.add_path(path_data.m_PathData);
    rasterizer.filling_rule((fill_mode & 3) == FXFILL_WINDING
                                ? agg::fill_non_zero
                                : agg::fill_even_odd);
    if (!RenderRasterizer(rasterizer, fill_color,
                          !!(fill_mode & FXFILL_FULLCOVER), false, 0,
                          nullptr)) {
      return false;
    }
  }
  int stroke_alpha = FXARGB_A(stroke_color);
  if (!pGraphState || !stroke_alpha)
    return true;

  if (fill_mode & FX_ZEROAREA_FILL) {
    CAgg_PathData path_data;
    path_data.BuildPath(pPathData, pObject2Device);
    agg::rasterizer_scanline_aa rasterizer;
    rasterizer.clip_box(0.0f, 0.0f, (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_WIDTH)),
                        (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
    RasterizeStroke(rasterizer, path_data.m_PathData, nullptr, pGraphState, 1,
                    false, !!(fill_mode & FX_STROKE_TEXT_MODE));
    return RenderRasterizer(rasterizer, stroke_color,
                            !!(fill_mode & FXFILL_FULLCOVER), m_bGroupKnockout,
                            0, nullptr);
  }
  CFX_Matrix matrix1;
  CFX_Matrix matrix2;
  if (pObject2Device) {
    matrix1.a =
        std::max(FXSYS_fabs(pObject2Device->a), FXSYS_fabs(pObject2Device->b));
    matrix1.d = matrix1.a;
    matrix2 = CFX_Matrix(
        pObject2Device->a / matrix1.a, pObject2Device->b / matrix1.a,
        pObject2Device->c / matrix1.d, pObject2Device->d / matrix1.d, 0, 0);

    CFX_Matrix mtRervese;
    mtRervese.SetReverse(matrix2);
    matrix1 = *pObject2Device;
    matrix1.Concat(mtRervese);
  }

  CAgg_PathData path_data;
  path_data.BuildPath(pPathData, &matrix1);
  agg::rasterizer_scanline_aa rasterizer;
  rasterizer.clip_box(0.0f, 0.0f, (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_WIDTH)),
                      (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
  RasterizeStroke(rasterizer, path_data.m_PathData, &matrix2, pGraphState,
                  matrix1.a, false, !!(fill_mode & FX_STROKE_TEXT_MODE));
  return RenderRasterizer(rasterizer, stroke_color,
                          !!(fill_mode & FXFILL_FULLCOVER), m_bGroupKnockout, 0,
                          nullptr);
}

bool CFX_AggDeviceDriver::SetPixel(int x, int y, uint32_t color) {
  if (!m_pBitmap->GetBuffer())
    return true;

  if (!m_pClipRgn) {
    if (!m_bRgbByteOrder)
      return DibSetPixel(m_pBitmap, x, y, color, 0, nullptr);
    RgbByteOrderSetPixel(m_pBitmap, x, y, color);
    return true;
  }
  if (!m_pClipRgn->GetBox().Contains(x, y))
    return true;

  if (m_pClipRgn->GetType() == CFX_ClipRgn::RectI) {
    if (!m_bRgbByteOrder)
      return DibSetPixel(m_pBitmap, x, y, color, 0, nullptr);
    RgbByteOrderSetPixel(m_pBitmap, x, y, color);
    return true;
  }
  if (m_pClipRgn->GetType() != CFX_ClipRgn::MaskF)
    return true;

  const CFX_DIBitmap* pMask = m_pClipRgn->GetMask().GetObject();
  int new_alpha = FXARGB_A(color) * pMask->GetScanline(y)[x] / 255;
  color = (color & 0xffffff) | (new_alpha << 24);
  if (m_bRgbByteOrder) {
    RgbByteOrderSetPixel(m_pBitmap, x, y, color);
    return true;
  }
  return DibSetPixel(m_pBitmap, x, y, color, 0, nullptr);
}

bool CFX_AggDeviceDriver::FillRectWithBlend(const FX_RECT* pRect,
                                            uint32_t fill_color,
                                            int blend_type) {
  if (blend_type != FXDIB_BLEND_NORMAL)
    return false;

  if (!m_pBitmap->GetBuffer())
    return true;

  FX_RECT clip_rect;
  GetClipBox(&clip_rect);
  FX_RECT draw_rect = clip_rect;
  if (pRect)
    draw_rect.Intersect(*pRect);
  if (draw_rect.IsEmpty())
    return true;

  if (!m_pClipRgn || m_pClipRgn->GetType() == CFX_ClipRgn::RectI) {
    if (m_bRgbByteOrder) {
      RgbByteOrderCompositeRect(m_pBitmap, draw_rect.left, draw_rect.top,
                                draw_rect.Width(), draw_rect.Height(),
                                fill_color);
    } else {
      m_pBitmap->CompositeRect(draw_rect.left, draw_rect.top, draw_rect.Width(),
                               draw_rect.Height(), fill_color, 0, nullptr);
    }
    return true;
  }
  m_pBitmap->CompositeMask(
      draw_rect.left, draw_rect.top, draw_rect.Width(), draw_rect.Height(),
      m_pClipRgn->GetMask().GetObject(), fill_color,
      draw_rect.left - clip_rect.left, draw_rect.top - clip_rect.top,
      FXDIB_BLEND_NORMAL, nullptr, m_bRgbByteOrder, 0, nullptr);
  return true;
}

bool CFX_AggDeviceDriver::GetClipBox(FX_RECT* pRect) {
  if (!m_pClipRgn) {
    pRect->left = pRect->top = 0;
    pRect->right = GetDeviceCaps(FXDC_PIXEL_WIDTH);
    pRect->bottom = GetDeviceCaps(FXDC_PIXEL_HEIGHT);
    return true;
  }
  *pRect = m_pClipRgn->GetBox();
  return true;
}

bool CFX_AggDeviceDriver::GetDIBits(CFX_DIBitmap* pBitmap, int left, int top) {
  if (!m_pBitmap || !m_pBitmap->GetBuffer())
    return true;

  FX_RECT rect(left, top, left + pBitmap->GetWidth(),
               top + pBitmap->GetHeight());
  std::unique_ptr<CFX_DIBitmap> pBack;
  if (m_pOriDevice) {
    pBack = m_pOriDevice->Clone(&rect);
    if (!pBack)
      return true;

    pBack->CompositeBitmap(0, 0, pBack->GetWidth(), pBack->GetHeight(),
                           m_pBitmap, 0, 0);
  } else {
    pBack = m_pBitmap->Clone(&rect);
    if (!pBack)
      return true;
  }

  left = std::min(left, 0);
  top = std::min(top, 0);
  if (m_bRgbByteOrder) {
    RgbByteOrderTransferBitmap(pBitmap, 0, 0, rect.Width(), rect.Height(),
                               pBack.get(), left, top);
    return true;
  }
  return pBitmap->TransferBitmap(0, 0, rect.Width(), rect.Height(), pBack.get(),
                                 left, top);
}

CFX_DIBitmap* CFX_AggDeviceDriver::GetBackDrop() {
  return m_pOriDevice;
}

bool CFX_AggDeviceDriver::SetDIBits(const CFX_DIBSource* pBitmap,
                                    uint32_t argb,
                                    const FX_RECT* pSrcRect,
                                    int left,
                                    int top,
                                    int blend_type) {
  if (!m_pBitmap->GetBuffer())
    return true;

  if (pBitmap->IsAlphaMask()) {
    return m_pBitmap->CompositeMask(
        left, top, pSrcRect->Width(), pSrcRect->Height(), pBitmap, argb,
        pSrcRect->left, pSrcRect->top, blend_type, m_pClipRgn.get(),
        m_bRgbByteOrder, 0, nullptr);
  }
  return m_pBitmap->CompositeBitmap(
      left, top, pSrcRect->Width(), pSrcRect->Height(), pBitmap, pSrcRect->left,
      pSrcRect->top, blend_type, m_pClipRgn.get(), m_bRgbByteOrder, nullptr);
}

bool CFX_AggDeviceDriver::StretchDIBits(const CFX_DIBSource* pSource,
                                        uint32_t argb,
                                        int dest_left,
                                        int dest_top,
                                        int dest_width,
                                        int dest_height,
                                        const FX_RECT* pClipRect,
                                        uint32_t flags,
                                        int blend_type) {
  if (!m_pBitmap->GetBuffer())
    return true;

  if (dest_width == pSource->GetWidth() &&
      dest_height == pSource->GetHeight()) {
    FX_RECT rect(0, 0, dest_width, dest_height);
    return SetDIBits(pSource, argb, &rect, dest_left, dest_top, blend_type);
  }
  FX_RECT dest_rect(dest_left, dest_top, dest_left + dest_width,
                    dest_top + dest_height);
  dest_rect.Normalize();
  FX_RECT dest_clip = dest_rect;
  dest_clip.Intersect(*pClipRect);
  CFX_BitmapComposer composer;
  composer.Compose(m_pBitmap, m_pClipRgn.get(), 255, argb, dest_clip, false,
                   false, false, m_bRgbByteOrder, 0, nullptr, blend_type);
  dest_clip.Offset(-dest_rect.left, -dest_rect.top);
  CFX_ImageStretcher stretcher(&composer, pSource, dest_width, dest_height,
                               dest_clip, flags);
  if (stretcher.Start())
    stretcher.Continue(nullptr);
  return true;
}

bool CFX_AggDeviceDriver::StartDIBits(const CFX_DIBSource* pSource,
                                      int bitmap_alpha,
                                      uint32_t argb,
                                      const CFX_Matrix* pMatrix,
                                      uint32_t render_flags,
                                      void*& handle,
                                      int blend_type) {
  if (!m_pBitmap->GetBuffer())
    return true;

  CFX_ImageRenderer* pRenderer = new CFX_ImageRenderer;
  pRenderer->Start(m_pBitmap, m_pClipRgn.get(), pSource, bitmap_alpha, argb,
                   pMatrix, render_flags, m_bRgbByteOrder, 0, nullptr);
  handle = pRenderer;
  return true;
}

bool CFX_AggDeviceDriver::ContinueDIBits(void* pHandle, IFX_Pause* pPause) {
  if (!m_pBitmap->GetBuffer()) {
    return true;
  }
  return ((CFX_ImageRenderer*)pHandle)->Continue(pPause);
}

void CFX_AggDeviceDriver::CancelDIBits(void* pHandle) {
  if (!m_pBitmap->GetBuffer()) {
    return;
  }
  delete (CFX_ImageRenderer*)pHandle;
}

#ifndef _SKIA_SUPPORT_
CFX_FxgeDevice::CFX_FxgeDevice() {
  m_bOwnedBitmap = false;
}

bool CFX_FxgeDevice::Attach(CFX_DIBitmap* pBitmap,
                            bool bRgbByteOrder,
                            CFX_DIBitmap* pOriDevice,
                            bool bGroupKnockout) {
  if (!pBitmap)
    return false;

  SetBitmap(pBitmap);
  SetDeviceDriver(pdfium::MakeUnique<CFX_AggDeviceDriver>(
      pBitmap, bRgbByteOrder, pOriDevice, bGroupKnockout));
  return true;
}

bool CFX_FxgeDevice::Create(int width,
                            int height,
                            FXDIB_Format format,
                            CFX_DIBitmap* pOriDevice) {
  m_bOwnedBitmap = true;
  CFX_DIBitmap* pBitmap = new CFX_DIBitmap;
  if (!pBitmap->Create(width, height, format)) {
    delete pBitmap;
    return false;
  }
  SetBitmap(pBitmap);
  SetDeviceDriver(pdfium::MakeUnique<CFX_AggDeviceDriver>(pBitmap, false,
                                                          pOriDevice, false));
  return true;
}

CFX_FxgeDevice::~CFX_FxgeDevice() {
  if (m_bOwnedBitmap) {
    delete GetBitmap();
  }
}
#endif
