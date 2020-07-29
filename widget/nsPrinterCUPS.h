/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterCUPS_h___
#define nsPrinterCUPS_h___

#include "nsCUPSShim.h"
#include "nsIPaper.h"
#include "nsIPrinter.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

#include "mozilla/Assertions.h"

/**
 * @brief Interface to help implementing nsIPrinter using a CUPS printer.
 */
class nsPrinterCUPS final : public nsIPrinter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTER
  nsPrinterCUPS() = delete;
  nsPrinterCUPS(const nsPrinterCUPS&) = delete;
  nsPrinterCUPS(nsPrinterCUPS&& aOther)
      : mShim(aOther.mShim),
        mPrinter(aOther.mPrinter),
        mPrinterInfo(aOther.mPrinterInfo),
        mPaperList(std::move(aOther.mPaperList)),
        mSupportsDuplex(aOther.mSupportsDuplex) {
    aOther.mPrinter = nullptr;
    aOther.mPrinterInfo = nullptr;
  }

  /**
   * @p aPrinter must not be null.
   * @todo: Once CUPS-enumeration of paper sizes lands, we can remove the
   * |aPaperList|.
   */
  nsPrinterCUPS(const nsCUPSShim& aShim, cups_dest_t* aPrinter,
                nsTArray<RefPtr<nsIPaper>>&& aPaperList);

 private:
  ~nsPrinterCUPS();

  // Little util for getting support flags using the direct CUPS names.
  bool Supports(const char* option, const char* value) const;

  const nsCUPSShim& mShim;
  cups_dest_t* mPrinter;
  cups_dinfo_t* mPrinterInfo;
  nsTArray<RefPtr<nsIPaper>> mPaperList;
  Maybe<bool> mSupportsDuplex;
};

#endif /* nsPrinterCUPS_h___ */
