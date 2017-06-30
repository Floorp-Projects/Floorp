// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_shadingobject.h"

#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_shadingpattern.h"
#include "core/fpdfapi/parser/cpdf_document.h"

CPDF_ShadingObject::CPDF_ShadingObject() : m_pShading(nullptr) {}

CPDF_ShadingObject::~CPDF_ShadingObject() {}

CPDF_PageObject::Type CPDF_ShadingObject::GetType() const {
  return SHADING;
}

void CPDF_ShadingObject::Transform(const CFX_Matrix& matrix) {
  if (m_ClipPath)
    m_ClipPath.Transform(matrix);

  m_Matrix.Concat(matrix);
  if (m_ClipPath) {
    CalcBoundingBox();
  } else {
    matrix.TransformRect(m_Left, m_Right, m_Top, m_Bottom);
  }
}

bool CPDF_ShadingObject::IsShading() const {
  return true;
}

CPDF_ShadingObject* CPDF_ShadingObject::AsShading() {
  return this;
}

const CPDF_ShadingObject* CPDF_ShadingObject::AsShading() const {
  return this;
}

void CPDF_ShadingObject::CalcBoundingBox() {
  if (!m_ClipPath)
    return;
  CFX_FloatRect rect = m_ClipPath.GetClipBox();
  m_Left = rect.left;
  m_Bottom = rect.bottom;
  m_Right = rect.right;
  m_Top = rect.top;
}
