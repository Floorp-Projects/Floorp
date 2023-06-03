/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceContextSpecX_h_
#define nsDeviceContextSpecX_h_

#include "nsIDeviceContextSpec.h"
#include "nsIPrinter.h"
#include "nsIPrinterList.h"

#include "nsCOMPtr.h"

#include "mozilla/gfx/PrintPromise.h"

#include <ApplicationServices/ApplicationServices.h>

class nsDeviceContextSpecX : public nsIDeviceContextSpec {
 public:
  NS_DECL_ISUPPORTS

  nsDeviceContextSpecX();

  NS_IMETHOD Init(nsIPrintSettings* aPS, bool aIsPrintPreview) override;
  already_AddRefed<PrintTarget> MakePrintTarget() final;
  NS_IMETHOD BeginDocument(const nsAString& aTitle,
                           const nsAString& aPrintToFileName,
                           int32_t aStartPage, int32_t aEndPage) override;
  RefPtr<mozilla::gfx::PrintEndDocumentPromise> EndDocument() override;
  NS_IMETHOD BeginPage(const IntSize& aSizeInPoints) override { return NS_OK; };
  NS_IMETHOD EndPage() override { return NS_OK; };

  void GetPaperRect(double* aTop, double* aLeft, double* aBottom,
                    double* aRight);

 protected:
  virtual ~nsDeviceContextSpecX();

 protected:
  PMPrintSession mPrintSession = nullptr;
  PMPageFormat mPageFormat = nullptr;
  PMPrintSettings mPMPrintSettings = nullptr;
  nsCOMPtr<nsIOutputStream> mOutputStream;  // Output stream from settings.
#ifdef MOZ_ENABLE_SKIA_PDF
  // file "print" output generated if printing via PDF
  nsCOMPtr<nsIFile> mTempFile;
#endif
 private:
  nsresult DoEndDocument();
};

#endif  // nsDeviceContextSpecX_h_
