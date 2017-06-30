// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/fx_bidi.h"
#include "core/fxcrt/fx_ucd.h"

#include <algorithm>

CFX_BidiChar::CFX_BidiChar()
    : m_CurrentSegment({0, 0, NEUTRAL}), m_LastSegment({0, 0, NEUTRAL}) {}

bool CFX_BidiChar::AppendChar(FX_WCHAR wch) {
  uint32_t dwProps = FX_GetUnicodeProperties(wch);
  int32_t iBidiCls = (dwProps & FX_BIDICLASSBITSMASK) >> FX_BIDICLASSBITS;
  Direction direction = NEUTRAL;
  switch (iBidiCls) {
    case FX_BIDICLASS_L:
    case FX_BIDICLASS_AN:
    case FX_BIDICLASS_EN:
      direction = LEFT;
      break;
    case FX_BIDICLASS_R:
    case FX_BIDICLASS_AL:
      direction = RIGHT;
      break;
  }

  bool bChangeDirection = (direction != m_CurrentSegment.direction);
  if (bChangeDirection)
    StartNewSegment(direction);

  m_CurrentSegment.count++;
  return bChangeDirection;
}

bool CFX_BidiChar::EndChar() {
  StartNewSegment(NEUTRAL);
  return m_LastSegment.count > 0;
}

void CFX_BidiChar::StartNewSegment(CFX_BidiChar::Direction direction) {
  m_LastSegment = m_CurrentSegment;
  m_CurrentSegment.start += m_CurrentSegment.count;
  m_CurrentSegment.count = 0;
  m_CurrentSegment.direction = direction;
}

CFX_BidiString::CFX_BidiString(const CFX_WideString& str)
    : m_Str(str),
      m_pBidiChar(new CFX_BidiChar),
      m_eOverallDirection(CFX_BidiChar::LEFT) {
  for (int i = 0; i < m_Str.GetLength(); ++i) {
    if (m_pBidiChar->AppendChar(m_Str.GetAt(i)))
      m_Order.push_back(m_pBidiChar->GetSegmentInfo());
  }
  if (m_pBidiChar->EndChar())
    m_Order.push_back(m_pBidiChar->GetSegmentInfo());

  size_t nR2L = std::count_if(m_Order.begin(), m_Order.end(),
                              [](const CFX_BidiChar::Segment& seg) {
                                return seg.direction == CFX_BidiChar::RIGHT;
                              });

  size_t nL2R = std::count_if(m_Order.begin(), m_Order.end(),
                              [](const CFX_BidiChar::Segment& seg) {
                                return seg.direction == CFX_BidiChar::LEFT;
                              });

  if (nR2L > 0 && nR2L >= nL2R)
    SetOverallDirectionRight();
}

CFX_BidiString::~CFX_BidiString() {}

void CFX_BidiString::SetOverallDirectionRight() {
  if (m_eOverallDirection != CFX_BidiChar::RIGHT) {
    std::reverse(m_Order.begin(), m_Order.end());
    m_eOverallDirection = CFX_BidiChar::RIGHT;
  }
}
