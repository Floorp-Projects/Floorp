// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_IMAGERENDERER_H_
#define CORE_FPDFAPI_RENDER_CPDF_IMAGERENDERER_H_

#include <memory>

#include "core/fpdfapi/render/cpdf_imageloader.h"

class CFX_FxgeDevice;
class CFX_ImageTransformer;
class CPDF_ImageObject;
class CPDF_PageObject;
class CPDF_Pattern;
class CPDF_RenderStatus;

class CPDF_ImageRenderer {
 public:
  CPDF_ImageRenderer();
  ~CPDF_ImageRenderer();

  bool Start(CPDF_RenderStatus* pStatus,
             CPDF_PageObject* pObj,
             const CFX_Matrix* pObj2Device,
             bool bStdCS,
             int blendType);

  bool Start(CPDF_RenderStatus* pStatus,
             const CFX_DIBSource* pDIBSource,
             FX_ARGB bitmap_argb,
             int bitmap_alpha,
             const CFX_Matrix* pImage2Device,
             uint32_t flags,
             bool bStdCS,
             int blendType);

  bool Continue(IFX_Pause* pPause);
  bool GetResult() const { return m_Result; }

 private:
  bool StartBitmapAlpha();
  bool StartDIBSource();
  bool StartRenderDIBSource();
  bool StartLoadDIBSource();
  bool DrawMaskedImage();
  bool DrawPatternImage(const CFX_Matrix* pObj2Device);
  bool NotDrawing() const;
  FX_RECT GetDrawRect() const;
  CFX_Matrix GetDrawMatrix(const FX_RECT& rect) const;
  void CalculateDrawImage(CFX_FxgeDevice* bitmap_device1,
                          CFX_FxgeDevice* bitmap_device2,
                          const CFX_DIBSource* pDIBSource,
                          CFX_Matrix* pNewMatrix,
                          const FX_RECT& rect) const;

  CPDF_RenderStatus* m_pRenderStatus;
  CPDF_ImageObject* m_pImageObject;
  int m_Status;
  const CFX_Matrix* m_pObj2Device;
  CFX_Matrix m_ImageMatrix;
  CPDF_ImageLoader m_Loader;
  const CFX_DIBSource* m_pDIBSource;
  std::unique_ptr<CFX_DIBitmap> m_pClone;
  int m_BitmapAlpha;
  bool m_bPatternColor;
  CPDF_Pattern* m_pPattern;
  FX_ARGB m_FillArgb;
  uint32_t m_Flags;
  std::unique_ptr<CFX_ImageTransformer> m_pTransformer;
  void* m_DeviceHandle;
  bool m_bStdCS;
  int m_BlendType;
  bool m_Result;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_IMAGERENDERER_H_
