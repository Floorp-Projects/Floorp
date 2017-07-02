// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_WORDPLACE_H_
#define CORE_FPDFDOC_CPVT_WORDPLACE_H_

#include "core/fxcrt/fx_system.h"

struct CPVT_WordPlace {
  CPVT_WordPlace() : nSecIndex(-1), nLineIndex(-1), nWordIndex(-1) {}

  CPVT_WordPlace(int32_t other_nSecIndex,
                 int32_t other_nLineIndex,
                 int32_t other_nWordIndex) {
    nSecIndex = other_nSecIndex;
    nLineIndex = other_nLineIndex;
    nWordIndex = other_nWordIndex;
  }

  void Default() { nSecIndex = nLineIndex = nWordIndex = -1; }

  bool operator==(const CPVT_WordPlace& wp) const {
    return wp.nSecIndex == nSecIndex && wp.nLineIndex == nLineIndex &&
           wp.nWordIndex == nWordIndex;
  }

  bool operator!=(const CPVT_WordPlace& wp) const { return !(*this == wp); }

  inline int32_t WordCmp(const CPVT_WordPlace& wp) const {
    if (nSecIndex > wp.nSecIndex)
      return 1;
    if (nSecIndex < wp.nSecIndex)
      return -1;
    if (nLineIndex > wp.nLineIndex)
      return 1;
    if (nLineIndex < wp.nLineIndex)
      return -1;
    if (nWordIndex > wp.nWordIndex)
      return 1;
    if (nWordIndex < wp.nWordIndex)
      return -1;
    return 0;
  }

  inline int32_t LineCmp(const CPVT_WordPlace& wp) const {
    if (nSecIndex > wp.nSecIndex)
      return 1;
    if (nSecIndex < wp.nSecIndex)
      return -1;
    if (nLineIndex > wp.nLineIndex)
      return 1;
    if (nLineIndex < wp.nLineIndex)
      return -1;
    return 0;
  }

  inline int32_t SecCmp(const CPVT_WordPlace& wp) const {
    if (nSecIndex > wp.nSecIndex)
      return 1;
    if (nSecIndex < wp.nSecIndex)
      return -1;
    return 0;
  }

  int32_t nSecIndex;
  int32_t nLineIndex;
  int32_t nWordIndex;
};

#endif  // CORE_FPDFDOC_CPVT_WORDPLACE_H_
