// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_RENDEROPTIONS_H_
#define CORE_FPDFAPI_RENDER_CPDF_RENDEROPTIONS_H_

#include "core/fpdfdoc/cpdf_occontext.h"
#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_dib.h"

#define RENDER_COLOR_NORMAL 0
#define RENDER_COLOR_GRAY 1
#define RENDER_COLOR_TWOCOLOR 2
#define RENDER_COLOR_ALPHA 3
#define RENDER_CLEARTYPE 0x00000001
#define RENDER_PRINTGRAPHICTEXT 0x00000002
#define RENDER_FORCE_DOWNSAMPLE 0x00000004
#define RENDER_PRINTPREVIEW 0x00000008
#define RENDER_BGR_STRIPE 0x00000010
#define RENDER_NO_NATIVETEXT 0x00000020
#define RENDER_FORCE_HALFTONE 0x00000040
#define RENDER_RECT_AA 0x00000080
#define RENDER_FILL_FULLCOVER 0x00000100
#define RENDER_PRINTIMAGETEXT 0x00000200
#define RENDER_OVERPRINT 0x00000400
#define RENDER_THINLINE 0x00000800
#define RENDER_NOTEXTSMOOTH 0x10000000
#define RENDER_NOPATHSMOOTH 0x20000000
#define RENDER_NOIMAGESMOOTH 0x40000000
#define RENDER_LIMITEDIMAGECACHE 0x80000000

class CPDF_RenderOptions {
 public:
  CPDF_RenderOptions();
  CPDF_RenderOptions(const CPDF_RenderOptions& rhs);
  ~CPDF_RenderOptions();

  FX_ARGB TranslateColor(FX_ARGB argb) const;

  int m_ColorMode;
  FX_COLORREF m_BackColor;
  FX_COLORREF m_ForeColor;
  uint32_t m_Flags;
  int m_Interpolation;
  uint32_t m_AddFlags;
  uint32_t m_dwLimitCacheSize;
  int m_HalftoneLimit;
  bool m_bDrawAnnots;
  CFX_RetainPtr<CPDF_OCContext> m_pOCContext;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_RENDEROPTIONS_H_
