// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/extension.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_ext.h"

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
#include <wincrypt.h>
#else
#include <ctime>
#endif

namespace {

#ifdef PDF_ENABLE_XFA

class CFX_CRTFileAccess : public IFX_FileAccess {
 public:
  template <typename T, typename... Args>
  friend CFX_RetainPtr<T> pdfium::MakeRetain(Args&&... args);

  // IFX_FileAccess
  void GetPath(CFX_WideString& wsPath) override;
  CFX_RetainPtr<IFX_SeekableStream> CreateFileStream(uint32_t dwModes) override;

  bool Init(const CFX_WideStringC& wsPath);

 private:
  CFX_CRTFileAccess();
  ~CFX_CRTFileAccess() override;

  CFX_WideString m_path;
};

CFX_CRTFileAccess::CFX_CRTFileAccess() {}

CFX_CRTFileAccess::~CFX_CRTFileAccess() {}

void CFX_CRTFileAccess::GetPath(CFX_WideString& wsPath) {
  wsPath = m_path;
}

CFX_RetainPtr<IFX_SeekableStream> CFX_CRTFileAccess::CreateFileStream(
    uint32_t dwModes) {
  return IFX_SeekableStream::CreateFromFilename(m_path.c_str(), dwModes);
}

bool CFX_CRTFileAccess::Init(const CFX_WideStringC& wsPath) {
  m_path = wsPath;
  return true;
}

#endif  // PDF_ENABLE_XFA

class CFX_CRTFileStream final : public IFX_SeekableStream {
 public:
  template <typename T, typename... Args>
  friend CFX_RetainPtr<T> pdfium::MakeRetain(Args&&... args);

  // IFX_SeekableStream:
  FX_FILESIZE GetSize() override;
  bool IsEOF() override;
  FX_FILESIZE GetPosition() override;
  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override;
  size_t ReadBlock(void* buffer, size_t size) override;
  bool WriteBlock(const void* buffer, FX_FILESIZE offset, size_t size) override;
  bool Flush() override;

 private:
  explicit CFX_CRTFileStream(std::unique_ptr<IFXCRT_FileAccess> pFA);
  ~CFX_CRTFileStream() override;

  std::unique_ptr<IFXCRT_FileAccess> m_pFile;
};

CFX_CRTFileStream::CFX_CRTFileStream(std::unique_ptr<IFXCRT_FileAccess> pFA)
    : m_pFile(std::move(pFA)) {}

CFX_CRTFileStream::~CFX_CRTFileStream() {}

FX_FILESIZE CFX_CRTFileStream::GetSize() {
  return m_pFile->GetSize();
}

bool CFX_CRTFileStream::IsEOF() {
  return GetPosition() >= GetSize();
}

FX_FILESIZE CFX_CRTFileStream::GetPosition() {
  return m_pFile->GetPosition();
}

bool CFX_CRTFileStream::ReadBlock(void* buffer,
                                  FX_FILESIZE offset,
                                  size_t size) {
  return m_pFile->ReadPos(buffer, size, offset) > 0;
}

size_t CFX_CRTFileStream::ReadBlock(void* buffer, size_t size) {
  return m_pFile->Read(buffer, size);
}

bool CFX_CRTFileStream::WriteBlock(const void* buffer,
                                   FX_FILESIZE offset,
                                   size_t size) {
  return !!m_pFile->WritePos(buffer, size, offset);
}

bool CFX_CRTFileStream::Flush() {
  return m_pFile->Flush();
}

#define FX_MEMSTREAM_BlockSize (64 * 1024)
#define FX_MEMSTREAM_Consecutive 0x01
#define FX_MEMSTREAM_TakeOver 0x02

class CFX_MemoryStream final : public IFX_MemoryStream {
 public:
  template <typename T, typename... Args>
  friend CFX_RetainPtr<T> pdfium::MakeRetain(Args&&... args);

  // IFX_MemoryStream
  FX_FILESIZE GetSize() override;
  bool IsEOF() override;
  FX_FILESIZE GetPosition() override;
  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override;
  size_t ReadBlock(void* buffer, size_t size) override;
  bool WriteBlock(const void* buffer, FX_FILESIZE offset, size_t size) override;
  bool Flush() override;
  bool IsConsecutive() const override;
  void EstimateSize(size_t nInitSize, size_t nGrowSize) override;
  uint8_t* GetBuffer() const override;
  void AttachBuffer(uint8_t* pBuffer,
                    size_t nSize,
                    bool bTakeOver = false) override;
  void DetachBuffer() override;

