// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_pattern.h"

CPDF_Pattern::CPDF_Pattern(PatternType type,
                           CPDF_Document* pDoc,
                           CPDF_Object* pObj,
                           const CFX_Matrix& parentMatrix)
    : m_PatternType(type),
      m_pDocument(pDoc),
      m_pPatternObj(pObj),
      m_ParentMatrix(parentMatrix) {}

CPDF_Pattern::~CPDF_Pattern() {}
