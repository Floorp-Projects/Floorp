// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcodec/jbig2/JBig2_PatternDict.h"

#include "core/fxcrt/fx_memory.h"

CJBig2_PatternDict::CJBig2_PatternDict() {
  NUMPATS = 0;
  HDPATS = nullptr;
}

CJBig2_PatternDict::~CJBig2_PatternDict() {
  if (HDPATS) {
    for (uint32_t i = 0; i < NUMPATS; i++) {
      delete HDPATS[i];
    }
    FX_Free(HDPATS);
  }
}
