// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <stddef.h>

#include <algorithm>
#include <cctype>

#include "core/fxcrt/cfx_string_pool_template.h"
#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_ext.h"
#include "third_party/base/numerics/safe_math.h"

template class CFX_StringDataTemplate<FX_WCHAR>;
template class CFX_StringCTemplate<FX_WCHAR>;
template class CFX_StringPoolTemplate<CFX_WideString>;
template struct std::hash<CFX_WideString>;

namespace {

#ifndef NDEBUG
bool IsValidCodePage(uint16_t codepage) {
  switch (codepage) {
    case 0:
    case 932:
    case 936:
    case 949:
    case 950:
      return true;

    default:
      return false;
  }
}
#endif

const FX_WCHAR* FX_wcsstr(const FX_WCHAR* haystack,
                          int haystack_len,
                          const FX_WCHAR* needle,
                          int needle_len) {
  if (needle_len > haystack_len || needle_len == 0) {
    return nullptr;
  }
  const FX_WCHAR* end_ptr = haystack + haystack_len - needle_len;
  while (haystack <= end_ptr) {
    int i = 0;
    while (1) {
      if (haystack[i] != needle[i]) {
        break;
      }
      i++;
      if (i == needle_len) {
        return haystack;
      }
    }
    haystack++;
  }
  return nullptr;
}

}  // namespace

static_assert(sizeof(CFX_WideString) <= sizeof(FX_WCHAR*),
              "Strings must not require more space than pointers");

CFX_WideString::CFX_WideString() {}

CFX_WideString::CFX_WideString(const CFX_WideString& other)
    : m_pData(other.m_pData) {}

CFX_WideString::CFX_WideString(CFX_WideString&& other) {
  m_pData.Swap(other.m_pData);
}

CFX_WideString::CFX_WideString(const FX_WCHAR* pStr, FX_STRSIZE nLen) {
  if (nLen < 0)
    nLen = pStr ? FXSYS_wcslen(pStr) : 0;

  if (nLen)
    m_pData.Reset(StringData::Create(pStr, nLen));
}

CFX_WideString::CFX_WideString(FX_WCHAR ch) {
  m_pData.Reset(StringData::Create(1));
  m_pData->m_String[0] = ch;
}

CFX_WideString::CFX_WideString(const FX_WCHAR* ptr)
    : CFX_WideString(ptr, ptr ? FXSYS_wcslen(ptr) : 0) {}

CFX_WideString::CFX_WideString(const CFX_WideStringC& stringSrc) {
  if (!stringSrc.IsEmpty()) {
    m_pData.Reset(StringData::Create(stringSrc.c_str(), stringSrc.GetLength()));
  }
}

CFX_WideString::CFX_WideString(const CFX_WideStringC& str1,
                               const CFX_WideStringC& str2) {
  int nNewLen = str1.GetLength() + str2.GetLength();
  if (nNewLen == 0)
    return;

  m_pData.Reset(StringData::Create(nNewLen));
  m_pData->CopyContents(str1.c_str(), str1.GetLength());
  m_pData->CopyContentsAt(str1.GetLength(), str2.c_str(), str2.GetLength());
}

CFX_WideString::~CFX_WideString() {}

const CFX_WideString& CFX_WideString::operator=(const FX_WCHAR* pStr) {
  if (!pStr || !pStr[0])
    clear();
  else
    AssignCopy(pStr, FXSYS_wcslen(pStr));

  return *this;
}

const CFX_WideString& CFX_WideString::operator=(
    const CFX_WideStringC& stringSrc) {
  if (stringSrc.IsEmpty())
    clear();
  else
    AssignCopy(stringSrc.c_str(), stringSrc.GetLength());

  return *this;
}

const CFX_WideString& CFX_WideString::operator=(
    const CFX_WideString& stringSrc) {
  if (m_pData != stringSrc.m_pData)
    m_pData = stringSrc.m_pData;

  return *this;
}

const CFX_WideString& CFX_WideString::operator+=(const FX_WCHAR* pStr) {
  if (pStr)
    Concat(pStr, FXSYS_wcslen(pStr));

  return *this;
}

const CFX_WideString& CFX_WideString::operator+=(FX_WCHAR ch) {
  Concat(&ch, 1);
  return *this;
}

