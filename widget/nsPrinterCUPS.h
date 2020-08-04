/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterCUPS_h___
#define nsPrinterCUPS_h___

#include "nsPrinterBase.h"
#include "nsCUPSShim.h"
#include "nsString.h"

/**
 * @brief Interface to help implementing nsIPrinter using a CUPS printer.
 */
class nsPrinterCUPS final : public nsPrinterBase {
 public:
  NS_IMETHOD GetName(nsAString& aName) override;
  NS_IMETHOD GetPaperList(nsTArray<RefPtr<nsIPaper>>& aPaperList) override;
  bool SupportsDuplex() const final;
  bool SupportsColor() const final;

  nsPrinterCUPS() = delete;

  /**
   * @p aPrinter must not be null.
   * @todo: Once CUPS-enumeration of paper sizes lands, we can remove the
   * |aPaperList|.
   */
  static already_AddRefed<nsPrinterCUPS> Create(
      const nsCUPSShim& aShim, cups_dest_t* aPrinter,
      nsTArray<RefPtr<nsIPaper>>&& aPaperList);

 private:
  nsPrinterCUPS(const nsCUPSShim& aShim, cups_dest_t* aPrinter,
                nsTArray<RefPtr<nsIPaper>>&& aPaperList);

  ~nsPrinterCUPS();

  // Little util for getting support flags using the direct CUPS names.
  bool Supports(const char* option, const char* value) const;

  const nsCUPSShim& mShim;
  cups_dest_t* mPrinter;
  cups_dinfo_t* mPrinterInfo;
};

#endif /* nsPrinterCUPS_h___ */
