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

#include "mozilla/DataMutex.h"
#include "mozilla/RecursiveMutex.h"

/**
 * @brief Interface to help implementing nsIPrinter using a CUPS printer.
 */
class nsPrinterCUPS final : public nsPrinterBase {
 public:
  NS_IMETHOD GetName(nsAString& aName) override;
  NS_IMETHOD GetSystemName(nsAString& aName) override;
  PrintSettingsInitializer DefaultSettings() const final;
  bool SupportsDuplex() const final;
  bool SupportsColor() const final;
  bool SupportsMonochrome() const final;
  bool SupportsCollation() const final;
  nsTArray<mozilla::PaperInfo> PaperList() const final;
  MarginDouble GetMarginsForPaper(nsString aPaperId) const final {
    MOZ_ASSERT_UNREACHABLE(
        "The CUPS API requires us to always get the margin when fetching the "
        "paper list so there should be no need to query it separately");
    return {};
  }

  nsPrinterCUPS() = delete;

  nsPrinterCUPS(const nsCUPSShim& aShim, nsString aDisplayName,
                cups_dest_t* aPrinter)
      : mShim(aShim),
        mDisplayName(std::move(aDisplayName)),
        mPrinter(aPrinter),
        mPrinterInfoMutex("nsPrinterCUPS::mPrinterInfoMutex") {}

 private:
  struct CUPSPrinterInfo {
    cups_dinfo_t* mPrinterInfo = nullptr;
    uint64_t mCUPSMajor = 0;
    uint64_t mCUPSMinor = 0;
    uint64_t mCUPSPatch = 0;

    // Tracks whether we have attempted to fetch mPrinterInfo yet.
    bool mWasInited = false;
    CUPSPrinterInfo() = default;
    CUPSPrinterInfo(const CUPSPrinterInfo&) = delete;
    CUPSPrinterInfo(CUPSPrinterInfo&&) = delete;
  };

  using PrinterInfoMutex =
      mozilla::DataMutexBase<CUPSPrinterInfo, mozilla::RecursiveMutex>;

  ~nsPrinterCUPS();

  /**
   * Retrieves the localized name for a given media (paper).
   * Returns nullptr if the name cannot be localized.
   */
  const char* LocalizeMediaName(http_t& aConnection, cups_size_t& aMedia) const;

  void GetPrinterName(nsAString& aName) const;

  // Little util for getting support flags using the direct CUPS names.
  bool Supports(const char* aOption, const char* aValue) const;

  // Returns support value if CUPS meets the minimum version, otherwise returns
  // |aDefault|
  bool IsCUPSVersionAtLeast(uint64_t aCUPSMajor, uint64_t aCUPSMinor,
                            uint64_t aCUPSPatch) const;

  void EnsurePrinterInfo(CUPSPrinterInfo& aInOutPrinterInfo) const;

  const nsCUPSShim& mShim;
  nsString mDisplayName;
  cups_dest_t* mPrinter;
  mutable PrinterInfoMutex mPrinterInfoMutex;
};

#endif /* nsPrinterCUPS_h___ */
