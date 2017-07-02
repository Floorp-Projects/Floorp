// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_GRAPHSTATEDATA_H_
#define CORE_FXGE_CFX_GRAPHSTATEDATA_H_

#include "core/fxcrt/fx_system.h"

class CFX_GraphStateData {
 public:
  enum LineCap { LineCapButt = 0, LineCapRound = 1, LineCapSquare = 2 };

  CFX_GraphStateData();
  CFX_GraphStateData(const CFX_GraphStateData& src);
  ~CFX_GraphStateData();

  void Copy(const CFX_GraphStateData& src);
  void SetDashCount(int count);

  LineCap m_LineCap;
  int m_DashCount;
  FX_FLOAT* m_DashArray;
  FX_FLOAT m_DashPhase;

  enum LineJoin {
    LineJoinMiter = 0,
    LineJoinRound = 1,
    LineJoinBevel = 2,
  };
  LineJoin m_LineJoin;
  FX_FLOAT m_MiterLimit;
  FX_FLOAT m_LineWidth;
};

#endif  // CORE_FXGE_CFX_GRAPHSTATEDATA_H_
