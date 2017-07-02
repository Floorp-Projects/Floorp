// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "third_party/base/ptr_util.h"

CPDF_Boolean::CPDF_Boolean() : m_bValue(false) {}

CPDF_Boolean::CPDF_Boolean(bool value) : m_bValue(value) {}

CPDF_Boolean::~CPDF_Boolean() {}

CPDF_Object::Type CPDF_Boolean::GetType() const {
  return BOOLEAN;
}

std::unique_ptr<CPDF_Object> CPDF_Boolean::Clone() const {
  return pdfium::MakeUnique<CPDF_Boolean>(m_bValue);
}

CFX_ByteString CPDF_Boolean::GetString() const {
  return m_bValue ? "true" : "false";
}

int CPDF_Boolean::GetInteger() const {
  return m_bValue;
}

void CPDF_Boolean::SetString(const CFX_ByteString& str) {
  m_bValue = (str == "true");
}

bool CPDF_Boolean::IsBoolean() const {
  return true;
}

CPDF_Boolean* CPDF_Boolean::AsBoolean() {
  return this;
}

const CPDF_Boolean* CPDF_Boolean::AsBoolean() const {
  return this;
}
