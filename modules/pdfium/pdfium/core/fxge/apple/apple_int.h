// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_APPLE_APPLE_INT_H_
#define CORE_FXGE_APPLE_APPLE_INT_H_

#include "core/fxcrt/fx_system.h"

#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "core/fxge/fx_dib.h"
#include "core/fxge/ifx_renderdevicedriver.h"

#include <Carbon/Carbon.h>

class CQuartz2D {
 public:
  void* createGraphics(CFX_DIBitmap* bitmap);
  void destroyGraphics(void* graphics);

  void* CreateFont(const uint8_t* pFontData, uint32_t dwFontSize);
  void DestroyFont(void* pFont);
  void setGraphicsTextMatrix(void* graphics, CFX_Matrix* matrix);
  bool drawGraphicsString(void* graphics,
                          void* font,
                          FX_FLOAT fontSize,
                          uint16_t* glyphIndices,
                          CGPoint* glyphPositions,
                          int32_t chars,
                          FX_ARGB argb,
                          CFX_Matrix* matrix = nullptr);
  void saveGraphicsState(void* graphics);
  void restoreGraphicsState(void* graphics);
};

class CApplePlatform {
 public:
  CApplePlatform() {}
  ~CApplePlatform() {}

  CQuartz2D m_quartz2d;
};

#endif  // CORE_FXGE_APPLE_APPLE_INT_H_