const CFX_WideString& CFX_WideString::operator+=(const CFX_WideString& str) {
  if (str.m_pData)
    Concat(str.m_pData->m_String, str.m_pData->m_nDataLength);

  return *this;
}

const CFX_WideString& CFX_WideString::operator+=(const CFX_WideStringC& str) {
  if (!str.IsEmpty())
    Concat(str.c_str(), str.GetLength());

  return *this;
}

bool CFX_WideString::operator==(const wchar_t* ptr) const {
  if (!m_pData)
    return !ptr || !ptr[0];

  if (!ptr)
    return m_pData->m_nDataLength == 0;

  return wcslen(ptr) == static_cast<size_t>(m_pData->m_nDataLength) &&
         wmemcmp(ptr, m_pData->m_String, m_pData->m_nDataLength) == 0;
}

bool CFX_WideString::operator==(const CFX_WideStringC& str) const {
  if (!m_pData)
    return str.IsEmpty();

  return m_pData->m_nDataLength == str.GetLength() &&
         wmemcmp(m_pData->m_String, str.c_str(), str.GetLength()) == 0;
}

bool CFX_WideString::operator==(const CFX_WideString& other) const {
  if (m_pData == other.m_pData)
    return true;

  if (IsEmpty())
    return other.IsEmpty();

  if (other.IsEmpty())
    return false;

  return other.m_pData->m_nDataLength == m_pData->m_nDataLength &&
         wmemcmp(other.m_pData->m_String, m_pData->m_String,
                 m_pData->m_nDataLength) == 0;
}

bool CFX_WideString::operator<(const CFX_WideString& str) const {
  if (m_pData == str.m_pData)
    return false;

  int result =
      wmemcmp(c_str(), str.c_str(), std::min(GetLength(), str.GetLength()));
  return result < 0 || (result == 0 && GetLength() < str.GetLength());
}

void CFX_WideString::AssignCopy(const FX_WCHAR* pSrcData, FX_STRSIZE nSrcLen) {
  AllocBeforeWrite(nSrcLen);
  m_pData->CopyContents(pSrcData, nSrcLen);
  m_pData->m_nDataLength = nSrcLen;
}

void CFX_WideString::ReallocBeforeWrite(FX_STRSIZE nNewLength) {
  if (m_pData && m_pData->CanOperateInPlace(nNewLength))
    return;

  if (nNewLength <= 0) {
    clear();
    return;
  }

  CFX_RetainPtr<StringData> pNewData(StringData::Create(nNewLength));
  if (m_pData) {
    FX_STRSIZE nCopyLength = std::min(m_pData->m_nDataLength, nNewLength);
    pNewData->CopyContents(m_pData->m_String, nCopyLength);
    pNewData->m_nDataLength = nCopyLength;
  } else {
    pNewData->m_nDataLength = 0;
  }
  pNewData->m_String[pNewData->m_nDataLength] = 0;
  m_pData.Swap(pNewData);
}

void CFX_WideString::AllocBeforeWrite(FX_STRSIZE nNewLength) {
  if (m_pData && m_pData->CanOperateInPlace(nNewLength))
    return;

  if (nNewLength <= 0) {
    clear();
    return;
  }

  m_pData.Reset(StringData::Create(nNewLength));
}

void CFX_WideString::ReleaseBuffer(FX_STRSIZE nNewLength) {
  if (!m_pData)
    return;

  if (nNewLength == -1)
    nNewLength = FXSYS_wcslen(m_pData->m_String);

  nNewLength = std::min(nNewLength, m_pData->m_nAllocLength);
  if (nNewLength == 0) {
    clear();
    return;
  }

  ASSERT(m_pData->m_nRefs == 1);
  m_pData->m_nDataLength = nNewLength;
  m_pData->m_String[nNewLength] = 0;
  if (m_pData->m_nAllocLength - nNewLength >= 32) {
    // Over arbitrary threshold, so pay the price to relocate.  Force copy to
    // always occur by holding a second reference to the string.
    CFX_WideString preserve(*this);
    ReallocBeforeWrite(nNewLength);
  }
}

void CFX_WideString::Reserve(FX_STRSIZE len) {
  GetBuffer(len);
}

