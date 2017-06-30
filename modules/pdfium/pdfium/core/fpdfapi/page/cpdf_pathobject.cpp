// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_pathobject.h"

CPDF_PathObject::CPDF_PathObject() {}

CPDF_PathObject::~CPDF_PathObject() {}

CPDF_PageObject::Type CPDF_PathObject::GetType() const {
  return PATH;
}

void CPDF_PathObject::Transform(const CFX_Matrix& matrix) {
  m_Matrix.Concat(matrix);
  CalcBoundingBox();
}

bool CPDF_PathObject::IsPath() const {
  return true;
}

CPDF_PathObject* CPDF_PathObject::AsPath() {
  return this;
}

const CPDF_PathObject* CPDF_PathObject::AsPath() const {
  return this;
}

void CPDF_PathObject::CalcBoundingBox() {
  if (!m_Path)
    return;
  CFX_FloatRect rect;
  FX_FLOAT width = m_GraphState.GetLineWidth();
  if (m_bStroke && width != 0) {
    rect = m_Path.GetBoundingBox(width, m_GraphState.GetMiterLimit());
  } else {
    rect = m_Path.GetBoundingBox();
  }
  m_Matrix.TransformRect(rect);

  if (width == 0 && m_bStroke) {
    rect.left += -0.5f;
    rect.right += 0.5f;
    rect.bottom += -0.5f;
    rect.top += 0.5f;
  }
  m_Left = rect.left;
  m_Right = rect.right;
  m_Top = rect.top;
  m_Bottom = rect.bottom;
}
