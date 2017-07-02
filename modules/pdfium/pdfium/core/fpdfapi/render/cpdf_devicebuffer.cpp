// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_devicebuffer.h"

#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/render/cpdf_rendercontext.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/fx_dib.h"
#include "third_party/base/ptr_util.h"

CPDF_DeviceBuffer::CPDF_DeviceBuffer()
    : m_pDevice(nullptr), m_pContext(nullptr), m_pObject(nullptr) {}

CPDF_DeviceBuffer::~CPDF_DeviceBuffer() {}

bool CPDF_DeviceBuffer::Initialize(CPDF_RenderContext* pContext,
                                   CFX_RenderDevice* pDevice,
                                   FX_RECT* pRect,
                                   const CPDF_PageObject* pObj,
                                   int max_dpi) {
  m_pDevice = pDevice;
  m_pContext = pContext;
  m_Rect = *pRect;
  m_pObject = pObj;
  m_Matrix.Translate(-pRect->left, -pRect->top);
#if _FXM_PLATFORM_ != _FXM_PLATFORM_APPLE_
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
#endif
  CFX_Matrix ctm = m_pDevice->GetCTM();
  m_Matrix.Concat(CFX_Matrix(FXSYS_fabs(ctm.a), 0, 0, FXSYS_fabs(ctm.d), 0, 0));

  CFX_FloatRect rect(*pRect);
  m_Matrix.TransformRect(rect);

  FX_RECT bitmap_rect = rect.GetOuterRect();
  m_pBitmap = pdfium::MakeUnique<CFX_DIBitmap>();
  m_pBitmap->Create(bitmap_rect.Width(), bitmap_rect.Height(), FXDIB_Argb);
  return true;
}

void CPDF_DeviceBuffer::OutputToDevice() {
  if (m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_GET_BITS) {
    if (m_Matrix.a == 1.0f && m_Matrix.d == 1.0f) {
      m_pDevice->SetDIBits(m_pBitmap.get(), m_Rect.left, m_Rect.top);
    } else {
      m_pDevice->StretchDIBits(m_pBitmap.get(), m_Rect.left, m_Rect.top,
                               m_Rect.Width(), m_Rect.Height());
    }
    return;
  }
  CFX_DIBitmap buffer;
  m_pDevice->CreateCompatibleBitmap(&buffer, m_pBitmap->GetWidth(),
                                    m_pBitmap->GetHeight());
  m_pContext->GetBackground(&buffer, m_pObject, nullptr, &m_Matrix);
  buffer.CompositeBitmap(0, 0, buffer.GetWidth(), buffer.GetHeight(),
                         m_pBitmap.get(), 0, 0);
  m_pDevice->StretchDIBits(&buffer, m_Rect.left, m_Rect.top, m_Rect.Width(),
                           m_Rect.Height());
}
