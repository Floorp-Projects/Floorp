/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterCUPS.h"

nsPrinterCUPS::nsPrinterCUPS(const nsCUPSShim& aShim, cups_dest_t* aPrinter,
                             nsTArray<RefPtr<nsIPaper>>&& aPaperList)
    : mShim(aShim), mPaperList(std::move(aPaperList)) {
  MOZ_ASSERT(aPrinter);
  MOZ_ASSERT(mShim.IsInitialized());
  DebugOnly<const int> numCopied = aShim.mCupsCopyDest(aPrinter, 0, &mPrinter);
  MOZ_ASSERT(numCopied == 1);
  mPrinterInfo = aShim.mCupsCopyDestInfo(CUPS_HTTP_DEFAULT, mPrinter);
}

nsPrinterCUPS::~nsPrinterCUPS() {
  if (mPrinterInfo) {
    mShim.mCupsFreeDestInfo(mPrinterInfo);
  }
  if (mPrinter) {
    mShim.mCupsFreeDests(1, mPrinter);
  }
}

NS_IMPL_ISUPPORTS(nsPrinterCUPS, nsIPrinter);

NS_IMETHODIMP
nsPrinterCUPS::GetName(nsAString& aName) {
  aName = NS_ConvertUTF8toUTF16(mPrinter->name);
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterCUPS::GetPaperList(nsTArray<RefPtr<nsIPaper>>& aPaperList) {
  aPaperList.Assign(mPaperList);
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterCUPS::GetSupportsDuplex(bool* aSupportsDuplex) {
  MOZ_ASSERT(aSupportsDuplex);
  if (mSupportsDuplex.isNothing()) {
    mSupportsDuplex = Some(Supports(CUPS_SIDES, CUPS_SIDES_TWO_SIDED_PORTRAIT));
  }
  *aSupportsDuplex = mSupportsDuplex.value();
  return NS_OK;
}

bool nsPrinterCUPS::Supports(const char* option, const char* value) const {
  MOZ_ASSERT(mPrinterInfo);
  return mShim.mCupsCheckDestSupported(CUPS_HTTP_DEFAULT, mPrinter,
                                       mPrinterInfo, option, value);
}