 private:
  explicit CFX_MemoryStream(bool bConsecutive);
  CFX_MemoryStream(uint8_t* pBuffer, size_t nSize, bool bTakeOver);
  ~CFX_MemoryStream() override;

  bool ExpandBlocks(size_t size);

  CFX_ArrayTemplate<uint8_t*> m_Blocks;
  size_t m_nTotalSize;
  size_t m_nCurSize;
  size_t m_nCurPos;
  size_t m_nGrowSize;
  uint32_t m_dwFlags;
};

CFX_MemoryStream::CFX_MemoryStream(bool bConsecutive)
    : m_nTotalSize(0),
      m_nCurSize(0),
      m_nCurPos(0),
      m_nGrowSize(FX_MEMSTREAM_BlockSize) {
  m_dwFlags =
      FX_MEMSTREAM_TakeOver | (bConsecutive ? FX_MEMSTREAM_Consecutive : 0);
}

CFX_MemoryStream::CFX_MemoryStream(uint8_t* pBuffer,
                                   size_t nSize,
                                   bool bTakeOver)
    : m_nTotalSize(nSize),
      m_nCurSize(nSize),
      m_nCurPos(0),
      m_nGrowSize(FX_MEMSTREAM_BlockSize) {
  m_Blocks.Add(pBuffer);
  m_dwFlags =
      FX_MEMSTREAM_Consecutive | (bTakeOver ? FX_MEMSTREAM_TakeOver : 0);
}

CFX_MemoryStream::~CFX_MemoryStream() {
  if (m_dwFlags & FX_MEMSTREAM_TakeOver) {
    for (int32_t i = 0; i < m_Blocks.GetSize(); i++) {
      FX_Free(m_Blocks[i]);
    }
  }
  m_Blocks.RemoveAll();
}

FX_FILESIZE CFX_MemoryStream::GetSize() {
  return (FX_FILESIZE)m_nCurSize;
}

bool CFX_MemoryStream::IsEOF() {
  return m_nCurPos >= (size_t)GetSize();
}

FX_FILESIZE CFX_MemoryStream::GetPosition() {
  return (FX_FILESIZE)m_nCurPos;
}

bool CFX_MemoryStream::ReadBlock(void* buffer,
                                 FX_FILESIZE offset,
                                 size_t size) {
  if (!buffer || !size || offset < 0)
    return false;

  FX_SAFE_SIZE_T newPos = size;
  newPos += offset;
  if (!newPos.IsValid() || newPos.ValueOrDefault(0) == 0 ||
      newPos.ValueOrDie() > m_nCurSize) {
    return false;
  }

  m_nCurPos = newPos.ValueOrDie();
  if (m_dwFlags & FX_MEMSTREAM_Consecutive) {
    FXSYS_memcpy(buffer, m_Blocks[0] + (size_t)offset, size);
    return true;
  }
  size_t nStartBlock = (size_t)offset / m_nGrowSize;
  offset -= (FX_FILESIZE)(nStartBlock * m_nGrowSize);
  while (size) {
    size_t nRead = m_nGrowSize - (size_t)offset;
    if (nRead > size) {
      nRead = size;
    }
    FXSYS_memcpy(buffer, m_Blocks[(int)nStartBlock] + (size_t)offset, nRead);
    buffer = ((uint8_t*)buffer) + nRead;
    size -= nRead;
    nStartBlock++;
    offset = 0;
  }
  return true;
}

size_t CFX_MemoryStream::ReadBlock(void* buffer, size_t size) {
  if (m_nCurPos >= m_nCurSize) {
    return 0;
  }
  size_t nRead = std::min(size, m_nCurSize - m_nCurPos);
  if (!ReadBlock(buffer, (int32_t)m_nCurPos, nRead)) {
    return 0;
  }
  return nRead;
}

bool CFX_MemoryStream::WriteBlock(const void* buffer,
                                  FX_FILESIZE offset,
                                  size_t size) {
  if (!buffer || !size)
    return false;

  if (m_dwFlags & FX_MEMSTREAM_Consecutive) {
    FX_SAFE_SIZE_T newPos = size;
    newPos += offset;
    if (!newPos.IsValid())
      return false;

    m_nCurPos = newPos.ValueOrDie();
    if (m_nCurPos > m_nTotalSize) {
      m_nTotalSize = (m_nCurPos + m_nGrowSize - 1) / m_nGrowSize * m_nGrowSize;
      if (m_Blocks.GetSize() < 1) {
        uint8_t* block = FX_Alloc(uint8_t, m_nTotalSize);
        m_Blocks.Add(block);
      } else {
        m_Blocks[0] = FX_Realloc(uint8_t, m_Blocks[0], m_nTotalSize);
      }
      if (!m_Blocks[0]) {
        m_Blocks.RemoveAll();
        return false;
      }
    }
    FXSYS_memcpy(m_Blocks[0] + (size_t)offset, buffer, size);
    if (m_nCurSize < m_nCurPos) {
      m_nCurSize = m_nCurPos;
    }
    return true;
  }

  FX_SAFE_SIZE_T newPos = size;
  newPos += offset;
  if (!newPos.IsValid()) {
    return false;
  }

  if (!ExpandBlocks(newPos.ValueOrDie())) {
    return false;
  }
  m_nCurPos = newPos.ValueOrDie();
  size_t nStartBlock = (size_t)offset / m_nGrowSize;
  offset -= (FX_FILESIZE)(nStartBlock * m_nGrowSize);
  while (size) {
    size_t nWrite = m_nGrowSize - (size_t)offset;
    if (nWrite > size) {
      nWrite = size;
    }
    FXSYS_memcpy(m_Blocks[(int)nStartBlock] + (size_t)offset, buffer, nWrite);
    buffer = ((uint8_t*)buffer) + nWrite;
    size -= nWrite;
    nStartBlock++;
    offset = 0;
  }
  return true;
}

bool CFX_MemoryStream::Flush() {
  return true;
}

bool CFX_MemoryStream::IsConsecutive() const {
  return !!(m_dwFlags & FX_MEMSTREAM_Consecutive);
}

void CFX_MemoryStream::EstimateSize(size_t nInitSize, size_t nGrowSize) {
  if (m_dwFlags & FX_MEMSTREAM_Consecutive) {
    if (m_Blocks.GetSize() < 1) {
      uint8_t* pBlock =
          FX_Alloc(uint8_t, std::max(nInitSize, static_cast<size_t>(4096)));
      m_Blocks.Add(pBlock);
    }
    m_nGrowSize = std::max(nGrowSize, static_cast<size_t>(4096));
  } else if (m_Blocks.GetSize() < 1) {
    m_nGrowSize = std::max(nGrowSize, static_cast<size_t>(4096));
  }
}

uint8_t* CFX_MemoryStream::GetBuffer() const {
  return m_Blocks.GetSize() ? m_Blocks[0] : nullptr;
}

void CFX_MemoryStream::AttachBuffer(uint8_t* pBuffer,
                                    size_t nSize,
                                    bool bTakeOver) {
  if (!(m_dwFlags & FX_MEMSTREAM_Consecutive))
    return;

  m_Blocks.RemoveAll();
  m_Blocks.Add(pBuffer);
  m_nTotalSize = m_nCurSize = nSize;
  m_nCurPos = 0;
  m_dwFlags =
      FX_MEMSTREAM_Consecutive | (bTakeOver ? FX_MEMSTREAM_TakeOver : 0);
}

void CFX_MemoryStream::DetachBuffer() {
  if (!(m_dwFlags & FX_MEMSTREAM_Consecutive)) {
    return;
  }
  m_Blocks.RemoveAll();
  m_nTotalSize = m_nCurSize = m_nCurPos = 0;
  m_dwFlags = FX_MEMSTREAM_TakeOver;
}

bool CFX_MemoryStream::ExpandBlocks(size_t size) {
  if (m_nCurSize < size) {
    m_nCurSize = size;
  }
  if (size <= m_nTotalSize) {
    return true;
  }
  int32_t iCount = m_Blocks.GetSize();
  size = (size - m_nTotalSize + m_nGrowSize - 1) / m_nGrowSize;
  m_Blocks.SetSize(m_Blocks.GetSize() + (int32_t)size);
  while (size--) {
    uint8_t* pBlock = FX_Alloc(uint8_t, m_nGrowSize);
    m_Blocks.SetAt(iCount++, pBlock);
    m_nTotalSize += m_nGrowSize;
  }
  return true;
}

}  // namespace

