/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPaperWin.h"

#include "nsWindowsHelpers.h"
#include "WinUtils.h"

using namespace mozilla::widget;

static const wchar_t kDriverName[] = L"WINSPOOL";

nsPaperWin::nsPaperWin(WORD aId, const nsAString& aName,
                       const nsAString& aPrinterName, double aWidth,
                       double aHeight)
    : mId(aId),
      mName(aName),
      mPrinterName(aPrinterName),
      mSize(aWidth, aHeight) {}

NS_IMPL_ISUPPORTS(nsPaperWin, nsIPaper);

void nsPaperWin::EnsureUnwriteableMargin() {
  if (mUnwriteableMarginRetrieved) {
    return;
  }

  // We need a DEVMODE to set the paper size on the context.
  DEVMODEW devmode = {};
  devmode.dmSize = sizeof(DEVMODEW);
  devmode.dmFields = DM_PAPERSIZE;
  devmode.dmPaperSize = mId;

  // Create an information context, so that we can get margin information.
  // Note: this blocking call interacts with the driver and can be slow.
  nsAutoHDC printerDc(
      ::CreateICW(kDriverName, mPrinterName.get(), nullptr, &devmode));

  MarginDouble unWrtMarginInInches =
      WinUtils::GetUnwriteableMarginsForDeviceInInches(printerDc);
  mUnwriteableMargin.SizeTo(unWrtMarginInInches.top * POINTS_PER_INCH_FLOAT,
                            unWrtMarginInInches.right * POINTS_PER_INCH_FLOAT,
                            unWrtMarginInInches.bottom * POINTS_PER_INCH_FLOAT,
                            unWrtMarginInInches.left * POINTS_PER_INCH_FLOAT);
  mUnwriteableMarginRetrieved = true;
}

NS_IMETHODIMP
nsPaperWin::GetName(nsAString& aName) {
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperWin::GetWidth(double* aWidth) {
  MOZ_ASSERT(aWidth);
  *aWidth = mSize.Width();
  return NS_OK;
}

NS_IMETHODIMP
nsPaperWin::GetHeight(double* aHeight) {
  MOZ_ASSERT(aHeight);
  *aHeight = mSize.Height();
  return NS_OK;
}

NS_IMETHODIMP
nsPaperWin::GetUnwriteableMarginTop(double* aUnwriteableMarginTop) {
  MOZ_ASSERT(aUnwriteableMarginTop);
  EnsureUnwriteableMargin();
  *aUnwriteableMarginTop = mUnwriteableMargin.top;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperWin::GetUnwriteableMarginBottom(double* aUnwriteableMarginBottom) {
  MOZ_ASSERT(aUnwriteableMarginBottom);
  EnsureUnwriteableMargin();
  *aUnwriteableMarginBottom = mUnwriteableMargin.bottom;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperWin::GetUnwriteableMarginLeft(double* aUnwriteableMarginLeft) {
  MOZ_ASSERT(aUnwriteableMarginLeft);
  EnsureUnwriteableMargin();
  *aUnwriteableMarginLeft = mUnwriteableMargin.left;
  return NS_OK;
}

NS_IMETHODIMP
nsPaperWin::GetUnwriteableMarginRight(double* aUnwriteableMarginRight) {
  MOZ_ASSERT(aUnwriteableMarginRight);
  EnsureUnwriteableMargin();
  *aUnwriteableMarginRight = mUnwriteableMargin.right;
  return NS_OK;
}
