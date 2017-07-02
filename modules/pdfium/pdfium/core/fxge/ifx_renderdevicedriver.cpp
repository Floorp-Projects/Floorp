// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/ifx_renderdevicedriver.h"

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"

IFX_RenderDeviceDriver::~IFX_RenderDeviceDriver() {}

CFX_Matrix IFX_RenderDeviceDriver::GetCTM() const {
  return CFX_Matrix();
}

bool IFX_RenderDeviceDriver::StartRendering() {
  return true;
}

void IFX_RenderDeviceDriver::EndRendering() {}

bool IFX_RenderDeviceDriver::SetClip_PathStroke(
    const CFX_PathData* pPathData,
    const CFX_Matrix* pObject2Device,
    const CFX_GraphStateData* pGraphState) {
  return false;
}

bool IFX_RenderDeviceDriver::SetPixel(int x, int y, uint32_t color) {
  return false;
}

bool IFX_RenderDeviceDriver::FillRectWithBlend(const FX_RECT* pRect,
                                               uint32_t fill_color,
                                               int blend_type) {
  return false;
}

bool IFX_RenderDeviceDriver::DrawCosmeticLine(FX_FLOAT x1,
                                              FX_FLOAT y1,
                                              FX_FLOAT x2,
                                              FX_FLOAT y2,
                                              uint32_t color,
                                              int blend_type) {
  return false;
}

bool IFX_RenderDeviceDriver::GetDIBits(CFX_DIBitmap* pBitmap,
                                       int left,
                                       int top) {
  return false;
}

CFX_DIBitmap* IFX_RenderDeviceDriver::GetBackDrop() {
  return nullptr;
}

bool IFX_RenderDeviceDriver::ContinueDIBits(void* handle, IFX_Pause* pPause) {
  return false;
}

void IFX_RenderDeviceDriver::CancelDIBits(void* handle) {}

bool IFX_RenderDeviceDriver::DrawDeviceText(int nChars,
                                            const FXTEXT_CHARPOS* pCharPos,
                                            CFX_Font* pFont,
                                            const CFX_Matrix* pObject2Device,
                                            FX_FLOAT font_size,
                                            uint32_t color) {
  return false;
}

void* IFX_RenderDeviceDriver::GetPlatformSurface() const {
  return nullptr;
}

int IFX_RenderDeviceDriver::GetDriverType() const {
  return 0;
}

void IFX_RenderDeviceDriver::ClearDriver() {}

bool IFX_RenderDeviceDriver::DrawShading(const CPDF_ShadingPattern* pPattern,
                                         const CFX_Matrix* pMatrix,
                                         const FX_RECT& clip_rect,
                                         int alpha,
                                         bool bAlphaMode) {
  return false;
}

bool IFX_RenderDeviceDriver::SetBitsWithMask(const CFX_DIBSource* pBitmap,
                                             const CFX_DIBSource* pMask,
                                             int left,
                                             int top,
                                             int bitmap_alpha,
                                             int blend_type) {
  return false;
}

#if defined _SKIA_SUPPORT_ || _SKIA_SUPPORT_PATHS_
void IFX_RenderDeviceDriver::Flush() {}
#endif
