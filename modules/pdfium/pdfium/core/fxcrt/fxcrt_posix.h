// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_FXCRT_POSIX_H_
#define CORE_FXCRT_FXCRT_POSIX_H_

#include "core/fxcrt/extension.h"

#if _FXM_PLATFORM_ == _FXM_PLATFORM_LINUX_ || \
    _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_ || \
    _FXM_PLATFORM_ == _FXM_PLATFORM_ANDROID_
class CFXCRT_FileAccess_Posix : public IFXCRT_FileAccess {
 public:
  CFXCRT_FileAccess_Posix();
  ~CFXCRT_FileAccess_Posix() override;

  // IFXCRT_FileAccess:
  bool Open(const CFX_ByteStringC& fileName, uint32_t dwMode) override;
  bool Open(const CFX_WideStringC& fileName, uint32_t dwMode) override;
  void Close() override;
  FX_FILESIZE GetSize() const override;
  FX_FILESIZE GetPosition() const override;
  FX_FILESIZE SetPosition(FX_FILESIZE pos) override;
  size_t Read(void* pBuffer, size_t szBuffer) override;
  size_t Write(const void* pBuffer, size_t szBuffer) override;
  size_t ReadPos(void* pBuffer, size_t szBuffer, FX_FILESIZE pos) override;
  size_t WritePos(const void* pBuffer,
                  size_t szBuffer,
                  FX_FILESIZE pos) override;
  bool Flush() override;
  bool Truncate(FX_FILESIZE szFile) override;

 protected:
  int32_t m_nFD;
};
#endif

#endif  // CORE_FXCRT_FXCRT_POSIX_H_
