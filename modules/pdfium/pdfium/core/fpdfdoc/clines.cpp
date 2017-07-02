// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/clines.h"

#include "core/fpdfdoc/cline.h"

CLines::CLines() : m_nTotal(0) {}

CLines::~CLines() {
  RemoveAll();
}

int32_t CLines::GetSize() const {
  return m_Lines.GetSize();
}

CLine* CLines::GetAt(int32_t nIndex) const {
  return m_Lines.GetAt(nIndex);
}

void CLines::Empty() {
  m_nTotal = 0;
}

void CLines::RemoveAll() {
  for (int32_t i = 0, sz = GetSize(); i < sz; i++)
    delete GetAt(i);
  m_Lines.RemoveAll();
  m_nTotal = 0;
}

int32_t CLines::Add(const CPVT_LineInfo& lineinfo) {
  if (m_nTotal >= GetSize()) {
    CLine* pLine = new CLine;
    pLine->m_LineInfo = lineinfo;
    m_Lines.Add(pLine);
  } else if (CLine* pLine = GetAt(m_nTotal)) {
    pLine->m_LineInfo = lineinfo;
  }
  return m_nTotal++;
}

void CLines::Clear() {
  for (int32_t i = GetSize() - 1; i >= m_nTotal; i--) {
    delete GetAt(i);
    m_Lines.RemoveAt(i);
  }
}
