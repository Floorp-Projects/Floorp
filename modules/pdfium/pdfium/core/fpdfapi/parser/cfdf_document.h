// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CFDF_DOCUMENT_H_
#define CORE_FPDFAPI_PARSER_CFDF_DOCUMENT_H_

#include <memory>

#include "core/fpdfapi/parser/cpdf_indirect_object_holder.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fxcrt/fx_basic.h"

class CPDF_Dictionary;

class CFDF_Document : public CPDF_IndirectObjectHolder {
 public:
  static std::unique_ptr<CFDF_Document> CreateNewDoc();
  static std::unique_ptr<CFDF_Document> ParseFile(
      const CFX_RetainPtr<IFX_SeekableReadStream>& pFile);
  static std::unique_ptr<CFDF_Document> ParseMemory(uint8_t* pData,
                                                    uint32_t size);

  CFDF_Document();
  ~CFDF_Document() override;

  bool WriteBuf(CFX_ByteTextBuf& buf) const;
  CPDF_Dictionary* GetRoot() const { return m_pRootDict; }

 protected:
  void ParseStream(const CFX_RetainPtr<IFX_SeekableReadStream>& pFile);

  CPDF_Dictionary* m_pRootDict;
  CFX_RetainPtr<IFX_SeekableReadStream> m_pFile;
};

#endif  // CORE_FPDFAPI_PARSER_CFDF_DOCUMENT_H_
