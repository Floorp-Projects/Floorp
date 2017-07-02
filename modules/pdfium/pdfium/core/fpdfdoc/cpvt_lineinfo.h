// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_LINEINFO_H_
#define CORE_FPDFDOC_CPVT_LINEINFO_H_

#include "core/fxcrt/fx_system.h"

class CPVT_LineInfo {
 public:
  CPVT_LineInfo();

  int32_t nTotalWord;
  int32_t nBeginWordIndex;
  int32_t nEndWordIndex;
  FX_FLOAT fLineX;
  FX_FLOAT fLineY;
  FX_FLOAT fLineWidth;
  FX_FLOAT fLineAscent;
  FX_FLOAT fLineDescent;
};

inline CPVT_LineInfo::CPVT_LineInfo()
    : nTotalWord(0),
      nBeginWordIndex(-1),
      nEndWordIndex(-1),
      fLineX(0.0f),
      fLineY(0.0f),
      fLineWidth(0.0f),
      fLineAscent(0.0f),
      fLineDescent(0.0f) {}

#endif  // CORE_FPDFDOC_CPVT_LINEINFO_H_
