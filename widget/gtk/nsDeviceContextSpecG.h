/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceContextSpecGTK_h___
#define nsDeviceContextSpecGTK_h___

struct JSContext;

#include "nsIDeviceContextSpec.h"
#include "nsIPrinterList.h"
#include "nsIPrintSettings.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "mozilla/gfx/PrintPromise.h"

#include "nsCRT.h" /* should be <limits.h>? */

#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

#define NS_PORTRAIT 0
#define NS_LANDSCAPE 1

class nsPrintSettingsGTK;

class nsDeviceContextSpecGTK : public nsIDeviceContextSpec {
 public:
  nsDeviceContextSpecGTK();

  NS_DECL_ISUPPORTS

  already_AddRefed<PrintTarget> MakePrintTarget() final;

  NS_IMETHOD Init(nsIPrintSettings* aPS, bool aIsPrintPreview) override;
  NS_IMETHOD BeginDocument(const nsAString& aTitle,
                           const nsAString& aPrintToFileName,
                           int32_t aStartPage, int32_t aEndPage) override;
  RefPtr<mozilla::gfx::PrintEndDocumentPromise> EndDocument() override;
  NS_IMETHOD BeginPage(const IntSize& aSizeInPoints) override { return NS_OK; }
  NS_IMETHOD EndPage() override { return NS_OK; }

 protected:
  virtual ~nsDeviceContextSpecGTK();
  GtkPrintSettings* mGtkPrintSettings;
  GtkPageSetup* mGtkPageSetup;

  nsCString mSpoolName;
  nsCOMPtr<nsIFile> mSpoolFile;
  nsCString mTitle;

 private:
  void EnumeratePrinters();
  void StartPrintJob();
  static gboolean PrinterEnumerator(GtkPrinter* aPrinter, gpointer aData);
};

#endif /* !nsDeviceContextSpecGTK_h___ */
