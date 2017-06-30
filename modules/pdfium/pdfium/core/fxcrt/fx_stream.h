// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_FX_STREAM_H_
#define CORE_FXCRT_FX_STREAM_H_

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
#include <direct.h>

class CFindFileDataA;
typedef CFindFileDataA FX_FileHandle;
#define FX_FILESIZE int32_t

#else

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif  // O_BINARY

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif  // O_LARGEFILE

typedef DIR FX_FileHandle;
#define FX_FILESIZE off_t
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_

FX_FileHandle* FX_OpenFolder(const FX_CHAR* path);
bool FX_GetNextFile(FX_FileHandle* handle,
                    CFX_ByteString* filename,
                    bool* bFolder);
void FX_CloseFolder(FX_FileHandle* handle);
FX_WCHAR FX_GetFolderSeparator();

#define FX_FILEMODE_Write 0
#define FX_FILEMODE_ReadOnly 1
#define FX_FILEMODE_Truncate 2

class IFX_WriteStream : virtual public CFX_Retainable {
 public:
  virtual bool WriteBlock(const void* pData, size_t size) = 0;
};

class IFX_ReadStream : virtual public CFX_Retainable {
 public:
  virtual bool IsEOF() = 0;
  virtual FX_FILESIZE GetPosition() = 0;
  virtual size_t ReadBlock(void* buffer, size_t size) = 0;
};

class IFX_SeekableWriteStream : public IFX_WriteStream {
 public:
  // IFX_WriteStream:
  bool WriteBlock(const void* pData, size_t size) override;

  virtual FX_FILESIZE GetSize() = 0;
  virtual bool Flush() = 0;
  virtual bool WriteBlock(const void* pData,
                          FX_FILESIZE offset,
                          size_t size) = 0;
};

class IFX_SeekableReadStream : public IFX_ReadStream {
 public:
  static CFX_RetainPtr<IFX_SeekableReadStream> CreateFromFilename(
      const FX_CHAR* filename);

  // IFX_ReadStream:
  bool IsEOF() override;
  FX_FILESIZE GetPosition() override;
  size_t ReadBlock(void* buffer, size_t size) override;

  virtual bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) = 0;
  virtual FX_FILESIZE GetSize() = 0;
};

class IFX_SeekableStream : public IFX_SeekableReadStream,
                           public IFX_SeekableWriteStream {
 public:
  static CFX_RetainPtr<IFX_SeekableStream> CreateFromFilename(
      const FX_CHAR* filename,
      uint32_t dwModes);

  static CFX_RetainPtr<IFX_SeekableStream> CreateFromFilename(
      const FX_WCHAR* filename,
      uint32_t dwModes);

  // IFX_SeekableReadStream:
  bool IsEOF() override = 0;
  FX_FILESIZE GetPosition() override = 0;
  size_t ReadBlock(void* buffer, size_t size) override = 0;
  bool ReadBlock(void* buffer, FX_FILESIZE offset, size_t size) override = 0;
  FX_FILESIZE GetSize() override = 0;

  // IFX_SeekableWriteStream:
  bool WriteBlock(const void* buffer,
                  FX_FILESIZE offset,
                  size_t size) override = 0;
  bool WriteBlock(const void* buffer, size_t size) override;
  bool Flush() override = 0;
};

class IFX_MemoryStream : public IFX_SeekableStream {
 public:
  static CFX_RetainPtr<IFX_MemoryStream> Create(uint8_t* pBuffer,
                                                size_t nSize,
                                                bool bTakeOver = false);
  static CFX_RetainPtr<IFX_MemoryStream> Create(bool bConsecutive = false);

  virtual bool IsConsecutive() const = 0;
  virtual void EstimateSize(size_t nInitSize, size_t nGrowSize) = 0;
  virtual uint8_t* GetBuffer() const = 0;
  virtual void AttachBuffer(uint8_t* pBuffer,
                            size_t nSize,
                            bool bTakeOver = false) = 0;
  virtual void DetachBuffer() = 0;
};

class IFX_BufferedReadStream : public IFX_ReadStream {
 public:
  // IFX_ReadStream:
  bool IsEOF() override = 0;
  FX_FILESIZE GetPosition() override = 0;
  size_t ReadBlock(void* buffer, size_t size) override = 0;

  virtual bool ReadNextBlock(bool bRestart = false) = 0;
  virtual const uint8_t* GetBlockBuffer() = 0;
  virtual size_t GetBlockSize() = 0;
  virtual FX_FILESIZE GetBlockOffset() = 0;
};

#ifdef PDF_ENABLE_XFA
class IFX_FileAccess : public CFX_Retainable {
 public:
  static CFX_RetainPtr<IFX_FileAccess> CreateDefault(
      const CFX_WideStringC& wsPath);

  virtual void GetPath(CFX_WideString& wsPath) = 0;
  virtual CFX_RetainPtr<IFX_SeekableStream> CreateFileStream(
      uint32_t dwModes) = 0;
};
#endif  // PDF_ENABLE_XFA

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
class CFindFileData {
 public:
  virtual ~CFindFileData() {}
  HANDLE m_Handle;
  bool m_bEnd;
};

class CFindFileDataA : public CFindFileData {
 public:
  ~CFindFileDataA() override {}
  WIN32_FIND_DATAA m_FindData;
};

class CFindFileDataW : public CFindFileData {
 public:
  ~CFindFileDataW() override {}
  WIN32_FIND_DATAW m_FindData;
};
#endif

#endif  // CORE_FXCRT_FX_STREAM_H_
