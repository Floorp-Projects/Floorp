// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cfdf_document.h"

#include <memory>
#include <utility>

#include "core/fpdfapi/edit/cpdf_creator.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_syntax_parser.h"
#include "third_party/base/ptr_util.h"

CFDF_Document::CFDF_Document()
    : CPDF_IndirectObjectHolder(), m_pRootDict(nullptr) {}

CFDF_Document::~CFDF_Document() {}

std::unique_ptr<CFDF_Document> CFDF_Document::CreateNewDoc() {
  auto pDoc = pdfium::MakeUnique<CFDF_Document>();
  pDoc->m_pRootDict = pDoc->NewIndirect<CPDF_Dictionary>();
  pDoc->m_pRootDict->SetNewFor<CPDF_Dictionary>("FDF");
  return pDoc;
}

std::unique_ptr<CFDF_Document> CFDF_Document::ParseFile(
    const CFX_RetainPtr<IFX_SeekableReadStream>& pFile) {
  if (!pFile)
    return nullptr;

  auto pDoc = pdfium::MakeUnique<CFDF_Document>();
  pDoc->ParseStream(pFile);
  return pDoc->m_pRootDict ? std::move(pDoc) : nullptr;
}

std::unique_ptr<CFDF_Document> CFDF_Document::ParseMemory(uint8_t* pData,
                                                          uint32_t size) {
  return CFDF_Document::ParseFile(IFX_MemoryStream::Create(pData, size));
}

void CFDF_Document::ParseStream(
    const CFX_RetainPtr<IFX_SeekableReadStream>& pFile) {
  m_pFile = pFile;
  CPDF_SyntaxParser parser;
  parser.InitParser(m_pFile, 0);
  while (1) {
    bool bNumber;
    CFX_ByteString word = parser.GetNextWord(&bNumber);
    if (bNumber) {
      uint32_t objnum = FXSYS_atoui(word.c_str());
      if (!objnum)
        break;

      word = parser.GetNextWord(&bNumber);
      if (!bNumber)
        break;

      word = parser.GetNextWord(nullptr);
      if (word != "obj")
        break;

      std::unique_ptr<CPDF_Object> pObj =
          parser.GetObject(this, objnum, 0, true);
      if (!pObj)
        break;

      ReplaceIndirectObjectIfHigherGeneration(objnum, std::move(pObj));
      word = parser.GetNextWord(nullptr);
      if (word != "endobj")
        break;
    } else {
      if (word != "trailer")
        break;

      std::unique_ptr<CPDF_Dictionary> pMainDict =
          ToDictionary(parser.GetObject(this, 0, 0, true));
      if (pMainDict)
        m_pRootDict = pMainDict->GetDictFor("Root");

      break;
    }
  }
}

bool CFDF_Document::WriteBuf(CFX_ByteTextBuf& buf) const {
  if (!m_pRootDict)
    return false;

  buf << "%FDF-1.2\r\n";
  for (const auto& pair : *this)
    buf << pair.first << " 0 obj\r\n"
        << pair.second.get() << "\r\nendobj\r\n\r\n";

  buf << "trailer\r\n<</Root " << m_pRootDict->GetObjNum()
      << " 0 R>>\r\n%%EOF\r\n";
  return true;
}
