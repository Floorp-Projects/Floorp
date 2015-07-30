/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceContextSpecGTK_h___
#define nsDeviceContextSpecGTK_h___

#include "nsIDeviceContextSpec.h"
#include "nsIPrintSettings.h"
#include "nsIPrintOptions.h" 
#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

#include "nsCRT.h" /* should be <limits.h>? */

#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

#define NS_PORTRAIT  0
#define NS_LANDSCAPE 1

class nsPrintSettingsGTK;

class nsDeviceContextSpecGTK : public nsIDeviceContextSpec
{
public:
  nsDeviceContextSpecGTK();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetSurfaceForPrinter(gfxASurface **surface) override;

  NS_IMETHOD Init(nsIWidget *aWidget, nsIPrintSettings* aPS,
                  bool aIsPrintPreview) override;
  NS_IMETHOD BeginDocument(const nsAString& aTitle, char16_t * aPrintToFileName,
                           int32_t aStartPage, int32_t aEndPage) override;
  NS_IMETHOD EndDocument() override;
  NS_IMETHOD BeginPage() override { return NS_OK; }
  NS_IMETHOD EndPage() override { return NS_OK; }

protected:
  virtual ~nsDeviceContextSpecGTK();
  nsCOMPtr<nsPrintSettingsGTK> mPrintSettings;
  bool mToPrinter : 1;      /* If true, print to printer */
  bool mIsPPreview : 1;     /* If true, is print preview */
  char   mPath[PATH_MAX];     /* If toPrinter = false, dest file */
  char   mPrinter[256];       /* Printer name */
  GtkPrintSettings* mGtkPrintSettings;
  GtkPageSetup*     mGtkPageSetup;

  nsCString         mSpoolName;
  nsCOMPtr<nsIFile> mSpoolFile;
  nsCString         mTitle;

private:
  void EnumeratePrinters();
  void StartPrintJob();
  static gboolean PrinterEnumerator(GtkPrinter *aPrinter, gpointer aData);
};

//-------------------------------------------------------------------------
// Printer Enumerator
//-------------------------------------------------------------------------
class nsPrinterEnumeratorGTK final : public nsIPrinterEnumerator
{
  ~nsPrinterEnumeratorGTK() {}
public:
  nsPrinterEnumeratorGTK();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTERENUMERATOR
};

#endif /* !nsDeviceContextSpecGTK_h___ */
