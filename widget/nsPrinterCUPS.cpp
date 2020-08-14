/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterCUPS.h"
#include "nsPaper.h"
#include "nsPrinterBase.h"

#include "plstr.h"

nsPrinterCUPS::~nsPrinterCUPS() {
  if (mPrinterInfo) {
    mShim.cupsFreeDestInfo(mPrinterInfo);
    mPrinterInfo = nullptr;
  }
  if (mPrinter) {
    mShim.cupsFreeDests(1, mPrinter);
    mPrinter = nullptr;
  }
}

NS_IMETHODIMP
nsPrinterCUPS::GetName(nsAString& aName) {
  if (mDisplayName.IsEmpty()) {
    aName.Truncate();
    CopyUTF8toUTF16(MakeStringSpan(mPrinter->name), aName);
  } else {
    aName = mDisplayName;
  }
  return NS_OK;
}

bool nsPrinterCUPS::SupportsDuplex() const {
  return Supports(CUPS_SIDES, CUPS_SIDES_TWO_SIDED_PORTRAIT);
}

bool nsPrinterCUPS::SupportsColor() const {
  return Supports(CUPS_PRINT_COLOR_MODE, CUPS_PRINT_COLOR_MODE_COLOR);
}

bool nsPrinterCUPS::SupportsCollation() const {
  // We can't depend on cupsGetIntegerOption existing.
  const char* const value = mShim.cupsGetOption(
      "printer-type", mPrinter->num_options, mPrinter->options);
  if (!value) {
    return false;
  }
  // If the value is non-numeric, then atoi will return 0, which will still
  // cause this function to return false.
  const int type = atoi(value);
  return type & CUPS_PRINTER_COLLATE;
}

bool nsPrinterCUPS::Supports(const char* option, const char* value) const {
  MOZ_ASSERT(mPrinterInfo);
  return mShim.cupsCheckDestSupported(CUPS_HTTP_DEFAULT, mPrinter, mPrinterInfo,
                                      option, value);
}

nsTArray<PaperInfo> nsPrinterCUPS::PaperList() const {
  if (!mPrinterInfo) {
    return {};
  }

  const int paperCount = mShim.cupsGetDestMediaCount(
      CUPS_HTTP_DEFAULT, mPrinter, mPrinterInfo, CUPS_MEDIA_FLAGS_DEFAULT);

  // blocking call
  http_t* connection = mShim.cupsConnectDest(mPrinter, CUPS_DEST_FLAGS_NONE,
                                             /* timeout(ms) */ 5000,
                                             /* cancel */ nullptr,
                                             /* resource */ nullptr,
                                             /* resourcesize */ 0,
                                             /* callback */ nullptr,
                                             /* user_data */ nullptr);

  if (!connection) {
    return {};
  }

  nsTArray<PaperInfo> paperList;
  for (int i = 0; i < paperCount; ++i) {
    cups_size_t info;
    int getInfoSucceded =
        mShim.cupsGetDestMediaByIndex(CUPS_HTTP_DEFAULT, mPrinter, mPrinterInfo,
                                      i, CUPS_MEDIA_FLAGS_DEFAULT, &info);

    if (!getInfoSucceded) {
      continue;
    }

    // localizedName is owned by mPrinterInfo.
    // https://www.cups.org/doc/cupspm.html#cupsLocalizeDestMedia
    const char* localizedName = mShim.cupsLocalizeDestMedia(
        connection, mPrinter, mPrinterInfo, CUPS_MEDIA_FLAGS_DEFAULT, &info);

    if (!localizedName) {
      continue;
    }

    // XXX Do we actually have the guarantee that this is utf-8?
    NS_ConvertUTF8toUTF16 name(localizedName);
    const double kPointsPerHundredthMillimeter = 0.0283465;

    paperList.AppendElement(PaperInfo{
        std::move(name),
        {info.width * kPointsPerHundredthMillimeter,
         info.length * kPointsPerHundredthMillimeter},
        Some(MarginDouble{info.top * kPointsPerHundredthMillimeter,
                          info.right * kPointsPerHundredthMillimeter,
                          info.bottom * kPointsPerHundredthMillimeter,
                          info.left * kPointsPerHundredthMillimeter}),
    });
  }

  mShim.httpClose(connection);
  return paperList;
}
