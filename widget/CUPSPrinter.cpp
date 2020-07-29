/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CUPSPrinter.h"

#include "mozilla/DebugOnly.h"

namespace mozilla {

CUPSPrinter::CUPSPrinter(const nsCUPSShim& aShim, cups_dest_t* aPrinter)
    : mShim(aShim) {
  MOZ_ASSERT(mShim.IsInitialized());
  MOZ_ASSERT(aPrinter);
  DebugOnly<const int> numCopied = aShim.mCupsCopyDest(aPrinter, 0, &mPrinter);
  MOZ_ASSERT(numCopied == 1);
  mPrinterInfo = aShim.mCupsCopyDestInfo(CUPS_HTTP_DEFAULT, mPrinter);
}

CUPSPrinter::~CUPSPrinter() {
  if (mPrinterInfo) {
    mShim.mCupsFreeDestInfo(mPrinterInfo);
  }
  if (mPrinter) {
    mShim.mCupsFreeDests(1, mPrinter);
  }
}

bool CUPSPrinter::SupportsColor() const {
  return Supports(CUPS_PRINT_COLOR_MODE, CUPS_PRINT_COLOR_MODE_COLOR);
}

bool CUPSPrinter::SupportsTwoSidedLongEdgeBinding() const {
  return Supports(CUPS_SIDES, CUPS_SIDES_TWO_SIDED_PORTRAIT);
}

bool CUPSPrinter::SupportsTwoSidedShortEdgeBinding() const {
  return Supports(CUPS_SIDES, CUPS_SIDES_TWO_SIDED_LANDSCAPE);
}

bool CUPSPrinter::Supports(const char* option, const char* value) const {
  MOZ_ASSERT(mPrinterInfo);
  return mShim.mCupsCheckDestSupported(CUPS_HTTP_DEFAULT, mPrinter,
                                       mPrinterInfo, option, value);
}

}  // namespace mozilla
