// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdftext/cpdf_textpagefind.h"

#include <cwchar>
#include <cwctype>
#include <vector>

#include "core/fpdftext/cpdf_textpage.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "third_party/base/stl_util.h"

namespace {

bool IsIgnoreSpaceCharacter(FX_WCHAR curChar) {
  if (curChar < 255 || (curChar >= 0x0600 && curChar <= 0x06FF) ||
      (curChar >= 0xFE70 && curChar <= 0xFEFF) ||
      (curChar >= 0xFB50 && curChar <= 0xFDFF) ||
      (curChar >= 0x0400 && curChar <= 0x04FF) ||
      (curChar >= 0x0500 && curChar <= 0x052F) ||
      (curChar >= 0xA640 && curChar <= 0xA69F) ||
      (curChar >= 0x2DE0 && curChar <= 0x2DFF) || curChar == 8467 ||
      (curChar >= 0x2000 && curChar <= 0x206F)) {
    return false;
  }
  return true;
}

}  // namespace

CPDF_TextPageFind::CPDF_TextPageFind(const CPDF_TextPage* pTextPage)
    : m_pTextPage(pTextPage),
      m_flags(0),
      m_findNextStart(-1),
      m_findPreStart(-1),
      m_bMatchCase(false),
      m_bMatchWholeWord(false),
      m_resStart(0),
      m_resEnd(-1),
      m_IsFind(false) {
  m_strText = m_pTextPage->GetPageText();
  int nCount = pTextPage->CountChars();
  if (nCount)
    m_CharIndex.push_back(0);
  for (int i = 0; i < nCount; i++) {
    FPDF_CHAR_INFO info;
    pTextPage->GetCharInfo(i, &info);
    int indexSize = pdfium::CollectionSize<int>(m_CharIndex);
    if (info.m_Flag == FPDFTEXT_CHAR_NORMAL ||
        info.m_Flag == FPDFTEXT_CHAR_GENERATED) {
      if (indexSize % 2) {
        m_CharIndex.push_back(1);
      } else {
        if (indexSize <= 0)
          continue;
        m_CharIndex[indexSize - 1] += 1;
      }
    } else {
      if (indexSize % 2) {
        if (indexSize <= 0)
          continue;
        m_CharIndex[indexSize - 1] = i + 1;
      } else {
        m_CharIndex.push_back(i + 1);
      }
    }
  }
  int indexSize = pdfium::CollectionSize<int>(m_CharIndex);
  if (indexSize % 2)
    m_CharIndex.erase(m_CharIndex.begin() + indexSize - 1);
}

CPDF_TextPageFind::~CPDF_TextPageFind() {}

int CPDF_TextPageFind::GetCharIndex(int index) const {
  return m_pTextPage->CharIndexFromTextIndex(index);
}

bool CPDF_TextPageFind::FindFirst(const CFX_WideString& findwhat,
                                  int flags,
                                  int startPos) {
  if (!m_pTextPage)
    return false;
  if (m_strText.IsEmpty() || m_bMatchCase != (flags & FPDFTEXT_MATCHCASE))
    m_strText = m_pTextPage->GetPageText();
  CFX_WideString findwhatStr = findwhat;
  m_findWhat = findwhatStr;
  m_flags = flags;
  m_bMatchCase = flags & FPDFTEXT_MATCHCASE;
  if (m_strText.IsEmpty()) {
    m_IsFind = false;
    return true;
  }
  FX_STRSIZE len = findwhatStr.GetLength();
  if (!m_bMatchCase) {
    findwhatStr.MakeLower();
    m_strText.MakeLower();
  }
  m_bMatchWholeWord = !!(flags & FPDFTEXT_MATCHWHOLEWORD);
  m_findNextStart = startPos;
  if (startPos == -1)
    m_findPreStart = m_strText.GetLength() - 1;
  else
    m_findPreStart = startPos;
  m_csFindWhatArray.clear();
  int i = 0;
  while (i < len) {
    if (findwhatStr.GetAt(i) != ' ')
      break;
    i++;
  }
  if (i < len)
    ExtractFindWhat(findwhatStr);
  else
    m_csFindWhatArray.push_back(findwhatStr);
  if (m_csFindWhatArray.empty())
    return false;
  m_IsFind = true;
  m_resStart = 0;
  m_resEnd = -1;
  return true;
}

