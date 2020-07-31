/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinter.h"

nsPrinter::nsPrinter(const nsAString& aName,
                     const nsTArray<RefPtr<nsIPaper>>& aPaperList,
                     const bool aSupportsDuplex, const bool aSupportsColor)
    : mName(aName),
      mPaperList(aPaperList.Clone()),
      mSupportsDuplex(aSupportsDuplex),
      mSupportsColor(aSupportsColor) {}

NS_IMPL_ISUPPORTS(nsPrinter, nsIPrinter);

NS_IMETHODIMP
nsPrinter::GetName(nsAString& aName) {
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsPrinter::GetPaperList(nsTArray<RefPtr<nsIPaper>>& aPaperList) {
  aPaperList.Assign(mPaperList);
  return NS_OK;
}

NS_IMETHODIMP
nsPrinter::GetSupportsDuplex(bool* aSupportsDuplex) {
  MOZ_ASSERT(aSupportsDuplex);
  *aSupportsDuplex = mSupportsDuplex;
  return NS_OK;
}

NS_IMETHODIMP
nsPrinter::GetSupportsColor(bool* aSupportsColor) {
  MOZ_ASSERT(aSupportsColor);
  *aSupportsColor = mSupportsColor;
  return NS_OK;
}
