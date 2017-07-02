// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_WIN32_CFX_WINDOWSDIB_H_
#define CORE_FXGE_WIN32_CFX_WINDOWSDIB_H_
#ifdef _WIN32
#ifndef _WINDOWS_
#include <windows.h>
#endif
#define WINDIB_OPEN_MEMORY 0x1
#define WINDIB_OPEN_PATHNAME 0x2

typedef struct WINDIB_Open_Args_ {
  int flags;

  const uint8_t* memory_base;

  size_t memory_size;

  const FX_WCHAR* path_name;
} WINDIB_Open_Args_;

class CFX_WindowsDIB : public CFX_DIBitmap {
 public:
  CFX_WindowsDIB(HDC hDC, int width, int height);
  ~CFX_WindowsDIB() override;

  static CFX_ByteString GetBitmapInfo(const CFX_DIBitmap* pBitmap);
  static CFX_DIBitmap* LoadFromBuf(BITMAPINFO* pbmi, void* pData);
  static HBITMAP GetDDBitmap(const CFX_DIBitmap* pBitmap, HDC hDC);
  static CFX_DIBitmap* LoadFromFile(const FX_WCHAR* filename);
  static CFX_DIBitmap* LoadFromFile(const FX_CHAR* filename);
  static CFX_DIBitmap* LoadDIBitmap(WINDIB_Open_Args_ args);

  HDC GetDC() const { return m_hMemDC; }
  HBITMAP GetWindowsBitmap() const { return m_hBitmap; }

  void LoadFromDevice(HDC hDC, int left, int top);
  void SetToDevice(HDC hDC, int left, int top);

 protected:
  HDC m_hMemDC;
  HBITMAP m_hBitmap;
  HBITMAP m_hOldBitmap;
};

#endif  // _WIN32

#endif  // CORE_FXGE_WIN32_CFX_WINDOWSDIB_H_
