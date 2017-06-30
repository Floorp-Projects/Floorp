// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_renderoptions.h"

CPDF_RenderOptions::CPDF_RenderOptions()
    : m_ColorMode(RENDER_COLOR_NORMAL),
      m_Flags(RENDER_CLEARTYPE),
      m_Interpolation(0),
      m_AddFlags(0),
      m_dwLimitCacheSize(1024 * 1024 * 100),
      m_HalftoneLimit(-1),
      m_bDrawAnnots(false) {}

CPDF_RenderOptions::CPDF_RenderOptions(const CPDF_RenderOptions& rhs)
    : m_ColorMode(rhs.m_ColorMode),
      m_BackColor(rhs.m_BackColor),
      m_ForeColor(rhs.m_ForeColor),
      m_Flags(rhs.m_Flags),
      m_Interpolation(rhs.m_Interpolation),
      m_AddFlags(rhs.m_AddFlags),
      m_dwLimitCacheSize(rhs.m_dwLimitCacheSize),
      m_HalftoneLimit(rhs.m_HalftoneLimit),
      m_bDrawAnnots(rhs.m_bDrawAnnots),
      m_pOCContext(rhs.m_pOCContext) {}

CPDF_RenderOptions::~CPDF_RenderOptions() {}

FX_ARGB CPDF_RenderOptions::TranslateColor(FX_ARGB argb) const {
  if (m_ColorMode == RENDER_COLOR_NORMAL)
    return argb;

  if (m_ColorMode == RENDER_COLOR_ALPHA)
    return argb;

  int a, r, g, b;
  ArgbDecode(argb, a, r, g, b);
  int gray = FXRGB2GRAY(r, g, b);
  if (m_ColorMode == RENDER_COLOR_TWOCOLOR) {
    int color = (r - gray) * (r - gray) + (g - gray) * (g - gray) +
                (b - gray) * (b - gray);
    if (gray < 35 && color < 20)
      return ArgbEncode(a, m_ForeColor);

    if (gray > 221 && color < 20)
      return ArgbEncode(a, m_BackColor);

    return argb;
  }
  int fr = FXSYS_GetRValue(m_ForeColor);
  int fg = FXSYS_GetGValue(m_ForeColor);
  int fb = FXSYS_GetBValue(m_ForeColor);
  int br = FXSYS_GetRValue(m_BackColor);
  int bg = FXSYS_GetGValue(m_BackColor);
  int bb = FXSYS_GetBValue(m_BackColor);
  r = (br - fr) * gray / 255 + fr;
  g = (bg - fg) * gray / 255 + fg;
  b = (bb - fb) * gray / 255 + fb;
  return ArgbEncode(a, r, g, b);
}