#ifdef PDF_ENABLE_XFA
CFX_RetainPtr<IFX_FileAccess> IFX_FileAccess::CreateDefault(
    const CFX_WideStringC& wsPath) {
  if (wsPath.GetLength() == 0)
    return nullptr;

  auto pFA = pdfium::MakeRetain<CFX_CRTFileAccess>();
  pFA->Init(wsPath);
  return pFA;
}
#endif  // PDF_ENABLE_XFA

// static
CFX_RetainPtr<IFX_SeekableStream> IFX_SeekableStream::CreateFromFilename(
    const FX_CHAR* filename,
    uint32_t dwModes) {
  std::unique_ptr<IFXCRT_FileAccess> pFA(IFXCRT_FileAccess::Create());
  if (!pFA->Open(filename, dwModes))
    return nullptr;
  return pdfium::MakeRetain<CFX_CRTFileStream>(std::move(pFA));
}

// static
CFX_RetainPtr<IFX_SeekableStream> IFX_SeekableStream::CreateFromFilename(
    const FX_WCHAR* filename,
    uint32_t dwModes) {
  std::unique_ptr<IFXCRT_FileAccess> pFA(IFXCRT_FileAccess::Create());
  if (!pFA->Open(filename, dwModes))
    return nullptr;
  return pdfium::MakeRetain<CFX_CRTFileStream>(std::move(pFA));
}

