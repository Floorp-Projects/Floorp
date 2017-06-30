// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCODEC_JBIG2_JBIG2_PAGE_H_
#define CORE_FXCODEC_JBIG2_JBIG2_PAGE_H_

#include "core/fxcrt/fx_system.h"

struct JBig2PageInfo {
  uint32_t m_dwWidth;
  uint32_t m_dwHeight;
  uint32_t m_dwResolutionX;
  uint32_t m_dwResolutionY;
  uint8_t m_cFlags;
  bool m_bIsStriped;
  uint16_t m_wMaxStripeSize;
};

#endif  // CORE_FXCODEC_JBIG2_JBIG2_PAGE_H_
