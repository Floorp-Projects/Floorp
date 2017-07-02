// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_WORD_H_
#define CORE_FPDFDOC_CPVT_WORD_H_

#include "core/fpdfdoc/cpvt_wordplace.h"
#include "core/fpdfdoc/cpvt_wordprops.h"
#include "core/fxcrt/fx_system.h"

class CPVT_Word {
 public:
  CPVT_Word();

  uint16_t Word;
  int32_t nCharset;
  CPVT_WordPlace WordPlace;
  CFX_PointF ptWord;
  FX_FLOAT fAscent;
  FX_FLOAT fDescent;
  FX_FLOAT fWidth;
  int32_t nFontIndex;
  FX_FLOAT fFontSize;
  CPVT_WordProps WordProps;
};

inline CPVT_Word::CPVT_Word()
    : Word(0),
      nCharset(0),
      fAscent(0.0f),
      fDescent(0.0f),
      fWidth(0.0f),
      nFontIndex(-1),
      fFontSize(0.0f) {}

#endif  // CORE_FPDFDOC_CPVT_WORD_H_
