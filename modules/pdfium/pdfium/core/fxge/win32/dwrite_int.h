// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_WIN32_DWRITE_INT_H_
#define CORE_FXGE_WIN32_DWRITE_INT_H_

#ifndef DECLSPEC_UUID
#if (_MSC_VER >= 1100) && defined(__cplusplus)
#define DECLSPEC_UUID(x) __declspec(uuid(x))
#else
#define DECLSPEC_UUID(x)
#endif
#endif
#ifndef DECLSPEC_NOVTABLE
#if (_MSC_VER >= 1100) && defined(__cplusplus)
#define DECLSPEC_NOVTABLE __declspec(novtable)
#else
#define DECLSPEC_NOVTABLE
#endif
#endif
#if (WINVER < 0x0500)
#ifndef _MAC
DECLARE_HANDLE(HMONITOR);
#endif
#endif
class CDWriteExt {
 public:
  CDWriteExt();
  ~CDWriteExt();

  void Load();
  void Unload();

  bool IsAvailable() { return !!m_pDWriteFactory; }

  void* DwCreateFontFaceFromStream(uint8_t* pData,
                                   uint32_t size,
                                   int simulation_style);
  bool DwCreateRenderingTarget(CFX_DIBitmap* pSrc, void** renderTarget);
  void DwDeleteRenderingTarget(void* renderTarget);
  bool DwRendingString(void* renderTarget,
                       CFX_ClipRgn* pClipRgn,
                       FX_RECT& stringRect,
                       CFX_Matrix* pMatrix,
                       void* font,
                       FX_FLOAT font_size,
                       FX_ARGB text_color,
                       int glyph_count,
                       unsigned short* glyph_indices,
                       FX_FLOAT baselineOriginX,
                       FX_FLOAT baselineOriginY,
                       void* glyph_offsets,
                       FX_FLOAT* glyph_advances);
  void DwDeleteFont(void* pFont);

 protected:
  void* m_hModule;
  void* m_pDWriteFactory;
  void* m_pDwFontContext;
  void* m_pDwTextRenderer;
};

#endif  // CORE_FXGE_WIN32_DWRITE_INT_H_
