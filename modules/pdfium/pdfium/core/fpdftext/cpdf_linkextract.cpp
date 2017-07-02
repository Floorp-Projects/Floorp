// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdftext/cpdf_linkextract.h"

#include <vector>

#include "core/fpdftext/cpdf_textpage.h"
#include "core/fxcrt/fx_ext.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

CPDF_LinkExtract::CPDF_LinkExtract(const CPDF_TextPage* pTextPage)
    : m_pTextPage(pTextPage) {}

CPDF_LinkExtract::~CPDF_LinkExtract() {}

void CPDF_LinkExtract::ExtractLinks() {
  m_LinkArray.clear();
  if (!m_pTextPage->IsParsed())
    return;

  m_strPageText = m_pTextPage->GetPageText(0, -1);
  if (m_strPageText.IsEmpty())
    return;

  ParseLink();
}

void CPDF_LinkExtract::ParseLink() {
  int start = 0, pos = 0;
  int TotalChar = m_pTextPage->CountChars();
  while (pos < TotalChar) {
    FPDF_CHAR_INFO pageChar;
    m_pTextPage->GetCharInfo(pos, &pageChar);
    if (pageChar.m_Flag == FPDFTEXT_CHAR_GENERATED ||
        pageChar.m_Unicode == 0x20 || pos == TotalChar - 1) {
      int nCount = pos - start;
      if (pos == TotalChar - 1)
        nCount++;
      CFX_WideString strBeCheck;
      strBeCheck = m_pTextPage->GetPageText(start, nCount);
      if (strBeCheck.GetLength() > 5) {
        while (strBeCheck.GetLength() > 0) {
          FX_WCHAR ch = strBeCheck.GetAt(strBeCheck.GetLength() - 1);
          if (ch == L')' || ch == L',' || ch == L'>' || ch == L'.') {
            strBeCheck = strBeCheck.Mid(0, strBeCheck.GetLength() - 1);
            nCount--;
          } else {
            break;
          }
        }
        if (nCount > 5 &&
            (CheckWebLink(strBeCheck) || CheckMailLink(strBeCheck))) {
          m_LinkArray.push_back({start, nCount, strBeCheck});
        }
      }
      start = ++pos;
    } else {
      pos++;
    }
  }
}

bool CPDF_LinkExtract::CheckWebLink(CFX_WideString& strBeCheck) {
  CFX_WideString str = strBeCheck;
  str.MakeLower();
  if (str.Find(L"http://www.") != -1) {
    strBeCheck = strBeCheck.Right(str.GetLength() - str.Find(L"http://www."));
    return true;
  }
  if (str.Find(L"http://") != -1) {
    strBeCheck = strBeCheck.Right(str.GetLength() - str.Find(L"http://"));
    return true;
  }
  if (str.Find(L"https://www.") != -1) {
    strBeCheck = strBeCheck.Right(str.GetLength() - str.Find(L"https://www."));
    return true;
  }
  if (str.Find(L"https://") != -1) {
    strBeCheck = strBeCheck.Right(str.GetLength() - str.Find(L"https://"));
    return true;
  }
  if (str.Find(L"www.") != -1) {
    strBeCheck = strBeCheck.Right(str.GetLength() - str.Find(L"www."));
    strBeCheck = L"http://" + strBeCheck;
    return true;
  }
  return false;
}

bool CPDF_LinkExtract::CheckMailLink(CFX_WideString& str) {
  int aPos = str.Find(L'@');
  // Invalid when no '@'.
  if (aPos < 1)
    return false;

  // Check the local part.
  int pPos = aPos;  // Used to track the position of '@' or '.'.
  for (int i = aPos - 1; i >= 0; i--) {
    FX_WCHAR ch = str.GetAt(i);
    if (ch == L'_' || ch == L'-' || FXSYS_iswalnum(ch))
      continue;

    if (ch != L'.' || i == pPos - 1 || i == 0) {
      if (i == aPos - 1) {
        // There is '.' or invalid char before '@'.
        return false;
      }
      // End extracting for other invalid chars, '.' at the beginning, or
      // consecutive '.'.
      int removed_len = i == pPos - 1 ? i + 2 : i + 1;
      str = str.Right(str.GetLength() - removed_len);
      break;
    }
    // Found a valid '.'.
    pPos = i;
  }

  // Check the domain name part.
  aPos = str.Find(L'@');
  if (aPos < 1)
    return false;

  str.TrimRight(L'.');
  // At least one '.' in domain name, but not at the beginning.
  // TODO(weili): RFC5322 allows domain names to be a local name without '.'.
  // Check whether we should remove this check.
  int ePos = str.Find(L'.', aPos + 1);
  if (ePos == -1 || ePos == aPos + 1)
    return false;

  // Validate all other chars in domain name.
  int nLen = str.GetLength();
  pPos = 0;  // Used to track the position of '.'.
  for (int i = aPos + 1; i < nLen; i++) {
    FX_WCHAR wch = str.GetAt(i);
    if (wch == L'-' || FXSYS_iswalnum(wch))
      continue;

    if (wch != L'.' || i == pPos + 1) {
      // Domain name should end before invalid char.
      int host_end = i == pPos + 1 ? i - 2 : i - 1;
      if (pPos > 0 && host_end - aPos >= 3) {
        // Trim the ending invalid chars if there is at least one '.' and name.
        str = str.Left(host_end + 1);
        break;
      }
      return false;
    }
    pPos = i;
  }

  if (str.Find(L"mailto:") == -1)
    str = L"mailto:" + str;

  return true;
}

CFX_WideString CPDF_LinkExtract::GetURL(size_t index) const {
  return index < m_LinkArray.size() ? m_LinkArray[index].m_strUrl : L"";
}

std::vector<CFX_FloatRect> CPDF_LinkExtract::GetRects(size_t index) const {
  if (index >= m_LinkArray.size())
    return std::vector<CFX_FloatRect>();

  return m_pTextPage->GetRectArray(m_LinkArray[index].m_Start,
                                   m_LinkArray[index].m_Count);
}