// static
CFX_RetainPtr<IFX_SeekableReadStream>
IFX_SeekableReadStream::CreateFromFilename(const FX_CHAR* filename) {
  return IFX_SeekableStream::CreateFromFilename(filename, FX_FILEMODE_ReadOnly);
}

// static
CFX_RetainPtr<IFX_MemoryStream> IFX_MemoryStream::Create(uint8_t* pBuffer,
                                                         size_t dwSize,
                                                         bool bTakeOver) {
  return pdfium::MakeRetain<CFX_MemoryStream>(pBuffer, dwSize, bTakeOver);
}

// static
CFX_RetainPtr<IFX_MemoryStream> IFX_MemoryStream::Create(bool bConsecutive) {
  return pdfium::MakeRetain<CFX_MemoryStream>(bConsecutive);
}

FX_FLOAT FXSYS_tan(FX_FLOAT a) {
  return (FX_FLOAT)tan(a);
}
FX_FLOAT FXSYS_logb(FX_FLOAT b, FX_FLOAT x) {
  return FXSYS_log(x) / FXSYS_log(b);
}
FX_FLOAT FXSYS_strtof(const FX_CHAR* pcsStr,
                      int32_t iLength,
                      int32_t* pUsedLen) {
  ASSERT(pcsStr);
  if (iLength < 0) {
    iLength = (int32_t)FXSYS_strlen(pcsStr);
  }
  CFX_WideString ws =
      CFX_WideString::FromLocal(CFX_ByteStringC(pcsStr, iLength));
  return FXSYS_wcstof(ws.c_str(), iLength, pUsedLen);
}
FX_FLOAT FXSYS_wcstof(const FX_WCHAR* pwsStr,
                      int32_t iLength,
                      int32_t* pUsedLen) {
  ASSERT(pwsStr);
  if (iLength < 0) {
    iLength = (int32_t)FXSYS_wcslen(pwsStr);
  }
  if (iLength == 0) {
    return 0.0f;
  }
  int32_t iUsedLen = 0;
  bool bNegtive = false;
  switch (pwsStr[iUsedLen]) {
    case '-':
      bNegtive = true;
    case '+':
      iUsedLen++;
      break;
  }
  FX_FLOAT fValue = 0.0f;
  while (iUsedLen < iLength) {
    FX_WCHAR wch = pwsStr[iUsedLen];
    if (wch >= L'0' && wch <= L'9') {
      fValue = fValue * 10.0f + (wch - L'0');
    } else {
      break;
    }
    iUsedLen++;
  }
  if (iUsedLen < iLength && pwsStr[iUsedLen] == L'.') {
    FX_FLOAT fPrecise = 0.1f;
    while (++iUsedLen < iLength) {
      FX_WCHAR wch = pwsStr[iUsedLen];
      if (wch >= L'0' && wch <= L'9') {
        fValue += (wch - L'0') * fPrecise;
        fPrecise *= 0.1f;
      } else {
        break;
      }
    }
  }
  if (pUsedLen) {
    *pUsedLen = iUsedLen;
  }
  return bNegtive ? -fValue : fValue;
}
FX_WCHAR* FXSYS_wcsncpy(FX_WCHAR* dstStr,
                        const FX_WCHAR* srcStr,
                        size_t count) {
  ASSERT(dstStr && srcStr && count > 0);
  for (size_t i = 0; i < count; ++i)
    if ((dstStr[i] = srcStr[i]) == L'\0') {
      break;
    }
  return dstStr;
}
int32_t FXSYS_wcsnicmp(const FX_WCHAR* s1, const FX_WCHAR* s2, size_t count) {
  ASSERT(s1 && s2 && count > 0);
  FX_WCHAR wch1 = 0, wch2 = 0;
  while (count-- > 0) {
    wch1 = (FX_WCHAR)FXSYS_tolower(*s1++);
    wch2 = (FX_WCHAR)FXSYS_tolower(*s2++);
    if (wch1 != wch2) {
      break;
    }
  }
  return wch1 - wch2;
}
int32_t FXSYS_strnicmp(const FX_CHAR* s1, const FX_CHAR* s2, size_t count) {
  ASSERT(s1 && s2 && count > 0);
  FX_CHAR ch1 = 0, ch2 = 0;
  while (count-- > 0) {
    ch1 = (FX_CHAR)FXSYS_tolower(*s1++);
    ch2 = (FX_CHAR)FXSYS_tolower(*s2++);
    if (ch1 != ch2) {
      break;
    }
  }
  return ch1 - ch2;
}

