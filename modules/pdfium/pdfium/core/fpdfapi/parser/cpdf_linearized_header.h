// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CPDF_LINEARIZED_HEADER_H_
#define CORE_FPDFAPI_PARSER_CPDF_LINEARIZED_HEADER_H_

#include <memory>

#include "core/fxcrt/fx_memory.h"
#include "core/fxcrt/fx_stream.h"

class CPDF_Dictionary;
class CPDF_Object;

class CPDF_LinearizedHeader {
 public:
  ~CPDF_LinearizedHeader();
  static std::unique_ptr<CPDF_LinearizedHeader> CreateForObject(
      std::unique_ptr<CPDF_Object> pObj);

  // Will only return values > 0.
  FX_FILESIZE GetFileSize() const { return m_szFileSize; }
  uint32_t GetFirstPageNo() const { return m_dwFirstPageNo; }
  // Will only return values > 0.
  FX_FILESIZE GetLastXRefOffset() const { return m_szLastXRefOffset; }
  uint32_t GetPageCount() const { return m_PageCount; }
  // Will only return values > 0.
  FX_FILESIZE GetFirstPageEndOffset() const { return m_szFirstPageEndOffset; }
  // Will only return values > 0.
  uint32_t GetFirstPageObjNum() const { return m_FirstPageObjNum; }

  bool HasHintTable() const;
  // Will only return values > 0.
  FX_FILESIZE GetHintStart() const { return m_szHintStart; }
  // Will only return values > 0.
  FX_FILESIZE GetHintLength() const { return m_szHintLength; }

 protected:
  explicit CPDF_LinearizedHeader(const CPDF_Dictionary* pDict);

 private:
  FX_FILESIZE m_szFileSize = 0;
  uint32_t m_dwFirstPageNo = 0;
  FX_FILESIZE m_szLastXRefOffset = 0;
  uint32_t m_PageCount = 0;
  FX_FILESIZE m_szFirstPageEndOffset = 0;
  uint32_t m_FirstPageObjNum = 0;
  FX_FILESIZE m_szHintStart = 0;
  FX_FILESIZE m_szHintLength = 0;
};

#endif  // CORE_FPDFAPI_PARSER_CPDF_LINEARIZED_HEADER_H_
