// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_rendercontext.h"

#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/cpdf_pageobjectholder.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_pagerendercache.h"
#include "core/fpdfapi/render/cpdf_progressiverenderer.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfapi/render/cpdf_renderstatus.h"
#include "core/fpdfapi/render/cpdf_textrenderer.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/fx_dib.h"

CPDF_RenderContext::CPDF_RenderContext(CPDF_Page* pPage)
    : m_pDocument(pPage->m_pDocument),
      m_pPageResources(pPage->m_pPageResources),
      m_pPageCache(pPage->GetRenderCache()) {}

CPDF_RenderContext::CPDF_RenderContext(CPDF_Document* pDoc,
                                       CPDF_PageRenderCache* pPageCache)
    : m_pDocument(pDoc), m_pPageResources(nullptr), m_pPageCache(pPageCache) {}

CPDF_RenderContext::~CPDF_RenderContext() {}

void CPDF_RenderContext::GetBackground(CFX_DIBitmap* pBuffer,
                                       const CPDF_PageObject* pObj,
                                       const CPDF_RenderOptions* pOptions,
                                       CFX_Matrix* pFinalMatrix) {
  CFX_FxgeDevice device;
  device.Attach(pBuffer, false, nullptr, false);

  FX_RECT rect(0, 0, device.GetWidth(), device.GetHeight());
  device.FillRect(&rect, 0xffffffff);
  Render(&device, pObj, pOptions, pFinalMatrix);
}

void CPDF_RenderContext::AppendLayer(CPDF_PageObjectHolder* pObjectHolder,
                                     const CFX_Matrix* pObject2Device) {
  m_Layers.emplace_back();
  m_Layers.back().m_pObjectHolder = pObjectHolder;
  if (pObject2Device)
    m_Layers.back().m_Matrix = *pObject2Device;
  else
    m_Layers.back().m_Matrix.SetIdentity();
}

void CPDF_RenderContext::Render(CFX_RenderDevice* pDevice,
                                const CPDF_RenderOptions* pOptions,
                                const CFX_Matrix* pLastMatrix) {
  Render(pDevice, nullptr, pOptions, pLastMatrix);
}

void CPDF_RenderContext::Render(CFX_RenderDevice* pDevice,
                                const CPDF_PageObject* pStopObj,
                                const CPDF_RenderOptions* pOptions,
                                const CFX_Matrix* pLastMatrix) {
  for (auto& layer : m_Layers) {
    pDevice->SaveState();
    if (pLastMatrix) {
      CFX_Matrix FinalMatrix = layer.m_Matrix;
      FinalMatrix.Concat(*pLastMatrix);
      CPDF_RenderStatus status;
      status.Initialize(this, pDevice, pLastMatrix, pStopObj, nullptr, nullptr,
                        pOptions, layer.m_pObjectHolder->m_Transparency, false,
                        nullptr);
      status.RenderObjectList(layer.m_pObjectHolder, &FinalMatrix);
      if (status.m_Options.m_Flags & RENDER_LIMITEDIMAGECACHE)
        m_pPageCache->CacheOptimization(status.m_Options.m_dwLimitCacheSize);
      if (status.m_bStopped) {
        pDevice->RestoreState(false);
        break;
      }
    } else {
      CPDF_RenderStatus status;
      status.Initialize(this, pDevice, nullptr, pStopObj, nullptr, nullptr,
                        pOptions, layer.m_pObjectHolder->m_Transparency, false,
                        nullptr);
      status.RenderObjectList(layer.m_pObjectHolder, &layer.m_Matrix);
      if (status.m_Options.m_Flags & RENDER_LIMITEDIMAGECACHE)
        m_pPageCache->CacheOptimization(status.m_Options.m_dwLimitCacheSize);
      if (status.m_bStopped) {
        pDevice->RestoreState(false);
        break;
      }
    }
    pDevice->RestoreState(false);
  }
}
