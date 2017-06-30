// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_EXTENSION_H_
#define CORE_FXCRT_EXTENSION_H_

#include <algorithm>
#include <memory>

#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_safe_types.h"

class IFXCRT_FileAccess {
 public:
  static IFXCRT_FileAccess* Create();
  virtual ~IFXCRT_FileAccess() {}

  virtual bool Open(const CFX_ByteStringC& fileName, uint32_t dwMode) = 0;
  virtual bool Open(const CFX_WideStringC& fileName, uint32_t dwMode) = 0;
  virtual void Close() = 0;
  virtual FX_FILESIZE GetSize() const = 0;
  virtual FX_FILESIZE GetPosition() const = 0;
  virtual FX_FILESIZE SetPosition(FX_FILESIZE pos) = 0;
  virtual size_t Read(void* pBuffer, size_t szBuffer) = 0;
  virtual size_t Write(const void* pBuffer, size_t szBuffer) = 0;
  virtual size_t ReadPos(void* pBuffer, size_t szBuffer, FX_FILESIZE pos) = 0;
  virtual size_t WritePos(const void* pBuffer,
                          size_t szBuffer,
                          FX_FILESIZE pos) = 0;
  virtual bool Flush() = 0;
  virtual bool Truncate(FX_FILESIZE szFile) = 0;
};

#ifdef __cplusplus
extern "C" {
#endif
#define MT_N 848
#define MT_M 456
#define MT_Matrix_A 0x9908b0df
#define MT_Upper_Mask 0x80000000
#define MT_Lower_Mask 0x7fffffff
struct FX_MTRANDOMCONTEXT {
  FX_MTRANDOMCONTEXT() {
    mti = MT_N + 1;
    bHaveSeed = false;
  }
  uint32_t mti;
  bool bHaveSeed;
  uint32_t mt[MT_N];
};
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
bool FX_GenerateCryptoRandom(uint32_t* pBuffer, int32_t iCount);
#endif
#ifdef __cplusplus
}
#endif

#endif  // CORE_FXCRT_EXTENSION_H_
