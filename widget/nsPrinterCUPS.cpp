/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterCUPS.h"
#include "nsPaper.h"
#include "nsPrinterBase.h"

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

// static
already_AddRefed<nsPrinterCUPS> nsPrinterCUPS::Create(const nsCUPSShim& aShim,
                                                      cups_dest_t* aPrinter) {
  return do_AddRef(new nsPrinterCUPS(aShim, aPrinter));
}

NS_IMETHODIMP
nsPrinterCUPS::GetName(nsAString& aName) {
  aName = NS_ConvertUTF8toUTF16(mPrinter->name);
  return NS_OK;
}

bool nsPrinterCUPS::SupportsDuplex() const {
  return Supports(CUPS_SIDES, CUPS_SIDES_TWO_SIDED_PORTRAIT);
}

bool nsPrinterCUPS::SupportsColor() const { return false; }

bool nsPrinterCUPS::Supports(const char* option, const char* value) const {
  MOZ_ASSERT(mPrinterInfo);
  return mShim.mCupsCheckDestSupported(CUPS_HTTP_DEFAULT, mPrinter,
                                       mPrinterInfo, option, value);
}

nsTArray<mozilla::PaperInfo> nsPrinterCUPS::PaperList() const {
  if (!mPrinterInfo) {
    return {};
  }

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

  if (!connection) {
    return {};
  }

  nsTArray<mozilla::PaperInfo> paperList;
  for (int i = 0; i < paperCount; ++i) {
    cups_size_t info;
    int getInfoSucceded = mShim.mCupsGetDestMediaByIndex(
        CUPS_HTTP_DEFAULT, mPrinter, mPrinterInfo, i, CUPS_MEDIA_FLAGS_DEFAULT,
        &info);

    if (!getInfoSucceded) {
      continue;
    }

    // localizedName is owned by mPrinterInfo.
    // https://www.cups.org/doc/cupspm.html#cupsLocalizeDestMedia
    const char* localizedName = mShim.mCupsLocalizeDestMedia(
        connection, mPrinter, mPrinterInfo, CUPS_MEDIA_FLAGS_DEFAULT, &info);

    if (!localizedName) {
      continue;
    }

    // XXX Do we actually have the guarantee that this is utf-8?
    NS_ConvertUTF8toUTF16 name(localizedName);
    const double kPointsPerHundredthMillimeter = 0.0283465;

    paperList.AppendElement(mozilla::PaperInfo{
        std::move(name),
        info.width * kPointsPerHundredthMillimeter,
        info.length * kPointsPerHundredthMillimeter,
        info.top * kPointsPerHundredthMillimeter,
        info.right * kPointsPerHundredthMillimeter,
        info.bottom * kPointsPerHundredthMillimeter,
        info.left * kPointsPerHundredthMillimeter,
    });
  }

  mShim.mHttpClose(connection);
  return paperList;
}