FX_WCHAR* CFX_WideString::GetBuffer(FX_STRSIZE nMinBufLength) {
  if (!m_pData) {
    if (nMinBufLength == 0)
      return nullptr;

    m_pData.Reset(StringData::Create(nMinBufLength));
    m_pData->m_nDataLength = 0;
    m_pData->m_String[0] = 0;
    return m_pData->m_String;
  }

  if (m_pData->CanOperateInPlace(nMinBufLength))
    return m_pData->m_String;

  nMinBufLength = std::max(nMinBufLength, m_pData->m_nDataLength);
  if (nMinBufLength == 0)
    return nullptr;

  CFX_RetainPtr<StringData> pNewData(StringData::Create(nMinBufLength));
  pNewData->CopyContents(*m_pData);
  pNewData->m_nDataLength = m_pData->m_nDataLength;
  m_pData.Swap(pNewData);
  return m_pData->m_String;
}

FX_STRSIZE CFX_WideString::Delete(FX_STRSIZE nIndex, FX_STRSIZE nCount) {
  if (!m_pData)
    return 0;

  if (nIndex < 0)
    nIndex = 0;

  FX_STRSIZE nOldLength = m_pData->m_nDataLength;
  if (nCount > 0 && nIndex < nOldLength) {
    FX_STRSIZE mLength = nIndex + nCount;
    if (mLength >= nOldLength) {
      m_pData->m_nDataLength = nIndex;
      return m_pData->m_nDataLength;
    }
    ReallocBeforeWrite(nOldLength);
    int nCharsToCopy = nOldLength - mLength + 1;
    wmemmove(m_pData->m_String + nIndex, m_pData->m_String + mLength,
             nCharsToCopy);
    m_pData->m_nDataLength = nOldLength - nCount;
  }
  return m_pData->m_nDataLength;
}

void CFX_WideString::Concat(const FX_WCHAR* pSrcData, FX_STRSIZE nSrcLen) {
  if (!pSrcData || nSrcLen <= 0)
    return;

  if (!m_pData) {
    m_pData.Reset(StringData::Create(pSrcData, nSrcLen));
    return;
  }

  if (m_pData->CanOperateInPlace(m_pData->m_nDataLength + nSrcLen)) {
    m_pData->CopyContentsAt(m_pData->m_nDataLength, pSrcData, nSrcLen);
    m_pData->m_nDataLength += nSrcLen;
    return;
  }

  CFX_RetainPtr<StringData> pNewData(
      StringData::Create(m_pData->m_nDataLength + nSrcLen));
  pNewData->CopyContents(*m_pData);
  pNewData->CopyContentsAt(m_pData->m_nDataLength, pSrcData, nSrcLen);
  m_pData.Swap(pNewData);
}

CFX_ByteString CFX_WideString::UTF8Encode() const {
  return FX_UTF8Encode(AsStringC());
}

CFX_ByteString CFX_WideString::UTF16LE_Encode() const {
  if (!m_pData) {
    return CFX_ByteString("\0\0", 2);
  }
  int len = m_pData->m_nDataLength;
  CFX_ByteString result;
  FX_CHAR* buffer = result.GetBuffer(len * 2 + 2);
  for (int i = 0; i < len; i++) {
    buffer[i * 2] = m_pData->m_String[i] & 0xff;
    buffer[i * 2 + 1] = m_pData->m_String[i] >> 8;
  }
  buffer[len * 2] = 0;
  buffer[len * 2 + 1] = 0;
  result.ReleaseBuffer(len * 2 + 2);
  return result;
}

CFX_WideString CFX_WideString::Mid(FX_STRSIZE nFirst) const {
  if (!m_pData)
    return CFX_WideString();

  return Mid(nFirst, m_pData->m_nDataLength - nFirst);
}

CFX_WideString CFX_WideString::Mid(FX_STRSIZE nFirst, FX_STRSIZE nCount) const {
  if (!m_pData)
    return CFX_WideString();

  nFirst = std::min(std::max(nFirst, 0), m_pData->m_nDataLength);
  nCount = std::min(std::max(nCount, 0), m_pData->m_nDataLength - nFirst);
  if (nCount == 0)
    return CFX_WideString();

  if (nFirst == 0 && nCount == m_pData->m_nDataLength)
    return *this;

  CFX_WideString dest;
  AllocCopy(dest, nCount, nFirst);
  return dest;
}

