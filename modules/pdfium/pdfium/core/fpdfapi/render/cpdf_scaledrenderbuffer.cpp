// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_scaledrenderbuffer.h"

#include "core/fpdfapi/render/cpdf_rendercontext.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_renderdevice.h"
#include "third_party/base/ptr_util.h"

#define _FPDFAPI_IMAGESIZE_LIMIT_ (30 * 1024 * 1024)

CPDF_ScaledRenderBuffer::CPDF_ScaledRenderBuffer() {}

CPDF_ScaledRenderBuffer::~CPDF_ScaledRenderBuffer() {}

bool CPDF_ScaledRenderBuffer::Initialize(CPDF_RenderContext* pContext,
                                         CFX_RenderDevice* pDevice,
                                         const FX_RECT& pRect,
                                         const CPDF_PageObject* pObj,
                                         const CPDF_RenderOptions* pOptions,
                                         int max_dpi) {
  m_pDevice = pDevice;
  if (m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_GET_BITS)
    return true;

  m_pContext = pContext;
  m_Rect = pRect;
  m_pObject = pObj;
  m_Matrix.Translate(-pRect.left, -pRect.top);
  int horz_size = pDevice->GetDeviceCaps(FXDC_HORZ_SIZE);
  int vert_size = pDevice->GetDeviceCaps(FXDC_VERT_SIZE);
  if (horz_size && vert_size && max_dpi) {
    int dpih =
        pDevice->GetDeviceCaps(FXDC_PIXEL_WIDTH) * 254 / (horz_size * 10);
    int dpiv =
        pDevice->GetDeviceCaps(FXDC_PIXEL_HEIGHT) * 254 / (vert_size * 10);
    if (dpih > max_dpi)
      m_Matrix.Scale((FX_FLOAT)(max_dpi) / dpih, 1.0f);
    if (dpiv > max_dpi)
      m_Matrix.Scale(1.0f, (FX_FLOAT)(max_dpi) / (FX_FLOAT)dpiv);
  }
  m_pBitmapDevice = pdfium::MakeUnique<CFX_FxgeDevice>();
  FXDIB_Format dibFormat = FXDIB_Rgb;
  int32_t bpp = 24;
  if (m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_ALPHA_OUTPUT) {
    dibFormat = FXDIB_Argb;
    bpp = 32;
  }
  while (1) {
    CFX_FloatRect rect(pRect);
    m_Matrix.TransformRect(rect);
    FX_RECT bitmap_rect = rect.GetOuterRect();
    int32_t iWidth = bitmap_rect.Width();
    int32_t iHeight = bitmap_rect.Height();
    int32_t iPitch = (iWidth * bpp + 31) / 32 * 4;
    if (iWidth * iHeight < 1)
      return false;

    if (iPitch * iHeight <= _FPDFAPI_IMAGESIZE_LIMIT_ &&
        m_pBitmapDevice->Create(iWidth, iHeight, dibFormat, nullptr)) {
      break;
    }
    m_Matrix.Scale(0.5f, 0.5f);
  }
  m_pContext->GetBackground(m_pBitmapDevice->GetBitmap(), m_pObject, pOptions,
                            &m_Matrix);
  return true;
}

void CPDF_ScaledRenderBuffer::OutputToDevice() {
  if (m_pBitmapDevice) {
    m_pDevice->StretchDIBits(m_pBitmapDevice->GetBitmap(), m_Rect.left,
                             m_Rect.top, m_Rect.Width(), m_Rect.Height());
  }
}