bool CPDF_TextPageFind::FindNext() {
  if (!m_pTextPage)
    return false;
  m_resArray.clear();
  if (m_findNextStart == -1)
    return false;
  if (m_strText.IsEmpty()) {
    m_IsFind = false;
    return m_IsFind;
  }
  int strLen = m_strText.GetLength();
  if (m_findNextStart > strLen - 1) {
    m_IsFind = false;
    return m_IsFind;
  }
  int nCount = pdfium::CollectionSize<int>(m_csFindWhatArray);
  int nResultPos = 0;
  int nStartPos = 0;
  nStartPos = m_findNextStart;
  bool bSpaceStart = false;
  for (int iWord = 0; iWord < nCount; iWord++) {
    CFX_WideString csWord = m_csFindWhatArray[iWord];
    if (csWord.IsEmpty()) {
      if (iWord == nCount - 1) {
        FX_WCHAR strInsert = m_strText.GetAt(nStartPos);
        if (strInsert == TEXT_LINEFEED_CHAR || strInsert == TEXT_SPACE_CHAR ||
            strInsert == TEXT_RETURN_CHAR || strInsert == 160) {
          nResultPos = nStartPos + 1;
          break;
        }
        iWord = -1;
      } else if (iWord == 0) {
        bSpaceStart = true;
      }
      continue;
    }
    int endIndex;
    nResultPos = m_strText.Find(csWord.c_str(), nStartPos);
    if (nResultPos == -1) {
      m_IsFind = false;
      return m_IsFind;
    }
    endIndex = nResultPos + csWord.GetLength() - 1;
    if (iWord == 0)
      m_resStart = nResultPos;
    bool bMatch = true;
    if (iWord != 0 && !bSpaceStart) {
      int PreResEndPos = nStartPos;
      int curChar = csWord.GetAt(0);
      CFX_WideString lastWord = m_csFindWhatArray[iWord - 1];
      int lastChar = lastWord.GetAt(lastWord.GetLength() - 1);
      if (nStartPos == nResultPos &&
          !(IsIgnoreSpaceCharacter(lastChar) ||
            IsIgnoreSpaceCharacter(curChar))) {
        bMatch = false;
      }
      for (int d = PreResEndPos; d < nResultPos; d++) {
        FX_WCHAR strInsert = m_strText.GetAt(d);
        if (strInsert != TEXT_LINEFEED_CHAR && strInsert != TEXT_SPACE_CHAR &&
            strInsert != TEXT_RETURN_CHAR && strInsert != 160) {
          bMatch = false;
          break;
        }
      }
    } else if (bSpaceStart) {
      if (nResultPos > 0) {
        FX_WCHAR strInsert = m_strText.GetAt(nResultPos - 1);
        if (strInsert != TEXT_LINEFEED_CHAR && strInsert != TEXT_SPACE_CHAR &&
            strInsert != TEXT_RETURN_CHAR && strInsert != 160) {
          bMatch = false;
          m_resStart = nResultPos;
        } else {
          m_resStart = nResultPos - 1;
        }
      }
    }
    if (m_bMatchWholeWord && bMatch) {
      bMatch = IsMatchWholeWord(m_strText, nResultPos, endIndex);
    }
    nStartPos = endIndex + 1;
    if (!bMatch) {
      iWord = -1;
      if (bSpaceStart)
        nStartPos = m_resStart + m_csFindWhatArray[1].GetLength();
      else
        nStartPos = m_resStart + m_csFindWhatArray[0].GetLength();
    }
  }
  m_resEnd = nResultPos + m_csFindWhatArray.back().GetLength() - 1;
  m_IsFind = true;
  int resStart = GetCharIndex(m_resStart);
  int resEnd = GetCharIndex(m_resEnd);
  m_resArray = m_pTextPage->GetRectArray(resStart, resEnd - resStart + 1);
  if (m_flags & FPDFTEXT_CONSECUTIVE) {
    m_findNextStart = m_resStart + 1;
    m_findPreStart = m_resEnd - 1;
  } else {
    m_findNextStart = m_resEnd + 1;
    m_findPreStart = m_resStart - 1;
  }
  return m_IsFind;
}

bool CPDF_TextPageFind::FindPrev() {
  if (!m_pTextPage)
    return false;
  m_resArray.clear();
  if (m_strText.IsEmpty() || m_findPreStart < 0) {
    m_IsFind = false;
    return m_IsFind;
  }
  CPDF_TextPageFind findEngine(m_pTextPage);
  bool ret = findEngine.FindFirst(m_findWhat, m_flags);
  if (!ret) {
    m_IsFind = false;
    return m_IsFind;
  }
  int order = -1, MatchedCount = 0;
  while (ret) {
    ret = findEngine.FindNext();
    if (ret) {
      int order1 = findEngine.GetCurOrder();
      int MatchedCount1 = findEngine.GetMatchedCount();
      if (((order1 + MatchedCount1) - 1) > m_findPreStart)
        break;
      order = order1;
      MatchedCount = MatchedCount1;
    }
  }
  if (order == -1) {
    m_IsFind = false;
    return m_IsFind;
  }
  m_resStart = m_pTextPage->TextIndexFromCharIndex(order);
  m_resEnd = m_pTextPage->TextIndexFromCharIndex(order + MatchedCount - 1);
  m_IsFind = true;
  m_resArray = m_pTextPage->GetRectArray(order, MatchedCount);
  if (m_flags & FPDFTEXT_CONSECUTIVE) {
    m_findNextStart = m_resStart + 1;
    m_findPreStart = m_resEnd - 1;
  } else {
    m_findNextStart = m_resEnd + 1;
    m_findPreStart = m_resStart - 1;
  }
  return m_IsFind;
}

