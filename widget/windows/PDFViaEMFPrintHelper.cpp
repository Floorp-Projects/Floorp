/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFViaEMFPrintHelper.h"
#include "nsIFileStreams.h"
#include "WindowsEMF.h"
#include "nsFileStreams.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/File.h"
#include "mozilla/Unused.h"

static
float ComputeScaleFactor(int aDCWidth, int aDCHeight,
                         int aPageWidth, int aPageHeight)
{
  MOZ_ASSERT(aPageWidth !=0 && aPageWidth != 0);

  float scaleFactor = 1.0;
  // If page fits DC - no scaling needed.
  if (aDCWidth < aPageWidth || aDCHeight < aPageHeight) {
    float xFactor =
      static_cast<float>(aDCWidth) / static_cast <float>(aPageWidth);
    float yFactor =
      static_cast<float>(aDCHeight) / static_cast <float>(aPageHeight);
    scaleFactor = std::min(xFactor, yFactor);
  }
  return scaleFactor;
}

PDFViaEMFPrintHelper::PDFViaEMFPrintHelper()
  : mPDFDoc(nullptr)
{
}

PDFViaEMFPrintHelper::~PDFViaEMFPrintHelper()
{
  CloseDocument();
}

nsresult
PDFViaEMFPrintHelper::OpenDocument(nsIFile *aFile)
{
  MOZ_ASSERT(aFile);
  if (mPDFDoc) {
    MOZ_ASSERT_UNREACHABLE("We can only open one PDF at a time");
    return NS_ERROR_FAILURE;
  }

  if (!mPDFiumEngine) {
    mPDFiumEngine = PDFiumEngineShim::GetInstanceOrNull();
    NS_ENSURE_TRUE(mPDFiumEngine, NS_ERROR_FAILURE);
  }

  nsAutoCString nativePath;
  nsresult rv = aFile->GetNativePath(nativePath);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mPDFDoc = mPDFiumEngine->LoadDocument(nativePath.get(), nullptr);
  if (!mPDFDoc) {
    return NS_ERROR_FAILURE;
  }

  if (mPDFiumEngine->GetPageCount(mPDFDoc) < 1) {
    CloseDocument();
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

bool
PDFViaEMFPrintHelper::RenderPageToDC(HDC aDC, unsigned int aPageIndex,
                                     int aPageWidth, int aPageHeight)
{
  MOZ_ASSERT(aDC && mPDFDoc);
  MOZ_ASSERT(static_cast<int>(aPageIndex) <
             mPDFiumEngine->GetPageCount(mPDFDoc));

  if (aPageWidth <= 0 || aPageHeight <= 0) {
    return false;
  }

  FPDF_PAGE pdfPage = mPDFiumEngine->LoadPage(mPDFDoc, aPageIndex);
  NS_ENSURE_TRUE(pdfPage, false);

  int dcWidth = ::GetDeviceCaps(aDC, HORZRES);
  int dcHeight = ::GetDeviceCaps(aDC, VERTRES);
  float scaleFactor = ComputeScaleFactor(dcWidth,dcHeight,
                                         aPageWidth, aPageHeight);
  int savedState = ::SaveDC(aDC);
  ::SetGraphicsMode(aDC, GM_ADVANCED);
  XFORM xform = { 0 };
  xform.eM11 = xform.eM22 = scaleFactor;
  ::ModifyWorldTransform(aDC, &xform, MWT_LEFTMULTIPLY);

  // The caller wanted all drawing to happen within the bounds specified.
  // Based on scale calculations, our destination rect might be larger
  // than the bounds. Set the clip rect to the bounds.
  ::IntersectClipRect(aDC, 0, 0, aPageWidth, aPageHeight);

  mPDFiumEngine->RenderPage(aDC, pdfPage,
                            0, 0, aPageWidth, aPageHeight,
                            0, FPDF_ANNOT | FPDF_PRINTING | FPDF_NO_CATCH);
  mPDFiumEngine->ClosePage(pdfPage);
  ::RestoreDC(aDC, savedState);

  return true;
}

bool
PDFViaEMFPrintHelper::DrawPage(HDC aPrinterDC, unsigned int aPageIndex,
                               int aPageWidth, int aPageHeight)
{
  // There is a comment in Chromium.
  // https://cs.chromium.org/chromium/src/pdf/pdfium/pdfium_engine.cc?rcl=9ad9f6860b4d6a4ec7f7f975b2c99672e02d5d49&l=4008
  // Some PDFs seems to render very slowly if RenderPageToDC is directly used
  // on a printer DC.
  // The way Chromium works around the issue at the code linked above is to
  // print to a bitmap and send that to a printer.  Instead of doing that we
  // render to an EMF file and replay that on the printer DC.  It is unclear
  // whether our approach will avoid the performance issues though.  Bug
  // 1359298 covers investigating that.

  MOZ_ASSERT(aPrinterDC);
  WindowsEMF emf;
  bool result = emf.InitForDrawing();
  NS_ENSURE_TRUE(result, false);

  result = RenderPageToDC(emf.GetDC(), aPageIndex, aPageWidth, aPageHeight);
  NS_ENSURE_TRUE(result, false);

  RECT printRect = {0, 0, aPageWidth, aPageHeight};
  result = emf.Playback(aPrinterDC, &printRect);
  return result;
}

bool
PDFViaEMFPrintHelper::DrawPageToFile(const wchar_t* aFilePath,
                                     unsigned int aPageIndex,
                                     int aPageWidth, int aPageHeight)
{
  WindowsEMF emf;
  bool result = emf.InitForDrawing(aFilePath);
  NS_ENSURE_TRUE(result, false);

  result = RenderPageToDC(emf.GetDC(), aPageIndex, aPageWidth, aPageHeight);
  NS_ENSURE_TRUE(result, false);
  return emf.SaveToFile();
}

void
PDFViaEMFPrintHelper::CloseDocument()
{
  if (mPDFDoc) {
    mPDFiumEngine->CloseDocument(mPDFDoc);
    mPDFDoc = nullptr;
  }
}