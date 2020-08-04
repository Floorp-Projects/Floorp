/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterCUPS.h"

#include "mozilla/ResultExtensions.h"
#include "nsPaperCUPS.h"

nsPrinterCUPS::nsPrinterCUPS(const nsCUPSShim& aShim, cups_dest_t* aPrinter)
    : mShim(aShim) {
  MOZ_ASSERT(aPrinter);
  MOZ_ASSERT(mShim.IsInitialized());
  DebugOnly<const int> numCopied = aShim.mCupsCopyDest(aPrinter, 0, &mPrinter);
  MOZ_ASSERT(numCopied == 1);
  mPrinterInfo = aShim.mCupsCopyDestInfo(CUPS_HTTP_DEFAULT, mPrinter);
}

nsPrinterCUPS::~nsPrinterCUPS() {
  if (mPrinterInfo) {
    mShim.mCupsFreeDestInfo(mPrinterInfo);
    mPrinterInfo = nullptr;
  }
  if (mPrinter) {
    mShim.mCupsFreeDests(1, mPrinter);
    mPrinter = nullptr;
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
  if (!mPaperList) {
    MOZ_TRY(InitPaperList());
  }
  aPaperList.Assign(*mPaperList);
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

NS_IMETHODIMP
nsPrinterCUPS::GetSupportsColor(bool* aSupportsColor) {
  MOZ_ASSERT(aSupportsColor);
  // Dummy implementation waiting platform specific one.
  *aSupportsColor = false;
  return NS_OK;
}

bool nsPrinterCUPS::Supports(const char* option, const char* value) const {
  MOZ_ASSERT(mPrinterInfo);
  return mShim.mCupsCheckDestSupported(CUPS_HTTP_DEFAULT, mPrinter,
                                       mPrinterInfo, option, value);
}

nsresult nsPrinterCUPS::InitPaperList() {
  if (mPaperList) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(mPrinterInfo, NS_ERROR_FAILURE);

  const int paperCount = mShim.mCupsGetDestMediaCount(
      CUPS_HTTP_DEFAULT, mPrinter, mPrinterInfo, CUPS_MEDIA_FLAGS_DEFAULT);

  // blocking call
  http_t* connection = mShim.mCupsConnectDest(mPrinter, CUPS_DEST_FLAGS_NONE,
                                              /* timeout(ms) */ 5000,
                                              /* cancel */ nullptr,
                                              /* resource */ nullptr,
                                              /* resourcesize */ 0,
                                              /* callback */ nullptr,
                                              /* user_data */ nullptr);

  NS_ENSURE_TRUE(connection, NS_ERROR_FAILURE);

  mPaperList = MakeUnique<nsTArray<RefPtr<nsIPaper>>>(paperCount);
  for (int i = 0; i < paperCount; ++i) {
    cups_size_t paperInfo;
    int getInfoSucceded = mShim.mCupsGetDestMediaByIndex(
        CUPS_HTTP_DEFAULT, mPrinter, mPrinterInfo, i, CUPS_MEDIA_FLAGS_DEFAULT,
        &paperInfo);

    if (!getInfoSucceded) {
      continue;
    }

    // localizedName is owned by mPrinterInfo.
    // https://www.cups.org/doc/cupspm.html#cupsLocalizeDestMedia
    const char* localizedName =
        mShim.mCupsLocalizeDestMedia(connection, mPrinter, mPrinterInfo,
                                     CUPS_MEDIA_FLAGS_DEFAULT, &paperInfo);

    if (!localizedName) {
      continue;
    }

    mPaperList->AppendElement(new nsPaperCUPS(paperInfo, localizedName));
  }

  mShim.mHttpClose(connection);
  connection = nullptr;

  return NS_OK;
}
