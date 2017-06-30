// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_IMAGELOADER_H_
#define CORE_FPDFAPI_RENDER_CPDF_IMAGELOADER_H_

#include <memory>

#include "core/fxcrt/fx_basic.h"
#include "core/fxge/fx_dib.h"

class CPDF_ImageObject;
class CPDF_PageRenderCache;
class CPDF_RenderStatus;

class CPDF_ImageLoader {
 public:
  CPDF_ImageLoader();
  ~CPDF_ImageLoader();

  bool Start(const CPDF_ImageObject* pImage,
             CPDF_PageRenderCache* pCache,
             bool bStdCS,
             uint32_t GroupFamily,
             bool bLoadMask,
             CPDF_RenderStatus* pRenderStatus,
             int32_t nDownsampleWidth,
             int32_t nDownsampleHeight);
  bool Continue(IFX_Pause* pPause);

  CFX_DIBSource* m_pBitmap;
  CFX_DIBSource* m_pMask;
  uint32_t m_MatteColor;
  bool m_bCached;

 private:
  void HandleFailure();

  int32_t m_nDownsampleWidth;
  int32_t m_nDownsampleHeight;
  CPDF_PageRenderCache* m_pCache;
  CPDF_ImageObject* m_pImage;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_IMAGELOADER_H_
