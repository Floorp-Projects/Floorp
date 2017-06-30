// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_name.h"

#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "third_party/base/ptr_util.h"

CPDF_Name::CPDF_Name(CFX_WeakPtr<CFX_ByteStringPool> pPool,
                     const CFX_ByteString& str)
    : m_Name(str) {
  if (pPool)
    m_Name = pPool->Intern(m_Name);
}

CPDF_Name::~CPDF_Name() {}

CPDF_Object::Type CPDF_Name::GetType() const {
  return NAME;
}

std::unique_ptr<CPDF_Object> CPDF_Name::Clone() const {
  return pdfium::MakeUnique<CPDF_Name>(nullptr, m_Name);
}

CFX_ByteString CPDF_Name::GetString() const {
  return m_Name;
}

void CPDF_Name::SetString(const CFX_ByteString& str) {
  m_Name = str;
}

bool CPDF_Name::IsName() const {
  return true;
}

CPDF_Name* CPDF_Name::AsName() {
  return this;
}

const CPDF_Name* CPDF_Name::AsName() const {
  return this;
}

CFX_WideString CPDF_Name::GetUnicodeText() const {
  return PDF_DecodeText(m_Name);
}
