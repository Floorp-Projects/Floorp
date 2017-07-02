// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_CFX_WINDOWSDEVICE_H_
#define CORE_FXGE_CFX_WINDOWSDEVICE_H_

#ifdef _WIN32
#ifndef _WINDOWS_
#include <windows.h>
#endif

#include "core/fxge/cfx_renderdevice.h"

class IFX_RenderDeviceDriver;

#if defined(PDFIUM_PRINT_TEXT_WITH_GDI)
typedef void (*PDFiumEnsureTypefaceCharactersAccessible)(const LOGFONT* font,
                                                         const wchar_t* text,
                                                         size_t text_length);

extern bool g_pdfium_print_text_with_gdi;
extern PDFiumEnsureTypefaceCharactersAccessible
    g_pdfium_typeface_accessible_func;
#endif
extern int g_pdfium_print_postscript_level;

class CFX_WindowsDevice : public CFX_RenderDevice {
 public:
  static IFX_RenderDeviceDriver* CreateDriver(HDC hDC);

  explicit CFX_WindowsDevice(HDC hDC);
  ~CFX_WindowsDevice() override;

  HDC GetDC() const;
};

#endif  // _WIN32

#endif  // CORE_FXGE_CFX_WINDOWSDEVICE_H_
