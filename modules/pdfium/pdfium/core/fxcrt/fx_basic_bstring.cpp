// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <stddef.h>

#include <algorithm>
#include <cctype>

#include "core/fxcrt/cfx_string_pool_template.h"
#include "core/fxcrt/fx_basic.h"
#include "third_party/base/numerics/safe_math.h"

template class CFX_StringDataTemplate<FX_CHAR>;
template class CFX_StringCTemplate<FX_CHAR>;
template class CFX_StringPoolTemplate<CFX_ByteString>;
template struct std::hash<CFX_ByteString>;

namespace {

int Buffer_itoa(char* buf, int i, uint32_t flags) {
  if (i == 0) {
    buf[0] = '0';
    return 1;
  }
  char buf1[32];
  int buf_pos = 31;
  uint32_t u = i;
  if ((flags & FXFORMAT_SIGNED) && i < 0) {
    u = -i;
  }
  int base = 10;
  const FX_CHAR* str = "0123456789abcdef";
  if (flags & FXFORMAT_HEX) {
    base = 16;
    if (flags & FXFORMAT_CAPITAL) {
      str = "0123456789ABCDEF";
    }
  }
  while (u != 0) {
    buf1[buf_pos--] = str[u % base];
    u = u / base;
  }
  if ((flags & FXFORMAT_SIGNED) && i < 0) {
    buf1[buf_pos--] = '-';
  }
  int len = 31 - buf_pos;
  for (int ii = 0; ii < len; ii++) {
    buf[ii] = buf1[ii + buf_pos + 1];
  }
  return len;
}

const FX_CHAR* FX_strstr(const FX_CHAR* haystack,
                         int haystack_len,
                         const FX_CHAR* needle,
                         int needle_len) {
  if (needle_len > haystack_len || needle_len == 0) {
    return nullptr;
  }
  const FX_CHAR* end_ptr = haystack + haystack_len - needle_len;
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

static_assert(sizeof(CFX_ByteString) <= sizeof(FX_CHAR*),
              "Strings must not require more space than pointers");

CFX_ByteString::CFX_ByteString(const FX_CHAR* pStr, FX_STRSIZE nLen) {
  if (nLen < 0)
    nLen = pStr ? FXSYS_strlen(pStr) : 0;

  if (nLen)
    m_pData.Reset(StringData::Create(pStr, nLen));
}

CFX_ByteString::CFX_ByteString(const uint8_t* pStr, FX_STRSIZE nLen) {
  if (nLen > 0) {
    m_pData.Reset(
        StringData::Create(reinterpret_cast<const FX_CHAR*>(pStr), nLen));
  }
}

CFX_ByteString::CFX_ByteString() {}

CFX_ByteString::CFX_ByteString(const CFX_ByteString& other)
    : m_pData(other.m_pData) {}

CFX_ByteString::CFX_ByteString(CFX_ByteString&& other) {
  m_pData.Swap(other.m_pData);
}

CFX_ByteString::CFX_ByteString(char ch) {
  m_pData.Reset(StringData::Create(1));
  m_pData->m_String[0] = ch;
}

CFX_ByteString::CFX_ByteString(const FX_CHAR* ptr)
    : CFX_ByteString(ptr, ptr ? FXSYS_strlen(ptr) : 0) {}

CFX_ByteString::CFX_ByteString(const CFX_ByteStringC& stringSrc) {
  if (!stringSrc.IsEmpty())
    m_pData.Reset(StringData::Create(stringSrc.c_str(), stringSrc.GetLength()));
}

CFX_ByteString::CFX_ByteString(const CFX_ByteStringC& str1,
                               const CFX_ByteStringC& str2) {
  int nNewLen = str1.GetLength() + str2.GetLength();
  if (nNewLen == 0)
    return;

  m_pData.Reset(StringData::Create(nNewLen));
  m_pData->CopyContents(str1.c_str(), str1.GetLength());
  m_pData->CopyContentsAt(str1.GetLength(), str2.c_str(), str2.GetLength());
}

CFX_ByteString::~CFX_ByteString() {}

const CFX_ByteString& CFX_ByteString::operator=(const FX_CHAR* pStr) {
  if (!pStr || !pStr[0])
    clear();
  else
    AssignCopy(pStr, FXSYS_strlen(pStr));

  return *this;
}

const CFX_ByteString& CFX_ByteString::operator=(
    const CFX_ByteStringC& stringSrc) {
  if (stringSrc.IsEmpty())
    clear();
  else
    AssignCopy(stringSrc.c_str(), stringSrc.GetLength());

  return *this;
}

const CFX_ByteString& CFX_ByteString::operator=(
    const CFX_ByteString& stringSrc) {
  if (m_pData != stringSrc.m_pData)
    m_pData = stringSrc.m_pData;

  return *this;
}

const CFX_ByteString& CFX_ByteString::operator+=(const FX_CHAR* pStr) {
  if (pStr)
    Concat(pStr, FXSYS_strlen(pStr));

  return *this;
}

const CFX_ByteString& CFX_ByteString::operator+=(char ch) {
  Concat(&ch, 1);
  return *this;
}

const CFX_ByteString& CFX_ByteString::operator+=(const CFX_ByteString& str) {
  if (str.m_pData)
    Concat(str.m_pData->m_String, str.m_pData->m_nDataLength);

  return *this;
}

const CFX_ByteString& CFX_ByteString::operator+=(const CFX_ByteStringC& str) {
  if (!str.IsEmpty())
    Concat(str.c_str(), str.GetLength());

  return *this;
}

bool CFX_ByteString::operator==(const char* ptr) const {
  if (!m_pData)
    return !ptr || !ptr[0];

  if (!ptr)
    return m_pData->m_nDataLength == 0;

  return FXSYS_strlen(ptr) == m_pData->m_nDataLength &&
         FXSYS_memcmp(ptr, m_pData->m_String, m_pData->m_nDataLength) == 0;
}

bool CFX_ByteString::operator==(const CFX_ByteStringC& str) const {
  if (!m_pData)
    return str.IsEmpty();

  return m_pData->m_nDataLength == str.GetLength() &&
         FXSYS_memcmp(m_pData->m_String, str.c_str(), str.GetLength()) == 0;
}

bool CFX_ByteString::operator==(const CFX_ByteString& other) const {
  if (m_pData == other.m_pData)
    return true;

  if (IsEmpty())
    return other.IsEmpty();

  if (other.IsEmpty())
    return false;

  return other.m_pData->m_nDataLength == m_pData->m_nDataLength &&
         FXSYS_memcmp(other.m_pData->m_String, m_pData->m_String,
                      m_pData->m_nDataLength) == 0;
}

bool CFX_ByteString::operator<(const CFX_ByteString& str) const {
  if (m_pData == str.m_pData)
    return false;

  int result = FXSYS_memcmp(c_str(), str.c_str(),
                            std::min(GetLength(), str.GetLength()));
  return result < 0 || (result == 0 && GetLength() < str.GetLength());
}

bool CFX_ByteString::EqualNoCase(const CFX_ByteStringC& str) const {
  if (!m_pData)
    return str.IsEmpty();

  FX_STRSIZE len = str.GetLength();
  if (m_pData->m_nDataLength != len)
    return false;

  const uint8_t* pThis = (const uint8_t*)m_pData->m_String;
  const uint8_t* pThat = str.raw_str();
  for (FX_STRSIZE i = 0; i < len; i++) {
    if ((*pThis) != (*pThat)) {
      uint8_t bThis = *pThis;
      if (bThis >= 'A' && bThis <= 'Z')
        bThis += 'a' - 'A';

      uint8_t bThat = *pThat;
      if (bThat >= 'A' && bThat <= 'Z')
        bThat += 'a' - 'A';

      if (bThis != bThat)
        return false;
    }
    pThis++;
    pThat++;
  }
  return true;
}

void CFX_ByteString::AssignCopy(const FX_CHAR* pSrcData, FX_STRSIZE nSrcLen) {
  AllocBeforeWrite(nSrcLen);
  m_pData->CopyContents(pSrcData, nSrcLen);
  m_pData->m_nDataLength = nSrcLen;
}

void CFX_ByteString::ReallocBeforeWrite(FX_STRSIZE nNewLength) {
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

void CFX_ByteString::AllocBeforeWrite(FX_STRSIZE nNewLength) {
  if (m_pData && m_pData->CanOperateInPlace(nNewLength))
    return;

  if (nNewLength <= 0) {
    clear();
    return;
  }

  m_pData.Reset(StringData::Create(nNewLength));
}

void CFX_ByteString::ReleaseBuffer(FX_STRSIZE nNewLength) {
  if (!m_pData)
    return;

  if (nNewLength == -1)
    nNewLength = FXSYS_strlen(m_pData->m_String);

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
    CFX_ByteString preserve(*this);
    ReallocBeforeWrite(nNewLength);
  }
}

void CFX_ByteString::Reserve(FX_STRSIZE len) {
  GetBuffer(len);
}

FX_CHAR* CFX_ByteString::GetBuffer(FX_STRSIZE nMinBufLength) {
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

FX_STRSIZE CFX_ByteString::Delete(FX_STRSIZE nIndex, FX_STRSIZE nCount) {
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
    FXSYS_memmove(m_pData->m_String + nIndex, m_pData->m_String + mLength,
                  nCharsToCopy);
    m_pData->m_nDataLength = nOldLength - nCount;
  }
  return m_pData->m_nDataLength;
}

void CFX_ByteString::Concat(const FX_CHAR* pSrcData, FX_STRSIZE nSrcLen) {
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

CFX_ByteString CFX_ByteString::Mid(FX_STRSIZE nFirst) const {
  if (!m_pData)
    return CFX_ByteString();

  return Mid(nFirst, m_pData->m_nDataLength - nFirst);
}

CFX_ByteString CFX_ByteString::Mid(FX_STRSIZE nFirst, FX_STRSIZE nCount) const {
  if (!m_pData)
    return CFX_ByteString();

  nFirst = std::min(std::max(nFirst, 0), m_pData->m_nDataLength);
  nCount = std::min(std::max(nCount, 0), m_pData->m_nDataLength - nFirst);
  if (nCount == 0)
    return CFX_ByteString();

  if (nFirst == 0 && nCount == m_pData->m_nDataLength)
    return *this;

  CFX_ByteString dest;
  AllocCopy(dest, nCount, nFirst);
  return dest;
}

void CFX_ByteString::AllocCopy(CFX_ByteString& dest,
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

CFX_ByteString CFX_ByteString::FormatInteger(int i, uint32_t flags) {
  char buf[32];
  return CFX_ByteString(buf, Buffer_itoa(buf, i, flags));
}

void CFX_ByteString::FormatV(const FX_CHAR* pFormat, va_list argList) {
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
  for (const FX_CHAR* pStr = pFormat; *pStr != 0; pStr++) {
    if (*pStr != '%' || *(pStr = pStr + 1) == '%') {
      nMaxLen += FXSYS_strlen(pStr);
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
      nWidth = FXSYS_atoi(pStr);
      while (std::isdigit(*pStr))
        pStr++;
    }
    if (nWidth < 0 || nWidth > 128 * 1024) {
      pFormat = "Bad width";
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
        nPrecision = FXSYS_atoi(pStr);
        while (std::isdigit(*pStr))
          pStr++;
      }
    }
    if (nPrecision < 0 || nPrecision > 128 * 1024) {
      pFormat = "Bad precision";
      nMaxLen = 14;
      break;
    }
    int nModifier = 0;
    if (FXSYS_strncmp(pStr, "I64", 3) == 0) {
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
      case 'S': {
        FX_WCHAR* pstrNextArg = va_arg(argList, FX_WCHAR*);
        if (pstrNextArg) {
          nItemLen = FXSYS_wcslen(pstrNextArg);
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
        FX_WCHAR* pstrNextArg = va_arg(argList, FX_WCHAR*);
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
            char pszTemp[256];
            double f = va_arg(argList, double);
            memset(pszTemp, 0, sizeof(pszTemp));
            FXSYS_snprintf(pszTemp, sizeof(pszTemp) - 1, "%*.*f", nWidth,
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
  nMaxLen += 32;  // Fudge factor.
  GetBuffer(nMaxLen);
  if (m_pData) {
    memset(m_pData->m_String, 0, nMaxLen);
    FXSYS_vsnprintf(m_pData->m_String, nMaxLen - 1, pFormat, argListSave);
    ReleaseBuffer();
  }
  va_end(argListSave);
}

void CFX_ByteString::Format(const FX_CHAR* pFormat, ...) {
  va_list argList;
  va_start(argList, pFormat);
  FormatV(pFormat, argList);
  va_end(argList);
}

FX_STRSIZE CFX_ByteString::Insert(FX_STRSIZE nIndex, FX_CHAR ch) {
  FX_STRSIZE nNewLength = m_pData ? m_pData->m_nDataLength : 0;
  nIndex = std::max(nIndex, 0);
  nIndex = std::min(nIndex, nNewLength);
  nNewLength++;

  ReallocBeforeWrite(nNewLength);
  FXSYS_memmove(m_pData->m_String + nIndex + 1, m_pData->m_String + nIndex,
                nNewLength - nIndex);
  m_pData->m_String[nIndex] = ch;
  m_pData->m_nDataLength = nNewLength;
  return nNewLength;
}

CFX_ByteString CFX_ByteString::Right(FX_STRSIZE nCount) const {
  if (!m_pData)
    return CFX_ByteString();

  nCount = std::max(nCount, 0);
  if (nCount >= m_pData->m_nDataLength)
    return *this;

  CFX_ByteString dest;
  AllocCopy(dest, nCount, m_pData->m_nDataLength - nCount);
  return dest;
}

CFX_ByteString CFX_ByteString::Left(FX_STRSIZE nCount) const {
  if (!m_pData)
    return CFX_ByteString();

  nCount = std::max(nCount, 0);
  if (nCount >= m_pData->m_nDataLength)
    return *this;

  CFX_ByteString dest;
  AllocCopy(dest, nCount, 0);
  return dest;
}

FX_STRSIZE CFX_ByteString::Find(FX_CHAR ch, FX_STRSIZE nStart) const {
  if (!m_pData)
    return -1;

  if (nStart < 0 || nStart >= m_pData->m_nDataLength)
    return -1;

  const FX_CHAR* pStr = static_cast<const FX_CHAR*>(
      memchr(m_pData->m_String + nStart, ch, m_pData->m_nDataLength - nStart));
  return pStr ? pStr - m_pData->m_String : -1;
}

FX_STRSIZE CFX_ByteString::ReverseFind(FX_CHAR ch) const {
  if (!m_pData)
    return -1;

  FX_STRSIZE nLength = m_pData->m_nDataLength;
  while (nLength--) {
    if (m_pData->m_String[nLength] == ch)
      return nLength;
  }
  return -1;
}

FX_STRSIZE CFX_ByteString::Find(const CFX_ByteStringC& pSub,
                                FX_STRSIZE nStart) const {
  if (!m_pData)
    return -1;

  FX_STRSIZE nLength = m_pData->m_nDataLength;
  if (nStart > nLength)
    return -1;

  const FX_CHAR* pStr =
      FX_strstr(m_pData->m_String + nStart, m_pData->m_nDataLength - nStart,
                pSub.c_str(), pSub.GetLength());
  return pStr ? (int)(pStr - m_pData->m_String) : -1;
}

void CFX_ByteString::MakeLower() {
  if (!m_pData)
    return;

  ReallocBeforeWrite(m_pData->m_nDataLength);
  FXSYS_strlwr(m_pData->m_String);
}

void CFX_ByteString::MakeUpper() {
  if (!m_pData)
    return;

  ReallocBeforeWrite(m_pData->m_nDataLength);
  FXSYS_strupr(m_pData->m_String);
}

FX_STRSIZE CFX_ByteString::Remove(FX_CHAR chRemove) {
  if (!m_pData || m_pData->m_nDataLength < 1)
    return 0;

  FX_CHAR* pstrSource = m_pData->m_String;
  FX_CHAR* pstrEnd = m_pData->m_String + m_pData->m_nDataLength;
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

  FX_CHAR* pstrDest = pstrSource;
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

FX_STRSIZE CFX_ByteString::Replace(const CFX_ByteStringC& pOld,
                                   const CFX_ByteStringC& pNew) {
  if (!m_pData || pOld.IsEmpty())
    return 0;

  FX_STRSIZE nSourceLen = pOld.GetLength();
  FX_STRSIZE nReplacementLen = pNew.GetLength();
  FX_STRSIZE nCount = 0;
  const FX_CHAR* pStart = m_pData->m_String;
  FX_CHAR* pEnd = m_pData->m_String + m_pData->m_nDataLength;
  while (1) {
    const FX_CHAR* pTarget = FX_strstr(pStart, (FX_STRSIZE)(pEnd - pStart),
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
  FX_CHAR* pDest = pNewData->m_String;
  for (FX_STRSIZE i = 0; i < nCount; i++) {
    const FX_CHAR* pTarget = FX_strstr(pStart, (FX_STRSIZE)(pEnd - pStart),
                                       pOld.c_str(), nSourceLen);
    FXSYS_memcpy(pDest, pStart, pTarget - pStart);
    pDest += pTarget - pStart;
    FXSYS_memcpy(pDest, pNew.c_str(), pNew.GetLength());
    pDest += pNew.GetLength();
    pStart = pTarget + nSourceLen;
  }
  FXSYS_memcpy(pDest, pStart, pEnd - pStart);
  m_pData.Swap(pNewData);
  return nCount;
}

void CFX_ByteString::SetAt(FX_STRSIZE nIndex, FX_CHAR ch) {
  if (!m_pData) {
    return;
  }
  ASSERT(nIndex >= 0);
  ASSERT(nIndex < m_pData->m_nDataLength);
  ReallocBeforeWrite(m_pData->m_nDataLength);
  m_pData->m_String[nIndex] = ch;
}

CFX_WideString CFX_ByteString::UTF8Decode() const {
  CFX_UTF8Decoder decoder;
  for (FX_STRSIZE i = 0; i < GetLength(); i++) {
    decoder.Input((uint8_t)m_pData->m_String[i]);
  }
  return CFX_WideString(decoder.GetResult());
}

// static
CFX_ByteString CFX_ByteString::FromUnicode(const FX_WCHAR* str,
                                           FX_STRSIZE len) {
  FX_STRSIZE str_len = len >= 0 ? len : FXSYS_wcslen(str);
  return FromUnicode(CFX_WideString(str, str_len));
}

// static
CFX_ByteString CFX_ByteString::FromUnicode(const CFX_WideString& str) {
  return CFX_CharMap::GetByteString(0, str.AsStringC());
}

int CFX_ByteString::Compare(const CFX_ByteStringC& str) const {
  if (!m_pData) {
    return str.IsEmpty() ? 0 : -1;
  }
  int this_len = m_pData->m_nDataLength;
  int that_len = str.GetLength();
  int min_len = this_len < that_len ? this_len : that_len;
  for (int i = 0; i < min_len; i++) {
    if ((uint8_t)m_pData->m_String[i] < str.GetAt(i)) {
      return -1;
    }
    if ((uint8_t)m_pData->m_String[i] > str.GetAt(i)) {
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

void CFX_ByteString::TrimRight(const CFX_ByteStringC& pTargets) {
  if (!m_pData || pTargets.IsEmpty()) {
    return;
  }
  FX_STRSIZE pos = GetLength();
  if (pos < 1) {
    return;
  }
  while (pos) {
    FX_STRSIZE i = 0;
    while (i < pTargets.GetLength() &&
           pTargets[i] != m_pData->m_String[pos - 1]) {
      i++;
    }
    if (i == pTargets.GetLength()) {
      break;
    }
    pos--;
  }
  if (pos < m_pData->m_nDataLength) {
    ReallocBeforeWrite(m_pData->m_nDataLength);
    m_pData->m_String[pos] = 0;
    m_pData->m_nDataLength = pos;
  }
}

void CFX_ByteString::TrimRight(FX_CHAR chTarget) {
  TrimRight(CFX_ByteStringC(chTarget));
}

void CFX_ByteString::TrimRight() {
  TrimRight("\x09\x0a\x0b\x0c\x0d\x20");
}

void CFX_ByteString::TrimLeft(const CFX_ByteStringC& pTargets) {
  if (!m_pData || pTargets.IsEmpty())
    return;

  FX_STRSIZE len = GetLength();
  if (len < 1)
    return;

  FX_STRSIZE pos = 0;
  while (pos < len) {
    FX_STRSIZE i = 0;
    while (i < pTargets.GetLength() && pTargets[i] != m_pData->m_String[pos]) {
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
                  (nDataLength + 1) * sizeof(FX_CHAR));
    m_pData->m_nDataLength = nDataLength;
  }
}

void CFX_ByteString::TrimLeft(FX_CHAR chTarget) {
  TrimLeft(CFX_ByteStringC(chTarget));
}

void CFX_ByteString::TrimLeft() {
  TrimLeft("\x09\x0a\x0b\x0c\x0d\x20");
}

uint32_t CFX_ByteString::GetID(FX_STRSIZE start_pos) const {
  return AsStringC().GetID(start_pos);
}
FX_STRSIZE FX_ftoa(FX_FLOAT d, FX_CHAR* buf) {
  buf[0] = '0';
  buf[1] = '\0';
  if (d == 0.0f) {
    return 1;
  }
  bool bNegative = false;
  if (d < 0) {
    bNegative = true;
    d = -d;
  }
  int scale = 1;
  int scaled = FXSYS_round(d);
  while (scaled < 100000) {
    if (scale == 1000000) {
      break;
    }
    scale *= 10;
    scaled = FXSYS_round(d * scale);
  }
  if (scaled == 0) {
    return 1;
  }
  char buf2[32];
  int buf_size = 0;
  if (bNegative) {
    buf[buf_size++] = '-';
  }
  int i = scaled / scale;
  FXSYS_itoa(i, buf2, 10);
  FX_STRSIZE len = FXSYS_strlen(buf2);
  FXSYS_memcpy(buf + buf_size, buf2, len);
  buf_size += len;
  int fraction = scaled % scale;
  if (fraction == 0) {
    return buf_size;
  }
  buf[buf_size++] = '.';
  scale /= 10;
  while (fraction) {
    buf[buf_size++] = '0' + fraction / scale;
    fraction %= scale;
    scale /= 10;
  }
  return buf_size;
}
CFX_ByteString CFX_ByteString::FormatFloat(FX_FLOAT d, int precision) {
  FX_CHAR buf[32];
  FX_STRSIZE len = FX_ftoa(d, buf);
  return CFX_ByteString(buf, len);
}