void CFX_WideString::AllocCopy(CFX_WideString& dest,
                               FX_STRSIZE nCopyLen,
                               FX_STRSIZE nCopyIndex) const {
  if (nCopyLen <= 0)
    return;

  CFX_RetainPtr<StringData> pNewData(
      StringData::Create(m_pData->m_String + nCopyIndex, nCopyLen));
  dest.m_pData.Swap(pNewData);
}

#define FORCE_ANSI 0x10000
#define FORCE_UNICODE 0x20000
#define FORCE_INT64 0x40000

void CFX_WideString::FormatV(const FX_WCHAR* pFormat, va_list argList) {
  va_list argListSave;
#if defined(__ARMCC_VERSION) ||                                              \
    (!defined(_MSC_VER) && (_FX_CPU_ == _FX_X64_ || _FX_CPU_ == _FX_IA64_ || \
                            _FX_CPU_ == _FX_ARM64_)) ||                      \
    defined(__native_client__)
  va_copy(argListSave, argList);
#else
  argListSave = argList;
#endif
  int nMaxLen = 0;
  for (const FX_WCHAR* pStr = pFormat; *pStr != 0; pStr++) {
    if (*pStr != '%' || *(pStr = pStr + 1) == '%') {
      nMaxLen += FXSYS_wcslen(pStr);
      continue;
    }
    int nItemLen = 0;
    int nWidth = 0;
    for (; *pStr != 0; pStr++) {
      if (*pStr == '#') {
        nMaxLen += 2;
      } else if (*pStr == '*') {
        nWidth = va_arg(argList, int);
      } else if (*pStr != '-' && *pStr != '+' && *pStr != '0' && *pStr != ' ') {
        break;
      }
    }
    if (nWidth == 0) {
      nWidth = FXSYS_wtoi(pStr);
      while (std::iswdigit(*pStr))
        ++pStr;
    }
    if (nWidth < 0 || nWidth > 128 * 1024) {
      pFormat = L"Bad width";
      nMaxLen = 10;
      break;
    }
    int nPrecision = 0;
    if (*pStr == '.') {
      pStr++;
      if (*pStr == '*') {
        nPrecision = va_arg(argList, int);
        pStr++;
      } else {
        nPrecision = FXSYS_wtoi(pStr);
        while (std::iswdigit(*pStr))
          ++pStr;
      }
    }
    if (nPrecision < 0 || nPrecision > 128 * 1024) {
      pFormat = L"Bad precision";
      nMaxLen = 14;
      break;
    }
    int nModifier = 0;
    if (*pStr == L'I' && *(pStr + 1) == L'6' && *(pStr + 2) == L'4') {
      pStr += 3;
      nModifier = FORCE_INT64;
    } else {
      switch (*pStr) {
        case 'h':
          nModifier = FORCE_ANSI;
          pStr++;
          break;
        case 'l':
          nModifier = FORCE_UNICODE;
          pStr++;
          break;
        case 'F':
        case 'N':
        case 'L':
          pStr++;
          break;
      }
    }
    switch (*pStr | nModifier) {
      case 'c':
      case 'C':
        nItemLen = 2;
        va_arg(argList, int);
        break;
      case 'c' | FORCE_ANSI:
      case 'C' | FORCE_ANSI:
        nItemLen = 2;
        va_arg(argList, int);
        break;
      case 'c' | FORCE_UNICODE:
      case 'C' | FORCE_UNICODE:
        nItemLen = 2;
        va_arg(argList, int);
        break;
      case 's': {
        const FX_WCHAR* pstrNextArg = va_arg(argList, const FX_WCHAR*);
        if (pstrNextArg) {
          nItemLen = FXSYS_wcslen(pstrNextArg);
          if (nItemLen < 1) {
            nItemLen = 1;
          }
        } else {
          nItemLen = 6;
        }
      } break;
      case 'S': {
        const FX_CHAR* pstrNextArg = va_arg(argList, const FX_CHAR*);
        if (pstrNextArg) {
          nItemLen = FXSYS_strlen(pstrNextArg);
          if (nItemLen < 1) {
            nItemLen = 1;
          }
        } else {
          nItemLen = 6;
        }
      } break;
      case 's' | FORCE_ANSI:
      case 'S' | FORCE_ANSI: {
        const FX_CHAR* pstrNextArg = va_arg(argList, const FX_CHAR*);
        if (pstrNextArg) {
          nItemLen = FXSYS_strlen(pstrNextArg);
          if (nItemLen < 1) {
            nItemLen = 1;
          }
        } else {
          nItemLen = 6;
        }
      } break;
      case 's' | FORCE_UNICODE:
      case 'S' | FORCE_UNICODE: {
        const FX_WCHAR* pstrNextArg = va_arg(argList, FX_WCHAR*);
        if (pstrNextArg) {
          nItemLen = FXSYS_wcslen(pstrNextArg);
          if (nItemLen < 1) {
            nItemLen = 1;
          }
        } else {
          nItemLen = 6;
        }
      } break;
    }
    if (nItemLen != 0) {
      if (nPrecision != 0 && nItemLen > nPrecision) {
        nItemLen = nPrecision;
      }
      if (nItemLen < nWidth) {
        nItemLen = nWidth;
      }
    } else {
      switch (*pStr) {
        case 'd':
        case 'i':
        case 'u':
        case 'x':
        case 'X':
        case 'o':
          if (nModifier & FORCE_INT64) {
            va_arg(argList, int64_t);
          } else {
            va_arg(argList, int);
          }
          nItemLen = 32;
          if (nItemLen < nWidth + nPrecision) {
            nItemLen = nWidth + nPrecision;
          }
          break;
        case 'a':
        case 'A':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
          va_arg(argList, double);
          nItemLen = 128;
          if (nItemLen < nWidth + nPrecision) {
            nItemLen = nWidth + nPrecision;
          }
          break;
        case 'f':
          if (nWidth + nPrecision > 100) {
            nItemLen = nPrecision + nWidth + 128;
          } else {
            double f;
            char pszTemp[256];
            f = va_arg(argList, double);
            FXSYS_snprintf(pszTemp, sizeof(pszTemp), "%*.*f", nWidth,
                           nPrecision + 6, f);
            nItemLen = FXSYS_strlen(pszTemp);
          }
          break;
        case 'p':
          va_arg(argList, void*);
          nItemLen = 32;
          if (nItemLen < nWidth + nPrecision) {
            nItemLen = nWidth + nPrecision;
          }
          break;
        case 'n':
          va_arg(argList, int*);
          break;
      }
    }
    nMaxLen += nItemLen;
  }
  GetBuffer(nMaxLen);
  if (m_pData) {
    FXSYS_vswprintf((wchar_t*)m_pData->m_String, nMaxLen + 1,
                    (const wchar_t*)pFormat, argListSave);
    ReleaseBuffer();
  }
  va_end(argListSave);
}

