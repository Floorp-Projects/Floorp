// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_metadata.h"

#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fxcrt/fx_xml.h"

CPDF_Metadata::CPDF_Metadata(CPDF_Document* pDoc) {
  CPDF_Dictionary* pRoot = pDoc->GetRoot();
  if (!pRoot)
    return;

  CPDF_Stream* pStream = pRoot->GetStreamFor("Metadata");
  if (!pStream)
    return;

  CPDF_StreamAcc acc;
  acc.LoadAllData(pStream, false);
  m_pXmlElement = CXML_Element::Parse(acc.GetData(), acc.GetSize());
}

CPDF_Metadata::~CPDF_Metadata() {}

const CXML_Element* CPDF_Metadata::GetRoot() const {
  return m_pXmlElement.get();
}
