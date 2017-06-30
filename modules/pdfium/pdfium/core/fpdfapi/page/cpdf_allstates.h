// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_ALLSTATES_H_
#define CORE_FPDFAPI_PAGE_CPDF_ALLSTATES_H_

#include "core/fpdfapi/page/cpdf_graphicstates.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Array;
class CPDF_Dictionary;
class CPDF_StreamContentParser;

class CPDF_AllStates : public CPDF_GraphicStates {
 public:
  CPDF_AllStates();
  ~CPDF_AllStates() override;

  void Copy(const CPDF_AllStates& src);
  void ProcessExtGS(CPDF_Dictionary* pGS, CPDF_StreamContentParser* pParser);
  void SetLineDash(CPDF_Array*, FX_FLOAT, FX_FLOAT scale);

  CFX_Matrix m_TextMatrix;
  CFX_Matrix m_CTM;
  CFX_Matrix m_ParentMatrix;
  CFX_PointF m_TextPos;
  CFX_PointF m_TextLinePos;
  FX_FLOAT m_TextLeading;
  FX_FLOAT m_TextRise;
  FX_FLOAT m_TextHorzScale;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_ALLSTATES_H_
