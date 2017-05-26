/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PDFIUMENGINESHIM_H
#define PDFIUMENGINESHIM_H

#include "prlink.h"

/* include windows.h for the HDC definitions that we need. */
#include <windows.h>

namespace mozilla {
namespace widget {
/**
 * Create Bug 1349139 to replace following defines with an appropriate include file.
 */
// Page rendering flags. They can be combined with bit-wise OR.
//
// Set if annotations are to be rendered.
#define FPDF_ANNOT 0x01
// Set if using text rendering optimized for LCD display.
#define FPDF_LCD_TEXT 0x02
// Don't use the native text output available on some platforms
#define FPDF_NO_NATIVETEXT 0x04
// Grayscale output.
#define FPDF_GRAYSCALE 0x08
// Set if you want to get some debug info.
#define FPDF_DEBUG_INFO 0x80
// Set if you don't want to catch exceptions.
#define FPDF_NO_CATCH 0x100
// Limit image cache size.
#define FPDF_RENDER_LIMITEDIMAGECACHE 0x200
// Always use halftone for image stretching.
#define FPDF_RENDER_FORCEHALFTONE 0x400
// Render for printing.
#define FPDF_PRINTING 0x800
// Set to disable anti-aliasing on text.
#define FPDF_RENDER_NO_SMOOTHTEXT 0x1000
// Set to disable anti-aliasing on images.
#define FPDF_RENDER_NO_SMOOTHIMAGE 0x2000
// Set to disable anti-aliasing on paths.
#define FPDF_RENDER_NO_SMOOTHPATH 0x4000
// Set whether to render in a reverse Byte order, this flag is only used when
// rendering to a bitmap.
#define FPDF_REVERSE_BYTE_ORDER 0x10

// PDF types
typedef void* FPDF_ACTION;
typedef void* FPDF_BITMAP;
typedef void* FPDF_BOOKMARK;
typedef void* FPDF_CLIPPATH;
typedef void* FPDF_DEST;
typedef void* FPDF_DOCSCHHANDLE;
typedef void* FPDF_DOCUMENT;
typedef void* FPDF_FONT;
typedef void* FPDF_HMODULE;
typedef void* FPDF_LINK;
typedef void* FPDF_MODULEMGR;
typedef void* FPDF_PAGE;
typedef void* FPDF_PAGELINK;
typedef void* FPDF_PAGEOBJECT;  // Page object(text, path, etc)
typedef void* FPDF_PAGERANGE;
typedef void* FPDF_PATH;
typedef void* FPDF_RECORDER;
typedef void* FPDF_SCHHANDLE;
typedef void* FPDF_STRUCTELEMENT;
typedef void* FPDF_STRUCTTREE;
typedef void* FPDF_TEXTPAGE;
typedef const char* FPDF_STRING;

// Basic data types
typedef int FPDF_BOOL;
typedef int FPDF_ERROR;
typedef unsigned long FPDF_DWORD;
typedef float FS_FLOAT;

typedef const char* FPDF_BYTESTRING;

typedef void (*FPDF_InitLibrary_Pfn)();
typedef void (*FPDF_DestroyLibrary_Pfn)();
typedef FPDF_DOCUMENT (*FPDF_LoadMemDocument_Pfn)(const void* aDataBuf,
                                                  int aSize,
                                                  FPDF_BYTESTRING aPassword);
typedef FPDF_DOCUMENT (*FPDF_LoadDocument_Pfn)(FPDF_STRING file_path,
                                               FPDF_BYTESTRING password);

typedef void(*FPDF_CloseDocument_Pfn)(FPDF_DOCUMENT aDocument);

typedef int (*FPDF_GetPageCount_Pfn)(FPDF_DOCUMENT aDocument);
typedef int (*FPDF_GetPageSizeByIndex_Pfn)(FPDF_DOCUMENT aDocument,
                                           int aPageIndex,
                                           double* aWidth,
                                           double* aWeight);

typedef FPDF_PAGE (*FPDF_LoadPage_Pfn)(FPDF_DOCUMENT aDocument, int aPageIndex);
typedef void (*FPDF_ClosePage_Pfn)(FPDF_PAGE aPage);
typedef void (*FPDF_RenderPage_Pfn)(HDC aDC,
                                    FPDF_PAGE aPage,
                                    int aStartX,
                                    int aStartY,
                                    int aSizeX,
                                    int aSizeY,
                                    int aRotate,
                                    int aFlags);
typedef void (*FPDF_RenderPage_Close_Pfn) (FPDF_PAGE aPage);



/**
 * This class exposes an interface to the PDFium library and
 * takes care of loading and linking to the appropriate PDFium symbols.
 */
class PDFiumEngineShim
{
public:

  explicit PDFiumEngineShim(PRLibrary* aLibrary);
  ~PDFiumEngineShim();

  FPDF_DOCUMENT LoadMemDocument(const void* aDataBuf,
                                int aSize,
                                FPDF_BYTESTRING aPassword);
  FPDF_DOCUMENT LoadDocument(FPDF_STRING file_path,
                             FPDF_BYTESTRING aPassword);
  void CloseDocument(FPDF_DOCUMENT aDocument);
  int GetPageCount(FPDF_DOCUMENT aDocument);
  int GetPageSizeByIndex(FPDF_DOCUMENT aDocument, int aPageIndex,
                         double* aWidth, double* aHeight);

  FPDF_PAGE LoadPage(FPDF_DOCUMENT aDocument, int aPageIndex);
  void ClosePage(FPDF_PAGE aPage);
  void RenderPage(HDC aDC, FPDF_PAGE aPage,
                  int aStartX, int aStartY,
                  int aSizeX, int aSizeY,
                  int aRotate, int aFlags);

  void RenderPage_Close(FPDF_PAGE aPage);

private:

  bool InitSymbolsAndLibrary();
  void InitLibrary();
  void DestroyLibrary();

  PRLibrary*  mPRLibrary;
  bool        mInitialized ;

  FPDF_InitLibrary_Pfn        mFPDF_InitLibrary;
  FPDF_DestroyLibrary_Pfn     mFPDF_DestroyLibrary;
  FPDF_LoadMemDocument_Pfn    mFPDF_LoadMemDocument;
  FPDF_LoadDocument_Pfn       mFPDF_LoadDocument;
  FPDF_CloseDocument_Pfn      mFPDF_CloseDocument;
  FPDF_GetPageCount_Pfn       mFPDF_GetPageCount;
  FPDF_GetPageSizeByIndex_Pfn mFPDF_GetPageSizeByIndex;
  FPDF_LoadPage_Pfn           mFPDF_LoadPage;
  FPDF_ClosePage_Pfn          mFPDF_ClosePage;
  FPDF_RenderPage_Pfn         mFPDF_RenderPage;
  FPDF_RenderPage_Close_Pfn   mFPDF_RenderPage_Close;

};

} // namespace widget
} // namespace mozilla

#endif /* PDFIUMENGINESHIM_H */