void CFX_WideString::Format(const FX_WCHAR* pFormat, ...) {
  va_list argList;
  va_start(argList, pFormat);
  FormatV(pFormat, argList);
  va_end(argList);
}

FX_STRSIZE CFX_WideString::Insert(FX_STRSIZE nIndex, FX_WCHAR ch) {
  FX_STRSIZE nNewLength = m_pData ? m_pData->m_nDataLength : 0;
  nIndex = std::max(nIndex, 0);
  nIndex = std::min(nIndex, nNewLength);
  nNewLength++;

  ReallocBeforeWrite(nNewLength);
  wmemmove(m_pData->m_String + nIndex + 1, m_pData->m_String + nIndex,
           nNewLength - nIndex);
  m_pData->m_String[nIndex] = ch;
  m_pData->m_nDataLength = nNewLength;
  return nNewLength;
}

CFX_WideString CFX_WideString::Right(FX_STRSIZE nCount) const {
  if (!m_pData)
    return CFX_WideString();

  nCount = std::max(nCount, 0);
  if (nCount >= m_pData->m_nDataLength)
    return *this;

  CFX_WideString dest;
  AllocCopy(dest, nCount, m_pData->m_nDataLength - nCount);
  return dest;
}

CFX_WideString CFX_WideString::Left(FX_STRSIZE nCount) const {
  if (!m_pData)
    return CFX_WideString();

  nCount = std::max(nCount, 0);
  if (nCount >= m_pData->m_nDataLength)
    return *this;

  CFX_WideString dest;
  AllocCopy(dest, nCount, 0);
  return dest;
}

