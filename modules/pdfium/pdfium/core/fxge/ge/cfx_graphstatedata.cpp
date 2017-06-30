// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/cfx_graphstatedata.h"

#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/fx_system.h"

CFX_GraphStateData::CFX_GraphStateData()
    : m_LineCap(LineCapButt),
      m_DashCount(0),
      m_DashArray(nullptr),
      m_DashPhase(0),
      m_LineJoin(LineJoinMiter),
      m_MiterLimit(10 * 1.0f),
      m_LineWidth(1.0f) {}

CFX_GraphStateData::CFX_GraphStateData(const CFX_GraphStateData& src) {
  m_DashArray = nullptr;
  Copy(src);
}

void CFX_GraphStateData::Copy(const CFX_GraphStateData& src) {
  m_LineCap = src.m_LineCap;
  m_DashCount = src.m_DashCount;
  FX_Free(m_DashArray);
  m_DashArray = nullptr;
  m_DashPhase = src.m_DashPhase;
  m_LineJoin = src.m_LineJoin;
  m_MiterLimit = src.m_MiterLimit;
  m_LineWidth = src.m_LineWidth;
  if (m_DashCount) {
    m_DashArray = FX_Alloc(FX_FLOAT, m_DashCount);
    FXSYS_memcpy(m_DashArray, src.m_DashArray, m_DashCount * sizeof(FX_FLOAT));
  }
}

CFX_GraphStateData::~CFX_GraphStateData() {
  FX_Free(m_DashArray);
}

void CFX_GraphStateData::SetDashCount(int count) {
  FX_Free(m_DashArray);
  m_DashArray = nullptr;
  m_DashCount = count;
  if (count == 0)
    return;
  m_DashArray = FX_Alloc(FX_FLOAT, count);
}
