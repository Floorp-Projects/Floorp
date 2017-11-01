/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PDFIUMENGINESHIM_H
#define PDFIUMENGINESHIM_H

#include "prlink.h"
#include "fpdfview.h"

/* include windows.h for the HDC definitions that we need. */
#include <windows.h>
#include "private/pprio.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace widget {

struct PDFFunctionPointerTable;

/**
 * This class exposes an interface to the PDFium library and
 * takes care of loading and linking to the appropriate PDFium symbols.
 */
class PDFiumEngineShim
{
public:

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PDFiumEngineShim)

  static already_AddRefed<PDFiumEngineShim> GetInstanceOrNull();
  // This function is used for testing purpose only, do not call it in regular
  // code.
  static already_AddRefed<PDFiumEngineShim>
  GetInstanceOrNull(const nsCString& aLibrary);

  FPDF_DOCUMENT LoadDocument(FPDF_STRING file_path,
                             FPDF_BYTESTRING aPassword);
  FPDF_DOCUMENT LoadDocument(PRFileDesc* aPrfile,
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

private:
  PDFiumEngineShim();
  ~PDFiumEngineShim();
  bool Init(const nsCString& aLibrary);

  UniquePtr<PDFFunctionPointerTable> mTable;
  bool        mInitialized ;

  PRLibrary*  mPRLibrary;
};

} // namespace widget
} // namespace mozilla

#endif /* PDFIUMENGINESHIM_H */