FX_STRSIZE CFX_WideString::Find(FX_WCHAR ch, FX_STRSIZE nStart) const {
  if (!m_pData)
    return -1;

  if (nStart < 0 || nStart >= m_pData->m_nDataLength)
    return -1;

  const FX_WCHAR* pStr =
      wmemchr(m_pData->m_String + nStart, ch, m_pData->m_nDataLength - nStart);
  return pStr ? pStr - m_pData->m_String : -1;
}

FX_STRSIZE CFX_WideString::Find(const CFX_WideStringC& pSub,
                                FX_STRSIZE nStart) const {
  if (!m_pData)
    return -1;

  FX_STRSIZE nLength = m_pData->m_nDataLength;
  if (nStart > nLength)
    return -1;

  const FX_WCHAR* pStr =
      FX_wcsstr(m_pData->m_String + nStart, m_pData->m_nDataLength - nStart,
                pSub.c_str(), pSub.GetLength());
  return pStr ? (int)(pStr - m_pData->m_String) : -1;
}

void CFX_WideString::MakeLower() {
  if (!m_pData)
    return;

  ReallocBeforeWrite(m_pData->m_nDataLength);
  FXSYS_wcslwr(m_pData->m_String);
}

void CFX_WideString::MakeUpper() {
  if (!m_pData)
    return;

  ReallocBeforeWrite(m_pData->m_nDataLength);
  FXSYS_wcsupr(m_pData->m_String);
}

FX_STRSIZE CFX_WideString::Remove(FX_WCHAR chRemove) {
  if (!m_pData || m_pData->m_nDataLength < 1)
    return 0;

  FX_WCHAR* pstrSource = m_pData->m_String;
  FX_WCHAR* pstrEnd = m_pData->m_String + m_pData->m_nDataLength;
  while (pstrSource < pstrEnd) {
    if (*pstrSource == chRemove)
      break;
    pstrSource++;
  }
  if (pstrSource == pstrEnd)
    return 0;

  ptrdiff_t copied = pstrSource - m_pData->m_String;
  ReallocBeforeWrite(m_pData->m_nDataLength);
  pstrSource = m_pData->m_String + copied;
  pstrEnd = m_pData->m_String + m_pData->m_nDataLength;

  FX_WCHAR* pstrDest = pstrSource;
  while (pstrSource < pstrEnd) {
    if (*pstrSource != chRemove) {
      *pstrDest = *pstrSource;
      pstrDest++;
    }
    pstrSource++;
  }

  *pstrDest = 0;
  FX_STRSIZE nCount = (FX_STRSIZE)(pstrSource - pstrDest);
  m_pData->m_nDataLength -= nCount;
  return nCount;
}

FX_STRSIZE CFX_WideString::Replace(const CFX_WideStringC& pOld,
                                   const CFX_WideStringC& pNew) {
  if (!m_pData || pOld.IsEmpty())
    return 0;

  FX_STRSIZE nSourceLen = pOld.GetLength();
  FX_STRSIZE nReplacementLen = pNew.GetLength();
  FX_STRSIZE nCount = 0;
  const FX_WCHAR* pStart = m_pData->m_String;
  FX_WCHAR* pEnd = m_pData->m_String + m_pData->m_nDataLength;
  while (1) {
    const FX_WCHAR* pTarget = FX_wcsstr(pStart, (FX_STRSIZE)(pEnd - pStart),
                                        pOld.c_str(), nSourceLen);
    if (!pTarget)
      break;

    nCount++;
    pStart = pTarget + nSourceLen;
  }
  if (nCount == 0)
    return 0;

  FX_STRSIZE nNewLength =
      m_pData->m_nDataLength + (nReplacementLen - nSourceLen) * nCount;

  if (nNewLength == 0) {
    clear();
    return nCount;
  }

  CFX_RetainPtr<StringData> pNewData(StringData::Create(nNewLength));
  pStart = m_pData->m_String;
  FX_WCHAR* pDest = pNewData->m_String;
  for (FX_STRSIZE i = 0; i < nCount; i++) {
    const FX_WCHAR* pTarget = FX_wcsstr(pStart, (FX_STRSIZE)(pEnd - pStart),
                                        pOld.c_str(), nSourceLen);
    wmemcpy(pDest, pStart, pTarget - pStart);
    pDest += pTarget - pStart;
    wmemcpy(pDest, pNew.c_str(), pNew.GetLength());
    pDest += pNew.GetLength();
    pStart = pTarget + nSourceLen;
  }
  wmemcpy(pDest, pStart, pEnd - pStart);
  m_pData.Swap(pNewData);
  return nCount;
}