uint32_t FX_HashCode_GetA(const CFX_ByteStringC& str, bool bIgnoreCase) {
  uint32_t dwHashCode = 0;
  if (bIgnoreCase) {
    for (FX_STRSIZE i = 0; i < str.GetLength(); ++i)
      dwHashCode = 31 * dwHashCode + FXSYS_tolower(str.CharAt(i));
  } else {
    for (FX_STRSIZE i = 0; i < str.GetLength(); ++i)
      dwHashCode = 31 * dwHashCode + str.CharAt(i);
  }
  return dwHashCode;
}

uint32_t FX_HashCode_GetW(const CFX_WideStringC& str, bool bIgnoreCase) {
  uint32_t dwHashCode = 0;
  if (bIgnoreCase) {
    for (FX_STRSIZE i = 0; i < str.GetLength(); ++i)
      dwHashCode = 1313 * dwHashCode + FXSYS_tolower(str.CharAt(i));
  } else {
    for (FX_STRSIZE i = 0; i < str.GetLength(); ++i)
      dwHashCode = 1313 * dwHashCode + str.CharAt(i);
  }
  return dwHashCode;
}

void* FX_Random_MT_Start(uint32_t dwSeed) {
  FX_MTRANDOMCONTEXT* pContext = FX_Alloc(FX_MTRANDOMCONTEXT, 1);
  pContext->mt[0] = dwSeed;
  uint32_t& i = pContext->mti;
  uint32_t* pBuf = pContext->mt;
  for (i = 1; i < MT_N; i++) {
    pBuf[i] = (1812433253UL * (pBuf[i - 1] ^ (pBuf[i - 1] >> 30)) + i);
  }
  pContext->bHaveSeed = true;
  return pContext;
}
uint32_t FX_Random_MT_Generate(void* pContext) {
  ASSERT(pContext);
  FX_MTRANDOMCONTEXT* pMTC = static_cast<FX_MTRANDOMCONTEXT*>(pContext);
  uint32_t v;
  static uint32_t mag[2] = {0, MT_Matrix_A};
  uint32_t& mti = pMTC->mti;
  uint32_t* pBuf = pMTC->mt;
  if ((int)mti < 0 || mti >= MT_N) {
    if (mti > MT_N && !pMTC->bHaveSeed) {
      return 0;
    }
    uint32_t kk;
    for (kk = 0; kk < MT_N - MT_M; kk++) {
      v = (pBuf[kk] & MT_Upper_Mask) | (pBuf[kk + 1] & MT_Lower_Mask);
      pBuf[kk] = pBuf[kk + MT_M] ^ (v >> 1) ^ mag[v & 1];
    }
    for (; kk < MT_N - 1; kk++) {
      v = (pBuf[kk] & MT_Upper_Mask) | (pBuf[kk + 1] & MT_Lower_Mask);
      pBuf[kk] = pBuf[kk + (MT_M - MT_N)] ^ (v >> 1) ^ mag[v & 1];
    }
    v = (pBuf[MT_N - 1] & MT_Upper_Mask) | (pBuf[0] & MT_Lower_Mask);
    pBuf[MT_N - 1] = pBuf[MT_M - 1] ^ (v >> 1) ^ mag[v & 1];
    mti = 0;
  }
  v = pBuf[mti++];
  v ^= (v >> 11);
  v ^= (v << 7) & 0x9d2c5680UL;
  v ^= (v << 15) & 0xefc60000UL;
  v ^= (v >> 18);
  return v;
}
void FX_Random_MT_Close(void* pContext) {
  ASSERT(pContext);
  FX_Free(pContext);
}
void FX_Random_GenerateMT(uint32_t* pBuffer, int32_t iCount) {
  uint32_t dwSeed;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  if (!FX_GenerateCryptoRandom(&dwSeed, 1)) {
    FX_Random_GenerateBase(&dwSeed, 1);
  }
#else
  FX_Random_GenerateBase(&dwSeed, 1);
#endif
  void* pContext = FX_Random_MT_Start(dwSeed);
  while (iCount-- > 0) {
    *pBuffer++ = FX_Random_MT_Generate(pContext);
  }
  FX_Random_MT_Close(pContext);
}
void FX_Random_GenerateBase(uint32_t* pBuffer, int32_t iCount) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  SYSTEMTIME st1, st2;
  ::GetSystemTime(&st1);
  do {
    ::GetSystemTime(&st2);
  } while (FXSYS_memcmp(&st1, &st2, sizeof(SYSTEMTIME)) == 0);
  uint32_t dwHash1 =
      FX_HashCode_GetA(CFX_ByteStringC((uint8_t*)&st1, sizeof(st1)), true);
  uint32_t dwHash2 =
      FX_HashCode_GetA(CFX_ByteStringC((uint8_t*)&st2, sizeof(st2)), true);
  ::srand((dwHash1 << 16) | (uint32_t)dwHash2);
