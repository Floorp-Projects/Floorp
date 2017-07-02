// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_pageobject.h"

CPDF_PageObject::CPDF_PageObject() {}

CPDF_PageObject::~CPDF_PageObject() {}

bool CPDF_PageObject::IsText() const {
  return false;
}

bool CPDF_PageObject::IsPath() const {
  return false;
}

bool CPDF_PageObject::IsImage() const {
  return false;
}

bool CPDF_PageObject::IsShading() const {
  return false;
}

bool CPDF_PageObject::IsForm() const {
  return false;
}

CPDF_TextObject* CPDF_PageObject::AsText() {
  return nullptr;
}

const CPDF_TextObject* CPDF_PageObject::AsText() const {
  return nullptr;
}

CPDF_PathObject* CPDF_PageObject::AsPath() {
  return nullptr;
}

const CPDF_PathObject* CPDF_PageObject::AsPath() const {
  return nullptr;
}

CPDF_ImageObject* CPDF_PageObject::AsImage() {
  return nullptr;
}

const CPDF_ImageObject* CPDF_PageObject::AsImage() const {
  return nullptr;
}

CPDF_ShadingObject* CPDF_PageObject::AsShading() {
  return nullptr;
}

const CPDF_ShadingObject* CPDF_PageObject::AsShading() const {
  return nullptr;
}

CPDF_FormObject* CPDF_PageObject::AsForm() {
  return nullptr;
}

const CPDF_FormObject* CPDF_PageObject::AsForm() const {
  return nullptr;
}

void CPDF_PageObject::CopyData(const CPDF_PageObject* pSrc) {
  CopyStates(*pSrc);
  m_Left = pSrc->m_Left;
  m_Right = pSrc->m_Right;
  m_Top = pSrc->m_Top;
  m_Bottom = pSrc->m_Bottom;
}

void CPDF_PageObject::TransformClipPath(CFX_Matrix& matrix) {
  if (!m_ClipPath)
    return;
  m_ClipPath.Transform(matrix);
}

void CPDF_PageObject::TransformGeneralState(CFX_Matrix& matrix) {
  if (!m_GeneralState)
    return;
  m_GeneralState.GetMutableMatrix()->Concat(matrix);
}

FX_RECT CPDF_PageObject::GetBBox(const CFX_Matrix* pMatrix) const {
  CFX_FloatRect rect(m_Left, m_Bottom, m_Right, m_Top);
  if (pMatrix) {
    pMatrix->TransformRect(rect);
  }
  return rect.GetOuterRect();
}
