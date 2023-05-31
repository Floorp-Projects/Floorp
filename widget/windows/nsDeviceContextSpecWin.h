/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceContextSpecWin_h___
#define nsDeviceContextSpecWin_h___

#include "nsCOMPtr.h"
#include "nsIDeviceContextSpec.h"
#include "nsPrinterListBase.h"
#include "nsIPrintSettings.h"
#include <windows.h>
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/PrintPromise.h"

class nsIFile;
class nsIWidget;

class nsDeviceContextSpecWin : public nsIDeviceContextSpec {
 public:
  nsDeviceContextSpecWin();

  NS_DECL_ISUPPORTS

  already_AddRefed<PrintTarget> MakePrintTarget() final;
  NS_IMETHOD BeginDocument(const nsAString& aTitle,
                           const nsAString& aPrintToFileName,
                           int32_t aStartPage, int32_t aEndPage) override {
    return NS_OK;
  }
  RefPtr<mozilla::gfx::PrintEndDocumentPromise> EndDocument() override;
  NS_IMETHOD BeginPage(const IntSize& aSizeInPoints) override { return NS_OK; }
  NS_IMETHOD EndPage() override { return NS_OK; }

  NS_IMETHOD Init(nsIPrintSettings* aPS, bool aIsPrintPreview) override;

  void GetDriverName(nsAString& aDriverName) const {
    aDriverName = mDriverName;
  }
  void GetDeviceName(nsAString& aDeviceName) const {
    aDeviceName = mDeviceName;
  }

  // The GetDevMode will return a pointer to a DevMode
  // whether it is from the Global memory handle or just the DevMode
  // To get the DevMode from the Global memory Handle it must lock it
  // So this call must be paired with a call to UnlockGlobalHandle
  void GetDevMode(LPDEVMODEW& aDevMode);

  // helper functions
  nsresult GetDataFromPrinter(const nsAString& aName,
                              nsIPrintSettings* aPS = nullptr);

 protected:
  void SetDeviceName(const nsAString& aDeviceName);
  void SetDriverName(const nsAString& aDriverName);
  void SetDevMode(LPDEVMODEW aDevMode);

  virtual ~nsDeviceContextSpecWin();

  nsString mDriverName;
  nsString mDeviceName;
  LPDEVMODEW mDevMode = nullptr;

  int16_t mOutputFormat = nsIPrintSettings::kOutputFormatNative;

  // A temporary file to create an "anonymous" print target. See bug 1664253,
  // this should ideally not be needed.
  nsCOMPtr<nsIFile> mTempFile;
};

//-------------------------------------------------------------------------
// Printer List
//-------------------------------------------------------------------------
class nsPrinterListWin final : public nsPrinterListBase {
 public:
  NS_IMETHOD InitPrintSettingsFromPrinter(const nsAString&,
                                          nsIPrintSettings*) final;

  nsTArray<PrinterInfo> Printers() const final;
  RefPtr<nsIPrinter> CreatePrinter(PrinterInfo) const final;

  nsPrinterListWin() = default;

 protected:
  nsresult SystemDefaultPrinterName(nsAString&) const final;

  mozilla::Maybe<PrinterInfo> PrinterByName(nsString) const final;
  mozilla::Maybe<PrinterInfo> PrinterBySystemName(
      nsString aPrinterName) const final;

 private:
  ~nsPrinterListWin();
};

#endif
