// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_pageobjectholder.h"

#include <algorithm>

#include "core/fpdfapi/page/cpdf_contentparser.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/pageint.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"

CPDF_PageObjectHolder::CPDF_PageObjectHolder()
    : m_pFormDict(nullptr),
      m_pFormStream(nullptr),
      m_pDocument(nullptr),
      m_pPageResources(nullptr),
      m_pResources(nullptr),
      m_Transparency(0),
      m_bBackgroundAlphaNeeded(false),
      m_bHasImageMask(false),
      m_ParseState(CONTENT_NOT_PARSED) {}

CPDF_PageObjectHolder::~CPDF_PageObjectHolder() {}

void CPDF_PageObjectHolder::ContinueParse(IFX_Pause* pPause) {
  if (!m_pParser) {
    return;
  }
  m_pParser->Continue(pPause);
  if (m_pParser->GetStatus() == CPDF_ContentParser::Done) {
    m_ParseState = CONTENT_PARSED;
    m_pParser.reset();
  }
}

void CPDF_PageObjectHolder::Transform(const CFX_Matrix& matrix) {
  for (auto& pObj : m_PageObjectList)
    pObj->Transform(matrix);
}

CFX_FloatRect CPDF_PageObjectHolder::CalcBoundingBox() const {
  if (m_PageObjectList.empty())
    return CFX_FloatRect(0, 0, 0, 0);

  FX_FLOAT left = 1000000.0f;
  FX_FLOAT right = -1000000.0f;
  FX_FLOAT bottom = 1000000.0f;
  FX_FLOAT top = -1000000.0f;
  for (const auto& pObj : m_PageObjectList) {
    left = std::min(left, pObj->m_Left);
    right = std::max(right, pObj->m_Right);
    bottom = std::min(bottom, pObj->m_Bottom);
    top = std::max(top, pObj->m_Top);
  }
  return CFX_FloatRect(left, bottom, right, top);
}

void CPDF_PageObjectHolder::LoadTransInfo() {
  if (!m_pFormDict) {
    return;
  }
  CPDF_Dictionary* pGroup = m_pFormDict->GetDictFor("Group");
  if (!pGroup) {
    return;
  }
  if (pGroup->GetStringFor("S") != "Transparency") {
    return;
  }
  m_Transparency |= PDFTRANS_GROUP;
  if (pGroup->GetIntegerFor("I")) {
    m_Transparency |= PDFTRANS_ISOLATED;
  }
  if (pGroup->GetIntegerFor("K")) {
    m_Transparency |= PDFTRANS_KNOCKOUT;
  }
}