void CFX_WideString::SetAt(FX_STRSIZE nIndex, FX_WCHAR ch) {
  if (!m_pData) {
    return;
  }
  ASSERT(nIndex >= 0);
  ASSERT(nIndex < m_pData->m_nDataLength);
  ReallocBeforeWrite(m_pData->m_nDataLength);
  m_pData->m_String[nIndex] = ch;
}

// static
CFX_WideString CFX_WideString::FromLocal(const CFX_ByteStringC& str) {
  return FromCodePage(str, 0);
}

// static
CFX_WideString CFX_WideString::FromCodePage(const CFX_ByteStringC& str,
                                            uint16_t codepage) {
  return CFX_CharMap::GetWideString(codepage, str);
}

// static
CFX_WideString CFX_WideString::FromUTF8(const CFX_ByteStringC& str) {
  if (str.IsEmpty())
    return CFX_WideString();

  CFX_UTF8Decoder decoder;
  for (FX_STRSIZE i = 0; i < str.GetLength(); i++)
    decoder.Input(str[i]);

  return CFX_WideString(decoder.GetResult());
}

// static
CFX_WideString CFX_WideString::FromUTF16LE(const unsigned short* wstr,
                                           FX_STRSIZE wlen) {
  if (!wstr || 0 == wlen) {
    return CFX_WideString();
  }

  CFX_WideString result;
  FX_WCHAR* buf = result.GetBuffer(wlen);
  for (int i = 0; i < wlen; i++) {
    buf[i] = wstr[i];
  }
  result.ReleaseBuffer(wlen);
  return result;
}

int CFX_WideString::Compare(const FX_WCHAR* lpsz) const {
  if (m_pData)
    return FXSYS_wcscmp(m_pData->m_String, lpsz);
  return (!lpsz || lpsz[0] == 0) ? 0 : -1;
}

int CFX_WideString::Compare(const CFX_WideString& str) const {
  if (!m_pData) {
    if (!str.m_pData) {
      return 0;
    }
    return -1;
  }
  if (!str.m_pData) {
    return 1;
  }
  int this_len = m_pData->m_nDataLength;
  int that_len = str.m_pData->m_nDataLength;
  int min_len = this_len < that_len ? this_len : that_len;
  for (int i = 0; i < min_len; i++) {
    if (m_pData->m_String[i] < str.m_pData->m_String[i]) {
      return -1;
    }
    if (m_pData->m_String[i] > str.m_pData->m_String[i]) {
      return 1;
    }
  }
  if (this_len < that_len) {
    return -1;
  }
  if (this_len > that_len) {
    return 1;
  }
  return 0;
}

int CFX_WideString::CompareNoCase(const FX_WCHAR* lpsz) const {
  if (!m_pData) {
    return (!lpsz || lpsz[0] == 0) ? 0 : -1;
  }
  return FXSYS_wcsicmp(m_pData->m_String, lpsz);
}

FX_STRSIZE CFX_WideString::WStringLength(const unsigned short* str) {
  FX_STRSIZE len = 0;
  if (str)
    while (str[len])
      len++;
  return len;
}

void CFX_WideString::TrimRight(const CFX_WideStringC& pTargets) {
  if (IsEmpty() || pTargets.IsEmpty())
    return;

  FX_STRSIZE pos = GetLength();
  while (pos && pTargets.Find(m_pData->m_String[pos - 1]) != -1)
    pos--;

  if (pos < m_pData->m_nDataLength) {
    ReallocBeforeWrite(m_pData->m_nDataLength);
    m_pData->m_String[pos] = 0;
    m_pData->m_nDataLength = pos;
  }
}

