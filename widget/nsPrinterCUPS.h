/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterCUPS_h___
#define nsPrinterCUPS_h___

#include "nsPrinterBase.h"
#include "nsPrintSettingsImpl.h"
#include "nsCUPSShim.h"
#include "nsString.h"

/**
 * @brief Interface to help implementing nsIPrinter using a CUPS printer.
 */
class nsPrinterCUPS final : public nsPrinterBase {
 public:
  NS_IMETHOD GetName(nsAString& aName) override;
  PrintSettingsInitializer DefaultSettings() const final;
  bool SupportsDuplex() const final;
  bool SupportsColor() const final;
  bool SupportsCollation() const final;
  nsTArray<mozilla::PaperInfo> PaperList() const final;
  MarginDouble GetMarginsForPaper(uint64_t) const final {
    MOZ_ASSERT_UNREACHABLE(
        "The CUPS API requires us to always get the margin when fetching the "
        "paper list so there should be no need to query it separately");
    return {};
  }

  nsPrinterCUPS() = delete;

  nsPrinterCUPS(const nsCUPSShim& aShim, nsString aDisplayName,
                cups_dest_t* aPrinter, cups_dinfo_t* aPrinterInfo)
      : mShim(aShim),
        mDisplayName(std::move(aDisplayName)),
        mPrinter(aPrinter),
        mPrinterInfo(aPrinterInfo) {}

 private:
  ~nsPrinterCUPS();

  /**
   * Retrieves the localized name for a given media (paper).
   */
  const char* LocalizeMediaName(http_t& aConnection, cups_size_t& aMedia) const;

  void GetPrinterName(nsAString& aName) const;

  // Little util for getting support flags using the direct CUPS names.
  bool Supports(const char* option, const char* value) const;

  const nsCUPSShim& mShim;
  nsString mDisplayName;
  cups_dest_t* mPrinter;
  cups_dinfo_t* mPrinterInfo;
};

#endif /* nsPrinterCUPS_h___ */
