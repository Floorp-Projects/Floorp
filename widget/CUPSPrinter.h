/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCUPSPrinter_h___
#define nsCUPSPrinter_h___

#include "nsCUPSShim.h"

#include "mozilla/Assertions.h"

namespace mozilla {

/**
 * @brief Interface to help implementing nsIPrinter using a CUPS printer.
 *
 * This will cause a blocking request on construction, and (depending on CUPS
 * scheduling) on attribute getters.
 */
class CUPSPrinter {
 public:
  CUPSPrinter(const CUPSPrinter&) = delete;
  CUPSPrinter(CUPSPrinter&& other)
      : mShim(other.mShim),
        mPrinter(other.mPrinter),
        mPrinterInfo(other.mPrinterInfo) {
    other.mPrinter = nullptr;
    other.mPrinterInfo = nullptr;
  }
  /**
   * @p aPrinter must not be null.
   */
  CUPSPrinter(const nsCUPSShim& aShim, cups_dest_t* aPrinter);
  ~CUPSPrinter();

  bool IsValid() const { return mPrinterInfo; }
  const char* Name() const { return mPrinter->name; }
  bool SupportsColor() const;
  bool SupportsTwoSidedLongEdgeBinding() const;
  bool SupportsTwoSidedShortEdgeBinding() const;

 private:
  // Little util for the Supports* functions using the direct CUPS names.
  bool Supports(const char* option, const char* value) const;

  const nsCUPSShim& mShim;
  cups_dest_t* mPrinter;
  cups_dinfo_t* mPrinterInfo;
};

}  // namespace mozilla

#endif /* nsCUPSPrinter_h___ */
