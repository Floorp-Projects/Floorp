// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_DEVICEBUFFER_H_
#define CORE_FPDFAPI_RENDER_CPDF_DEVICEBUFFER_H_

#include <memory>

#include "core/fxcrt/fx_coordinates.h"

class CFX_DIBitmap;
class CFX_RenderDevice;
class CPDF_PageObject;
class CPDF_RenderContext;

class CPDF_DeviceBuffer {
 public:
  CPDF_DeviceBuffer();
  ~CPDF_DeviceBuffer();
  bool Initialize(CPDF_RenderContext* pContext,
                  CFX_RenderDevice* pDevice,
                  FX_RECT* pRect,
                  const CPDF_PageObject* pObj,
                  int max_dpi);
  void OutputToDevice();
  CFX_DIBitmap* GetBitmap() const { return m_pBitmap.get(); }
  const CFX_Matrix* GetMatrix() const { return &m_Matrix; }

 private:
  CFX_RenderDevice* m_pDevice;
  CPDF_RenderContext* m_pContext;
  FX_RECT m_Rect;
  const CPDF_PageObject* m_pObject;
  std::unique_ptr<CFX_DIBitmap> m_pBitmap;
  CFX_Matrix m_Matrix;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_DEVICEBUFFER_H_