void CFX_WideString::TrimRight(FX_WCHAR chTarget) {
  FX_WCHAR str[2] = {chTarget, 0};
  TrimRight(str);
}

void CFX_WideString::TrimRight() {
  TrimRight(L"\x09\x0a\x0b\x0c\x0d\x20");
}

void CFX_WideString::TrimLeft(const CFX_WideStringC& pTargets) {
  if (!m_pData || pTargets.IsEmpty())
    return;

  FX_STRSIZE len = GetLength();
  if (len < 1)
    return;

  FX_STRSIZE pos = 0;
  while (pos < len) {
    FX_STRSIZE i = 0;
    while (i < pTargets.GetLength() &&
           pTargets.CharAt(i) != m_pData->m_String[pos]) {
      i++;
    }
    if (i == pTargets.GetLength()) {
      break;
    }
    pos++;
  }
  if (pos) {
    ReallocBeforeWrite(len);
    FX_STRSIZE nDataLength = len - pos;
    FXSYS_memmove(m_pData->m_String, m_pData->m_String + pos,
                  (nDataLength + 1) * sizeof(FX_WCHAR));
    m_pData->m_nDataLength = nDataLength;
  }
}

void CFX_WideString::TrimLeft(FX_WCHAR chTarget) {
  FX_WCHAR str[2] = {chTarget, 0};
  TrimLeft(str);
}

void CFX_WideString::TrimLeft() {
  TrimLeft(L"\x09\x0a\x0b\x0c\x0d\x20");
}
FX_FLOAT FX_wtof(const FX_WCHAR* str, int len) {
  if (len == 0) {
    return 0.0;
  }
  int cc = 0;
  bool bNegative = false;
  if (str[0] == '+') {
    cc++;
  } else if (str[0] == '-') {
    bNegative = true;
    cc++;
  }
  int integer = 0;
  while (cc < len) {
    if (str[cc] == '.') {
      break;
    }
    integer = integer * 10 + FXSYS_toDecimalDigit(str[cc]);
    cc++;
  }
  FX_FLOAT fraction = 0;
  if (str[cc] == '.') {
    cc++;
    FX_FLOAT scale = 0.1f;
    while (cc < len) {
      fraction += scale * FXSYS_toDecimalDigit(str[cc]);
      scale *= 0.1f;
      cc++;
    }
  }
  fraction += (FX_FLOAT)integer;
  return bNegative ? -fraction : fraction;
}

int CFX_WideString::GetInteger() const {
  return m_pData ? FXSYS_wtoi(m_pData->m_String) : 0;
}

FX_FLOAT CFX_WideString::GetFloat() const {
  return m_pData ? FX_wtof(m_pData->m_String, m_pData->m_nDataLength) : 0.0f;
}

// static
CFX_ByteString CFX_CharMap::GetByteString(uint16_t codepage,
                                          const CFX_WideStringC& wstr) {
  ASSERT(IsValidCodePage(codepage));
  int src_len = wstr.GetLength();
  int dest_len = FXSYS_WideCharToMultiByte(codepage, 0, wstr.c_str(), src_len,
                                           nullptr, 0, nullptr, nullptr);
  CFX_ByteString bstr;
  if (dest_len) {
    FX_CHAR* dest_buf = bstr.GetBuffer(dest_len);
    FXSYS_WideCharToMultiByte(codepage, 0, wstr.c_str(), src_len, dest_buf,
                              dest_len, nullptr, nullptr);
    bstr.ReleaseBuffer(dest_len);
  }
  return bstr;
}

// static
CFX_WideString CFX_CharMap::GetWideString(uint16_t codepage,
                                          const CFX_ByteStringC& bstr) {
  ASSERT(IsValidCodePage(codepage));
  int src_len = bstr.GetLength();
  int dest_len =
      FXSYS_MultiByteToWideChar(codepage, 0, bstr.c_str(), src_len, nullptr, 0);
  CFX_WideString wstr;
  if (dest_len) {
    FX_WCHAR* dest_buf = wstr.GetBuffer(dest_len);
    FXSYS_MultiByteToWideChar(codepage, 0, bstr.c_str(), src_len, dest_buf,
                              dest_len);
    wstr.ReleaseBuffer(dest_len);
  }
  return wstr;
}
