// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_WORDRANGE_H_
#define CORE_FPDFDOC_CPVT_WORDRANGE_H_

#include "core/fpdfdoc/cpvt_wordplace.h"
#include "core/fxcrt/fx_system.h"

struct CPVT_WordRange {
  CPVT_WordRange() {}

  CPVT_WordRange(const CPVT_WordPlace& begin, const CPVT_WordPlace& end) {
    Set(begin, end);
  }

  void Default() {
    BeginPos.Default();
    EndPos.Default();
  }

  void Set(const CPVT_WordPlace& begin, const CPVT_WordPlace& end) {
    BeginPos = begin;
    EndPos = end;
    SwapWordPlace();
  }

  void SetBeginPos(const CPVT_WordPlace& begin) {
    BeginPos = begin;
    SwapWordPlace();
  }

  void SetEndPos(const CPVT_WordPlace& end) {
    EndPos = end;
    SwapWordPlace();
  }

  bool IsExist() const { return BeginPos != EndPos; }

  bool operator!=(const CPVT_WordRange& wr) const {
    return wr.BeginPos != BeginPos || wr.EndPos != EndPos;
  }

  void SwapWordPlace() {
    if (BeginPos.WordCmp(EndPos) > 0) {
      CPVT_WordPlace place = EndPos;
      EndPos = BeginPos;
      BeginPos = place;
    }
  }

  CPVT_WordPlace BeginPos;
  CPVT_WordPlace EndPos;
};

#endif  // CORE_FPDFDOC_CPVT_WORDRANGE_H_
