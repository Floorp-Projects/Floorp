// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cline.h"

CLine::CLine() {}

CLine::~CLine() {}

CPVT_WordPlace CLine::GetBeginWordPlace() const {
  return CPVT_WordPlace(LinePlace.nSecIndex, LinePlace.nLineIndex, -1);
}

CPVT_WordPlace CLine::GetEndWordPlace() const {
  return CPVT_WordPlace(LinePlace.nSecIndex, LinePlace.nLineIndex,
                        m_LineInfo.nEndWordIndex);
}

CPVT_WordPlace CLine::GetPrevWordPlace(const CPVT_WordPlace& place) const {
  if (place.nWordIndex > m_LineInfo.nEndWordIndex) {
    return CPVT_WordPlace(place.nSecIndex, place.nLineIndex,
                          m_LineInfo.nEndWordIndex);
  }
  return CPVT_WordPlace(place.nSecIndex, place.nLineIndex,
                        place.nWordIndex - 1);
}

CPVT_WordPlace CLine::GetNextWordPlace(const CPVT_WordPlace& place) const {
  if (place.nWordIndex < m_LineInfo.nBeginWordIndex) {
    return CPVT_WordPlace(place.nSecIndex, place.nLineIndex,
                          m_LineInfo.nBeginWordIndex);
  }
  return CPVT_WordPlace(place.nSecIndex, place.nLineIndex,
                        place.nWordIndex + 1);
}
