// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_tilingpattern.h"

#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "third_party/base/ptr_util.h"

CPDF_TilingPattern::CPDF_TilingPattern(CPDF_Document* pDoc,
                                       CPDF_Object* pPatternObj,
                                       const CFX_Matrix& parentMatrix)
    : CPDF_Pattern(TILING, pDoc, pPatternObj, parentMatrix) {
  CPDF_Dictionary* pDict = m_pPatternObj->GetDict();
  m_Pattern2Form = pDict->GetMatrixFor("Matrix");
  m_bColored = pDict->GetIntegerFor("PaintType") == 1;
  m_Pattern2Form.Concat(parentMatrix);
}

CPDF_TilingPattern::~CPDF_TilingPattern() {}

CPDF_TilingPattern* CPDF_TilingPattern::AsTilingPattern() {
  return this;
}

CPDF_ShadingPattern* CPDF_TilingPattern::AsShadingPattern() {
  return nullptr;
}

bool CPDF_TilingPattern::Load() {
  if (m_pForm)
    return true;

  CPDF_Dictionary* pDict = m_pPatternObj->GetDict();
  if (!pDict)
    return false;

  m_bColored = pDict->GetIntegerFor("PaintType") == 1;
  m_XStep = (FX_FLOAT)FXSYS_fabs(pDict->GetNumberFor("XStep"));
  m_YStep = (FX_FLOAT)FXSYS_fabs(pDict->GetNumberFor("YStep"));

  CPDF_Stream* pStream = m_pPatternObj->AsStream();
  if (!pStream)
    return false;

  m_pForm = pdfium::MakeUnique<CPDF_Form>(m_pDocument, nullptr, pStream);
  m_pForm->ParseContent(nullptr, &m_ParentMatrix, nullptr);
  m_BBox = pDict->GetRectFor("BBox");
  return true;
}
