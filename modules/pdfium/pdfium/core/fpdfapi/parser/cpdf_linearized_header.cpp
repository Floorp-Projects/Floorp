// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_linearized_header.h"

#include <algorithm>
#include <utility>

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "third_party/base/ptr_util.h"

namespace {

template <class T>
bool IsValidNumericDictionaryValue(const CPDF_Dictionary* pDict,
                                   const char* key,
                                   T min_value,
                                   bool must_exist = true) {
  if (!pDict->KeyExist(key))
    return !must_exist;
  const CPDF_Number* pNum = ToNumber(pDict->GetObjectFor(key));
  if (!pNum || !pNum->IsInteger())
    return false;
  const int raw_value = pNum->GetInteger();
  if (!pdfium::base::IsValueInRangeForNumericType<T>(raw_value))
    return false;
  return static_cast<T>(raw_value) >= min_value;
}

}  // namespace

// static
std::unique_ptr<CPDF_LinearizedHeader> CPDF_LinearizedHeader::CreateForObject(
    std::unique_ptr<CPDF_Object> pObj) {
  auto pDict = ToDictionary(std::move(pObj));
  if (!pDict || !pDict->KeyExist("Linearized") ||
      !IsValidNumericDictionaryValue<FX_FILESIZE>(pDict.get(), "L", 1) ||
      !IsValidNumericDictionaryValue<uint32_t>(pDict.get(), "P", 0, false) ||
      !IsValidNumericDictionaryValue<FX_FILESIZE>(pDict.get(), "T", 1) ||
      !IsValidNumericDictionaryValue<uint32_t>(pDict.get(), "N", 0) ||
      !IsValidNumericDictionaryValue<FX_FILESIZE>(pDict.get(), "E", 1) ||
      !IsValidNumericDictionaryValue<uint32_t>(pDict.get(), "O", 1))
    return nullptr;
  return pdfium::WrapUnique(new CPDF_LinearizedHeader(pDict.get()));
}

CPDF_LinearizedHeader::CPDF_LinearizedHeader(const CPDF_Dictionary* pDict) {
  m_szFileSize = pDict->GetIntegerFor("L");
  m_dwFirstPageNo = pDict->GetIntegerFor("P");
  m_szLastXRefOffset = pDict->GetIntegerFor("T");
  m_PageCount = pDict->GetIntegerFor("N");
  m_szFirstPageEndOffset = pDict->GetIntegerFor("E");
  m_FirstPageObjNum = pDict->GetIntegerFor("O");
  const CPDF_Array* pHintStreamRange = pDict->GetArrayFor("H");
  const size_t nHintStreamSize =
      pHintStreamRange ? pHintStreamRange->GetCount() : 0;
  if (nHintStreamSize == 2 || nHintStreamSize == 4) {
    m_szHintStart = std::max(pHintStreamRange->GetIntegerAt(0), 0);
    m_szHintLength = std::max(pHintStreamRange->GetIntegerAt(1), 0);
  }
}

CPDF_LinearizedHeader::~CPDF_LinearizedHeader() {}

bool CPDF_LinearizedHeader::HasHintTable() const {
  return GetPageCount() > 1 && GetHintStart() > 0 && GetHintLength() > 0;
}
