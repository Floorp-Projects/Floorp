// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/cpdf_pagelabel.h"

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfdoc/cpdf_numbertree.h"

namespace {

CFX_WideString MakeRoman(int num) {
  const int kArabic[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
  const CFX_WideString kRoman[] = {L"m",  L"cm", L"d",  L"cd", L"c",
                                   L"xc", L"l",  L"xl", L"x",  L"ix",
                                   L"v",  L"iv", L"i"};
  const int kMaxNum = 1000000;

  num %= kMaxNum;
  int i = 0;
  CFX_WideString wsRomanNumber;
  while (num > 0) {
    while (num >= kArabic[i]) {
      num = num - kArabic[i];
      wsRomanNumber += kRoman[i];
    }
    i = i + 1;
  }
  return wsRomanNumber;
}

CFX_WideString MakeLetters(int num) {
  if (num == 0)
    return CFX_WideString();

  CFX_WideString wsLetters;
  const int nMaxCount = 1000;
  const int nLetterCount = 26;
  --num;

  int count = num / nLetterCount + 1;
  count %= nMaxCount;
  FX_WCHAR ch = L'a' + num % nLetterCount;
  for (int i = 0; i < count; i++)
    wsLetters += ch;
  return wsLetters;
}

CFX_WideString GetLabelNumPortion(int num, const CFX_ByteString& bsStyle) {
  CFX_WideString wsNumPortion;
  if (bsStyle.IsEmpty())
    return wsNumPortion;
  if (bsStyle == "D") {
    wsNumPortion.Format(L"%d", num);
  } else if (bsStyle == "R") {
    wsNumPortion = MakeRoman(num);
    wsNumPortion.MakeUpper();
  } else if (bsStyle == "r") {
    wsNumPortion = MakeRoman(num);
  } else if (bsStyle == "A") {
    wsNumPortion = MakeLetters(num);
    wsNumPortion.MakeUpper();
  } else if (bsStyle == "a") {
    wsNumPortion = MakeLetters(num);
  }
  return wsNumPortion;
}

}  // namespace

CPDF_PageLabel::CPDF_PageLabel(CPDF_Document* pDocument)
    : m_pDocument(pDocument) {}

bool CPDF_PageLabel::GetLabel(int nPage, CFX_WideString* wsLabel) const {
  if (!m_pDocument)
    return false;

  if (nPage < 0 || nPage >= m_pDocument->GetPageCount())
    return false;

  CPDF_Dictionary* pPDFRoot = m_pDocument->GetRoot();
  if (!pPDFRoot)
    return false;

  CPDF_Dictionary* pLabels = pPDFRoot->GetDictFor("PageLabels");
  if (!pLabels)
    return false;

  CPDF_NumberTree numberTree(pLabels);
  CPDF_Object* pValue = nullptr;
  int n = nPage;
  while (n >= 0) {
    pValue = numberTree.LookupValue(n);
    if (pValue)
      break;
    n--;
  }

  if (pValue) {
    pValue = pValue->GetDirect();
    if (CPDF_Dictionary* pLabel = pValue->AsDictionary()) {
      if (pLabel->KeyExist("P"))
        *wsLabel += pLabel->GetUnicodeTextFor("P");

      CFX_ByteString bsNumberingStyle = pLabel->GetStringFor("S", "");
      int nLabelNum = nPage - n + pLabel->GetIntegerFor("St", 1);
      CFX_WideString wsNumPortion =
          GetLabelNumPortion(nLabelNum, bsNumberingStyle);
      *wsLabel += wsNumPortion;
      return true;
    }
  }
  wsLabel->Format(L"%d", nPage + 1);
  return true;
}

int32_t CPDF_PageLabel::GetPageByLabel(const CFX_ByteStringC& bsLabel) const {
  if (!m_pDocument)
    return -1;

  CPDF_Dictionary* pPDFRoot = m_pDocument->GetRoot();
  if (!pPDFRoot)
    return -1;

  int nPages = m_pDocument->GetPageCount();
  for (int i = 0; i < nPages; i++) {
    CFX_WideString str;
    if (!GetLabel(i, &str))
      continue;
    if (PDF_EncodeText(str).Compare(bsLabel))
      return i;
  }

  int nPage = FXSYS_atoi(CFX_ByteString(bsLabel).c_str());  // NUL terminate.
  return nPage > 0 && nPage <= nPages ? nPage : -1;
}

int32_t CPDF_PageLabel::GetPageByLabel(const CFX_WideStringC& wsLabel) const {
  return GetPageByLabel(PDF_EncodeText(wsLabel.c_str()).AsStringC());
}