void CPDF_TextPageFind::ExtractFindWhat(const CFX_WideString& findwhat) {
  if (findwhat.IsEmpty())
    return;
  int index = 0;
  while (1) {
    CFX_WideString csWord = TEXT_EMPTY;
    int ret =
        ExtractSubString(csWord, findwhat.c_str(), index, TEXT_SPACE_CHAR);
    if (csWord.IsEmpty()) {
      if (ret) {
        m_csFindWhatArray.push_back(L"");
        index++;
        continue;
      } else {
        break;
      }
    }
    int pos = 0;
    while (pos < csWord.GetLength()) {
      CFX_WideString curStr = csWord.Mid(pos, 1);
      FX_WCHAR curChar = csWord.GetAt(pos);
      if (IsIgnoreSpaceCharacter(curChar)) {
        if (pos > 0 && curChar == 0x2019) {
          pos++;
          continue;
        }
        if (pos > 0)
          m_csFindWhatArray.push_back(csWord.Mid(0, pos));
        m_csFindWhatArray.push_back(curStr);
        if (pos == csWord.GetLength() - 1) {
          csWord.clear();
          break;
        }
        csWord = csWord.Right(csWord.GetLength() - pos - 1);
        pos = 0;
        continue;
      }
      pos++;
    }
    if (!csWord.IsEmpty())
      m_csFindWhatArray.push_back(csWord);
    index++;
  }
}

bool CPDF_TextPageFind::IsMatchWholeWord(const CFX_WideString& csPageText,
                                         int startPos,
                                         int endPos) {
  FX_WCHAR char_left = 0;
  FX_WCHAR char_right = 0;
  int char_count = endPos - startPos + 1;
  if (char_count < 1)
    return false;
  if (char_count == 1 && csPageText.GetAt(startPos) > 255)
    return true;
  if (startPos - 1 >= 0)
    char_left = csPageText.GetAt(startPos - 1);
  if (startPos + char_count < csPageText.GetLength())
    char_right = csPageText.GetAt(startPos + char_count);
  if ((char_left > 'A' && char_left < 'a') ||
      (char_left > 'a' && char_left < 'z') ||
      (char_left > 0xfb00 && char_left < 0xfb06) || std::iswdigit(char_left) ||
      (char_right > 'A' && char_right < 'a') ||
      (char_right > 'a' && char_right < 'z') ||
      (char_right > 0xfb00 && char_right < 0xfb06) ||
      std::iswdigit(char_right)) {
    return false;
  }
  if (!(('A' > char_left || char_left > 'Z') &&
        ('a' > char_left || char_left > 'z') &&
        ('A' > char_right || char_right > 'Z') &&
        ('a' > char_right || char_right > 'z'))) {
    return false;
  }
  if (char_count > 0) {
    if (csPageText.GetAt(startPos) >= L'0' &&
        csPageText.GetAt(startPos) <= L'9' && char_left >= L'0' &&
        char_left <= L'9') {
      return false;
    }
    if (csPageText.GetAt(endPos) >= L'0' && csPageText.GetAt(endPos) <= L'9' &&
        char_right >= L'0' && char_right <= L'9') {
      return false;
    }
  }
  return true;
}

bool CPDF_TextPageFind::ExtractSubString(CFX_WideString& rString,
                                         const FX_WCHAR* lpszFullString,
                                         int iSubString,
                                         FX_WCHAR chSep) {
  if (!lpszFullString)
    return false;
  while (iSubString--) {
    lpszFullString = std::wcschr(lpszFullString, chSep);
    if (!lpszFullString) {
      rString.clear();
      return false;
    }
    lpszFullString++;
    while (*lpszFullString == chSep)
      lpszFullString++;
  }
  const FX_WCHAR* lpchEnd = std::wcschr(lpszFullString, chSep);
  int nLen = lpchEnd ? (int)(lpchEnd - lpszFullString)
                     : (int)FXSYS_wcslen(lpszFullString);
  ASSERT(nLen >= 0);
  FXSYS_memcpy(rString.GetBuffer(nLen), lpszFullString,
               nLen * sizeof(FX_WCHAR));
  rString.ReleaseBuffer();
  return true;
}

CFX_WideString CPDF_TextPageFind::MakeReverse(const CFX_WideString& str) {
  CFX_WideString str2;
  str2.clear();
  int nlen = str.GetLength();
  for (int i = nlen - 1; i >= 0; i--)
    str2 += str.GetAt(i);
  return str2;
}

int CPDF_TextPageFind::GetCurOrder() const {
  return GetCharIndex(m_resStart);
}

int CPDF_TextPageFind::GetMatchedCount() const {
  int resStart = GetCharIndex(m_resStart);
  int resEnd = GetCharIndex(m_resEnd);
  return resEnd - resStart + 1;
}