#else
  time_t tmLast = time(nullptr);
  time_t tmCur;
  while ((tmCur = time(nullptr)) == tmLast) {
    continue;
  }

  ::srand((tmCur << 16) | (tmLast & 0xFFFF));
#endif
  while (iCount-- > 0) {
    *pBuffer++ = (uint32_t)((::rand() << 16) | (::rand() & 0xFFFF));
  }
}
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
bool FX_GenerateCryptoRandom(uint32_t* pBuffer, int32_t iCount) {
  HCRYPTPROV hCP = 0;
  if (!::CryptAcquireContext(&hCP, nullptr, nullptr, PROV_RSA_FULL, 0) ||
      !hCP) {
    return false;
  }
  ::CryptGenRandom(hCP, iCount * sizeof(uint32_t), (uint8_t*)pBuffer);
  ::CryptReleaseContext(hCP, 0);
  return true;
}
#endif
void FX_Random_GenerateCrypto(uint32_t* pBuffer, int32_t iCount) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  FX_GenerateCryptoRandom(pBuffer, iCount);
#else
  FX_Random_GenerateBase(pBuffer, iCount);
#endif
}

#ifdef PDF_ENABLE_XFA
static const FX_CHAR gs_FX_pHexChars[] = "0123456789ABCDEF";
void FX_GUID_CreateV4(FX_GUID* pGUID) {
  FX_Random_GenerateMT((uint32_t*)pGUID, 4);
  uint8_t& b = ((uint8_t*)pGUID)[6];
  b = (b & 0x0F) | 0x40;
}
void FX_GUID_ToString(const FX_GUID* pGUID,
                      CFX_ByteString& bsStr,
                      bool bSeparator) {
  FX_CHAR* pBuf = bsStr.GetBuffer(40);
  uint8_t b;
  for (int32_t i = 0; i < 16; i++) {
    b = ((const uint8_t*)pGUID)[i];
    *pBuf++ = gs_FX_pHexChars[b >> 4];
    *pBuf++ = gs_FX_pHexChars[b & 0x0F];
    if (bSeparator && (i == 3 || i == 5 || i == 7 || i == 9)) {
      *pBuf++ = L'-';
    }
  }
  bsStr.ReleaseBuffer(bSeparator ? 36 : 32);
}
#endif  // PDF_ENABLE_XFA
