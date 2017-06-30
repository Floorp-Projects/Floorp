// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CLINE_H_
#define CORE_FPDFDOC_CLINE_H_

#include "core/fpdfdoc/cpvt_lineinfo.h"
#include "core/fpdfdoc/cpvt_wordplace.h"

class CLine final {
 public:
  CLine();
  ~CLine();

  CPVT_WordPlace GetBeginWordPlace() const;
  CPVT_WordPlace GetEndWordPlace() const;
  CPVT_WordPlace GetPrevWordPlace(const CPVT_WordPlace& place) const;
  CPVT_WordPlace GetNextWordPlace(const CPVT_WordPlace& place) const;
  CPVT_WordPlace LinePlace;
  CPVT_LineInfo m_LineInfo;
};

#endif  // CORE_FPDFDOC_CLINE_H_
