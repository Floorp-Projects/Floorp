// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_SCALEDRENDERBUFFER_H_
#define CORE_FPDFAPI_RENDER_CPDF_SCALEDRENDERBUFFER_H_

#include <memory>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxge/cfx_fxgedevice.h"

class CFX_RenderDevice;
class CPDF_PageObject;
class CPDF_RenderContext;
class CPDF_RenderOptions;

class CPDF_ScaledRenderBuffer {
 public:
  CPDF_ScaledRenderBuffer();
  ~CPDF_ScaledRenderBuffer();

  bool Initialize(CPDF_RenderContext* pContext,
                  CFX_RenderDevice* pDevice,
                  const FX_RECT& pRect,
                  const CPDF_PageObject* pObj,
                  const CPDF_RenderOptions* pOptions,
                  int max_dpi);
  CFX_RenderDevice* GetDevice() {
    return m_pBitmapDevice ? m_pBitmapDevice.get() : m_pDevice;
  }
  CFX_Matrix* GetMatrix() { return &m_Matrix; }
  void OutputToDevice();

 private:
  CFX_RenderDevice* m_pDevice;
  CPDF_RenderContext* m_pContext;
  FX_RECT m_Rect;
  const CPDF_PageObject* m_pObject;
  std::unique_ptr<CFX_FxgeDevice> m_pBitmapDevice;
  CFX_Matrix m_Matrix;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_SCALEDRENDERBUFFER_H_
