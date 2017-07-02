// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_RENDER_CPDF_RENDERCONTEXT_H_
#define CORE_FPDFAPI_RENDER_CPDF_RENDERCONTEXT_H_

#include <vector>

#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_coordinates.h"

class CPDF_Dictionary;
class CPDF_Document;
class CPDF_Page;
class CPDF_PageObject;
class CPDF_PageObjectHolder;
class CPDF_PageRenderCache;
class CPDF_RenderOptions;
class CFX_DIBitmap;
class CFX_Matrix;
class CFX_RenderDevice;

class CPDF_RenderContext {
 public:
  class Layer {
   public:
    CPDF_PageObjectHolder* m_pObjectHolder;
    CFX_Matrix m_Matrix;
  };

  explicit CPDF_RenderContext(CPDF_Page* pPage);
  CPDF_RenderContext(CPDF_Document* pDoc, CPDF_PageRenderCache* pPageCache);
  ~CPDF_RenderContext();

  void AppendLayer(CPDF_PageObjectHolder* pObjectHolder,
                   const CFX_Matrix* pObject2Device);

  void Render(CFX_RenderDevice* pDevice,
              const CPDF_RenderOptions* pOptions,
              const CFX_Matrix* pFinalMatrix);

  void Render(CFX_RenderDevice* pDevice,
              const CPDF_PageObject* pStopObj,
              const CPDF_RenderOptions* pOptions,
              const CFX_Matrix* pFinalMatrix);

  void GetBackground(CFX_DIBitmap* pBuffer,
                     const CPDF_PageObject* pObj,
                     const CPDF_RenderOptions* pOptions,
                     CFX_Matrix* pFinalMatrix);

  size_t CountLayers() const { return m_Layers.size(); }
  Layer* GetLayer(uint32_t index) { return &m_Layers[index]; }

  CPDF_Document* GetDocument() const { return m_pDocument; }
  CPDF_Dictionary* GetPageResources() const { return m_pPageResources; }
  CPDF_PageRenderCache* GetPageCache() const { return m_pPageCache; }

 protected:
  CPDF_Document* const m_pDocument;
  CPDF_Dictionary* m_pPageResources;
  CPDF_PageRenderCache* m_pPageCache;
  std::vector<Layer> m_Layers;
};

#endif  // CORE_FPDFAPI_RENDER_CPDF_RENDERCONTEXT_H_
