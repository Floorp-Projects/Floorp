// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_STREAMPARSER_H_
#define CORE_FPDFAPI_PAGE_CPDF_STREAMPARSER_H_

#include <memory>
#include <utility>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/cfx_string_pool_template.h"
#include "core/fxcrt/cfx_weak_ptr.h"

class CPDF_StreamParser {
 public:
  enum SyntaxType { EndOfData, Number, Keyword, Name, Others };

  CPDF_StreamParser(const uint8_t* pData, uint32_t dwSize);
  CPDF_StreamParser(const uint8_t* pData,
                    uint32_t dwSize,
                    const CFX_WeakPtr<CFX_ByteStringPool>& pPool);
  ~CPDF_StreamParser();

  SyntaxType ParseNextElement();
  CFX_ByteStringC GetWord() const {
    return CFX_ByteStringC(m_WordBuffer, m_WordSize);
  }
  uint32_t GetPos() const { return m_Pos; }
  void SetPos(uint32_t pos) { m_Pos = pos; }
  std::unique_ptr<CPDF_Object> GetObject() { return std::move(m_pLastObj); }
  std::unique_ptr<CPDF_Object> ReadNextObject(bool bAllowNestedArray,
                                              bool bInArray,
                                              uint32_t dwRecursionLevel);
  std::unique_ptr<CPDF_Stream> ReadInlineStream(
      CPDF_Document* pDoc,
      std::unique_ptr<CPDF_Dictionary> pDict,
      CPDF_Object* pCSObj);

 private:
  friend class cpdf_streamparser_ReadHexString_Test;

  void GetNextWord(bool& bIsNumber);
  CFX_ByteString ReadString();
  CFX_ByteString ReadHexString();
  bool PositionIsInBounds() const;

  const uint8_t* m_pBuf;
  uint32_t m_Size;  // Length in bytes of m_pBuf.
  uint32_t m_Pos;   // Current byte position within m_pBuf.
  uint8_t m_WordBuffer[256];
  uint32_t m_WordSize;
  std::unique_ptr<CPDF_Object> m_pLastObj;
  CFX_WeakPtr<CFX_ByteStringPool> m_pPool;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_STREAMPARSER_H_
