/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PDFVIAEMFPRINTHELPER_H_
#define PDFVIAEMFPRINTHELPER_H_

#include "nsCOMPtr.h"
#include "PDFiumEngineShim.h"
#include "mozilla/Vector.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/Shmem.h"

/* include windows.h for the HDC definitions that we need. */
#include <windows.h>

class nsIFile;
class nsFileInputStream;

namespace mozilla {

namespace ipc {
  class IShmemAllocator;
}

namespace widget {

/**
 * This class helps draw a PDF file to a given Windows DC.
 * To do that it first converts the PDF file to EMF.
 * Windows EMF:
 * https://msdn.microsoft.com/en-us/windows/hardware/drivers/print/emf-data-type
 */
class PDFViaEMFPrintHelper
{
public:
  typedef mozilla::ipc::FileDescriptor FileDescriptor;

  PDFViaEMFPrintHelper();
  virtual ~PDFViaEMFPrintHelper();

  /** Loads the specified PDF file. */
  NS_IMETHOD OpenDocument(nsIFile* aFile);
  NS_IMETHOD OpenDocument(const char* aFileName);
  NS_IMETHOD OpenDocument(const FileDescriptor& aFD);

  /** Releases document buffer. */
  void CloseDocument();

  int GetPageCount() const { return mPDFiumEngine->GetPageCount(mPDFDoc); }

  /**
   * Convert the specified PDF page to EMF and draw the EMF onto the
   * given DC.
   */
  bool DrawPage(HDC aPrinterDC, unsigned int aPageIndex,
                int aPageWidth, int aPageHeight);

  /** Convert the specified PDF page to EMF and save it to file. */
  bool SavePageToFile(const wchar_t* aFilePath, unsigned int aPageIndex,
                      int aPageWidth, int aPageHeight);

  /** Create a share memory and serialize the EMF content into it. */
  bool SavePageToBuffer(unsigned int aPageIndex, int aPageWidth,
                        int aPageHeight, ipc::Shmem& aMem,
                        mozilla::ipc::IShmemAllocator* aAllocator);

protected:
  virtual bool CreatePDFiumEngineIfNeed();
  bool RenderPageToDC(HDC aDC, unsigned int aPageIndex,
                      int aPageWidth, int aPageHeight);

  RefPtr<PDFiumEngineShim>    mPDFiumEngine;
  FPDF_DOCUMENT               mPDFDoc;
  PRFileDesc*                 mPrfile;
};

} // namespace widget
} // namespace mozilla

#endif /* PDFVIAEMFPRINTHELPER_H_ */
