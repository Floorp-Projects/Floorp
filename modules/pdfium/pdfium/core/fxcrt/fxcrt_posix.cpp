// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/fxcrt_posix.h"

#include "core/fxcrt/fx_basic.h"

#if _FXM_PLATFORM_ == _FXM_PLATFORM_LINUX_ || \
    _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_ || \
    _FXM_PLATFORM_ == _FXM_PLATFORM_ANDROID_

// static
IFXCRT_FileAccess* IFXCRT_FileAccess::Create() {
  return new CFXCRT_FileAccess_Posix;
}

void FXCRT_Posix_GetFileMode(uint32_t dwModes,
                             int32_t& nFlags,
                             int32_t& nMasks) {
  nFlags = O_BINARY | O_LARGEFILE;
  if (dwModes & FX_FILEMODE_ReadOnly) {
    nFlags |= O_RDONLY;
    nMasks = 0;
  } else {
    nFlags |= O_RDWR | O_CREAT;
    if (dwModes & FX_FILEMODE_Truncate) {
      nFlags |= O_TRUNC;
    }
    nMasks = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  }
}
CFXCRT_FileAccess_Posix::CFXCRT_FileAccess_Posix() : m_nFD(-1) {}
CFXCRT_FileAccess_Posix::~CFXCRT_FileAccess_Posix() {
  Close();
}

bool CFXCRT_FileAccess_Posix::Open(const CFX_ByteStringC& fileName,
                                   uint32_t dwMode) {
  if (m_nFD > -1)
    return false;

  int32_t nFlags;
  int32_t nMasks;
  FXCRT_Posix_GetFileMode(dwMode, nFlags, nMasks);
  m_nFD = open(fileName.c_str(), nFlags, nMasks);
  return m_nFD > -1;
}

bool CFXCRT_FileAccess_Posix::Open(const CFX_WideStringC& fileName,
                                   uint32_t dwMode) {
  return Open(FX_UTF8Encode(fileName).AsStringC(), dwMode);
}

void CFXCRT_FileAccess_Posix::Close() {
  if (m_nFD < 0) {
    return;
  }
  close(m_nFD);
  m_nFD = -1;
}
FX_FILESIZE CFXCRT_FileAccess_Posix::GetSize() const {
  if (m_nFD < 0) {
    return 0;
  }
  struct stat s;
  FXSYS_memset(&s, 0, sizeof(s));
  fstat(m_nFD, &s);
  return s.st_size;
}
FX_FILESIZE CFXCRT_FileAccess_Posix::GetPosition() const {
  if (m_nFD < 0) {
    return (FX_FILESIZE)-1;
  }
  return lseek(m_nFD, 0, SEEK_CUR);
}
FX_FILESIZE CFXCRT_FileAccess_Posix::SetPosition(FX_FILESIZE pos) {
  if (m_nFD < 0) {
    return (FX_FILESIZE)-1;
  }
  return lseek(m_nFD, pos, SEEK_SET);
}
size_t CFXCRT_FileAccess_Posix::Read(void* pBuffer, size_t szBuffer) {
  if (m_nFD < 0) {
    return 0;
  }
  return read(m_nFD, pBuffer, szBuffer);
}
size_t CFXCRT_FileAccess_Posix::Write(const void* pBuffer, size_t szBuffer) {
  if (m_nFD < 0) {
    return 0;
  }
  return write(m_nFD, pBuffer, szBuffer);
}
size_t CFXCRT_FileAccess_Posix::ReadPos(void* pBuffer,
                                        size_t szBuffer,
                                        FX_FILESIZE pos) {
  if (m_nFD < 0) {
    return 0;
  }
  if (pos >= GetSize()) {
    return 0;
  }
  if (SetPosition(pos) == (FX_FILESIZE)-1) {
    return 0;
  }
  return Read(pBuffer, szBuffer);
}
size_t CFXCRT_FileAccess_Posix::WritePos(const void* pBuffer,
                                         size_t szBuffer,
                                         FX_FILESIZE pos) {
  if (m_nFD < 0) {
    return 0;
  }
  if (SetPosition(pos) == (FX_FILESIZE)-1) {
    return 0;
  }
  return Write(pBuffer, szBuffer);
}

bool CFXCRT_FileAccess_Posix::Flush() {
  if (m_nFD < 0)
    return false;

  return fsync(m_nFD) > -1;
}

bool CFXCRT_FileAccess_Posix::Truncate(FX_FILESIZE szFile) {
  if (m_nFD < 0)
    return false;

  return !ftruncate(m_nFD, szFile);
}

#endif
