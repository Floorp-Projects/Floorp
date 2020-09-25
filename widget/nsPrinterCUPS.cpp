/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterCUPS.h"

#include "mozilla/GkRustUtils.h"
#include "nsPaper.h"
#include "nsPrinterBase.h"
#include "nsPrintSettingsImpl.h"
#include "plstr.h"

using namespace mozilla;

// Requested attributes for IPP requests, just the CUPS version now.
static constexpr Array<const char* const, 1> requestedAttributes{
    "cups-version"};

static PaperInfo MakePaperInfo(const char* aName, const cups_size_t& aMedia) {
  const double kPointsPerHundredthMillimeter = 72.0 / 2540.0;
  // XXX Do we actually have the guarantee that these are utf-8?
  NS_ConvertUTF8toUTF16 paperId(aMedia.media);  // internal paper name/ID
  NS_ConvertUTF8toUTF16 paperName(aName);       // localized human-friendly name
  return PaperInfo(
      paperId, paperName,
      {aMedia.width * kPointsPerHundredthMillimeter,
       aMedia.length * kPointsPerHundredthMillimeter},
      Some(gfx::MarginDouble{aMedia.top * kPointsPerHundredthMillimeter,
                             aMedia.right * kPointsPerHundredthMillimeter,
                             aMedia.bottom * kPointsPerHundredthMillimeter,
                             aMedia.left * kPointsPerHundredthMillimeter}));
}

// Fetches the CUPS version for the print server controlling the printer. This
// will only modify the output arguments if the fetch succeeds.
static void FetchCUPSVersionForPrinter(const nsCUPSShim& aShim,
                                       const cups_dest_t* const aDest,
                                       uint64_t& aOutMajor, uint64_t& aOutMinor,
                                       uint64_t& aOutPatch) {
  // Make an IPP request to the server for the printer.
  const char* const uri = aShim.cupsGetOption(
      "printer-uri-supported", aDest->num_options, aDest->options);
  if (!uri) {
    return;
  }

  ipp_t* const ippRequest = aShim.ippNewRequest(IPP_OP_GET_PRINTER_ATTRIBUTES);

  // Set the URI we want to use.
  aShim.ippAddString(ippRequest, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri",
                     nullptr, uri);

  // Set the attributes to request.
  aShim.ippAddStrings(ippRequest, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                      "requested-attributes", requestedAttributes.Length,
                      nullptr, &(requestedAttributes[0]));

  // Use the default HTTP connection to query the CUPS server itself to get
  // the CUPS version.
  // Note that cupsDoRequest will delete the request whether it succeeds or
  // fails, so we should not use ippDelete on it.
  if (ipp_t* const ippResponse =
          aShim.cupsDoRequest(CUPS_HTTP_DEFAULT, ippRequest, "/")) {
    ipp_attribute_t* const versionAttrib =
        aShim.ippFindAttribute(ippResponse, "cups-version", IPP_TAG_TEXT);
    if (versionAttrib && aShim.ippGetCount(versionAttrib) == 1) {
      const char* versionString = aShim.ippGetString(versionAttrib, 0, nullptr);
      MOZ_ASSERT(versionString);
      // On error, GkRustUtils::ParseSemVer will not modify its arguments.
      GkRustUtils::ParseSemVer(
          nsDependentCSubstring{MakeStringSpan(versionString)}, aOutMajor,
          aOutMinor, aOutPatch);
    }
    aShim.ippDelete(ippResponse);
  }
}

nsPrinterCUPS::~nsPrinterCUPS() {
  auto printerInfoLock = mPrinterInfoMutex.Lock();
  if (printerInfoLock->mPrinterInfo) {
    mShim.cupsFreeDestInfo(printerInfoLock->mPrinterInfo);
  }
  if (mPrinter) {
    mShim.cupsFreeDests(1, mPrinter);
    mPrinter = nullptr;
  }
}

PrintSettingsInitializer nsPrinterCUPS::DefaultSettings() const {
  nsString printerName;
  GetPrinterName(printerName);
  auto printerInfoLock = mPrinterInfoMutex.Lock();
  EnsurePrinterInfo(*printerInfoLock);
  cups_dinfo_t* const printerInfo = printerInfoLock->mPrinterInfo;

  cups_size_t media;

// cupsGetDestMediaDefault appears to return more accurate defaults on macOS,
// and the IPP attribute appears to return more accurate defaults on Linux.
#ifdef XP_MACOSX
  bool hasDefaultMedia =
      mShim.cupsGetDestMediaDefault(CUPS_HTTP_DEFAULT, mPrinter, printerInfo,
                                    CUPS_MEDIA_FLAGS_DEFAULT, &media);
#else
  ipp_attribute_t* defaultMediaIPP = mShim.cupsFindDestDefault(
      CUPS_HTTP_DEFAULT, mPrinter, printerInfo, "media");

  const char* defaultMediaName =
      mShim.ippGetString(defaultMediaIPP, 0, nullptr);

  bool hasDefaultMedia = mShim.cupsGetDestMediaByName(
      CUPS_HTTP_DEFAULT, mPrinter, printerInfo, defaultMediaName,
      CUPS_MEDIA_FLAGS_DEFAULT, &media);
#endif

  if (!hasDefaultMedia) {
    return PrintSettingsInitializer{
        std::move(printerName),
        PaperInfo(),
        SupportsColor(),
    };
  }

  const char* localizedName = nullptr;

  // blocking call
  http_t* connection = mShim.cupsConnectDest(mPrinter, CUPS_DEST_FLAGS_NONE,
                                             /* timeout(ms) */ 5000,
                                             /* cancel */ nullptr,
                                             /* resource */ nullptr,
                                             /* resourcesize */ 0,
                                             /* callback */ nullptr,
                                             /* user_data */ nullptr);

  if (connection) {
    localizedName = LocalizeMediaName(*connection, media);
    mShim.httpClose(connection);
  }

  return PrintSettingsInitializer{
      std::move(printerName),
      MakePaperInfo(localizedName, media),
      SupportsColor(),
  };
}

NS_IMETHODIMP
nsPrinterCUPS::GetName(nsAString& aName) {
  GetPrinterName(aName);
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterCUPS::GetSystemName(nsAString& aName) {
  CopyUTF8toUTF16(MakeStringSpan(mPrinter->name), aName);
  return NS_OK;
}

void nsPrinterCUPS::GetPrinterName(nsAString& aName) const {
  if (mDisplayName.IsEmpty()) {
    aName.Truncate();
    CopyUTF8toUTF16(MakeStringSpan(mPrinter->name), aName);
  } else {
    aName = mDisplayName;
  }
}

const char* nsPrinterCUPS::LocalizeMediaName(http_t& aConnection,
                                             cups_size_t& aMedia) const {
  // The returned string is owned by mPrinterInfo.
  // https://www.cups.org/doc/cupspm.html#cupsLocalizeDestMedia
  auto printerInfoLock = mPrinterInfoMutex.Lock();
  EnsurePrinterInfo(*printerInfoLock);
  cups_dinfo_t* const printerInfo = printerInfoLock->mPrinterInfo;
  return mShim.cupsLocalizeDestMedia(&aConnection, mPrinter, printerInfo,
                                     CUPS_MEDIA_FLAGS_DEFAULT, &aMedia);
}

bool nsPrinterCUPS::SupportsDuplex() const {
  return Supports(CUPS_SIDES, CUPS_SIDES_TWO_SIDED_PORTRAIT);
}

bool nsPrinterCUPS::SupportsMonochrome() const {
#ifdef XP_MACOSX
  // On Cocoa/CorePrinting there's no driver-independent API to switch a printer
  // from color to monochrome printing, so we can't claim to support it if we
  // support color.
  return !SupportsColor();
#else
  return true;
#endif
}

bool nsPrinterCUPS::SupportsColor() const {
  // CUPS 2.1 (particularly as used in Ubuntu 16) is known to have innaccurate
  // results for CUPS_PRINT_COLOR_MODE.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1660658#c15
  if (!IsCUPSVersionAtLeast(2, 2, 0)) {
    return true;  // See comment for PrintSettingsInitializer.mPrintInColor
  }
  return Supports(CUPS_PRINT_COLOR_MODE, CUPS_PRINT_COLOR_MODE_AUTO) ||
         Supports(CUPS_PRINT_COLOR_MODE, CUPS_PRINT_COLOR_MODE_COLOR) ||
         !Supports(CUPS_PRINT_COLOR_MODE, CUPS_PRINT_COLOR_MODE_MONOCHROME);
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

bool nsPrinterCUPS::Supports(const char* aOption, const char* aValue) const {
  auto printerInfoLock = mPrinterInfoMutex.Lock();
  EnsurePrinterInfo(*printerInfoLock);
  cups_dinfo_t* const printerInfo = printerInfoLock->mPrinterInfo;
  return mShim.cupsCheckDestSupported(CUPS_HTTP_DEFAULT, mPrinter, printerInfo,
                                      aOption, aValue);
}

bool nsPrinterCUPS::IsCUPSVersionAtLeast(uint64_t aCUPSMajor,
                                         uint64_t aCUPSMinor,
                                         uint64_t aCUPSPatch) const {
  auto printerInfoLock = mPrinterInfoMutex.Lock();
  EnsurePrinterInfo(*printerInfoLock);
  // Compare major version.
  if (printerInfoLock->mCUPSMajor > aCUPSMajor) {
    return true;
  }
  if (printerInfoLock->mCUPSMajor < aCUPSMajor) {
    return false;
  }

  // Compare minor version.
  if (printerInfoLock->mCUPSMinor > aCUPSMinor) {
    return true;
  }
  if (printerInfoLock->mCUPSMinor < aCUPSMinor) {
    return false;
  }

  // Compare patch.
  return aCUPSPatch <= printerInfoLock->mCUPSPatch;
}

nsTArray<PaperInfo> nsPrinterCUPS::PaperList() const {
  auto printerInfoLock = mPrinterInfoMutex.Lock();
  EnsurePrinterInfo(*printerInfoLock);
  cups_dinfo_t* const printerInfo = printerInfoLock->mPrinterInfo;
  if (!printerInfo) {
    return {};
  }

  const int paperCount = mShim.cupsGetDestMediaCount(
      CUPS_HTTP_DEFAULT, mPrinter, printerInfo, CUPS_MEDIA_FLAGS_DEFAULT);

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
    cups_size_t media;
    int getInfoSucceded =
        mShim.cupsGetDestMediaByIndex(CUPS_HTTP_DEFAULT, mPrinter, printerInfo,
                                      i, CUPS_MEDIA_FLAGS_DEFAULT, &media);

    if (!getInfoSucceded) {
      continue;
    }

    paperList.AppendElement(
        MakePaperInfo(LocalizeMediaName(*connection, media), media));
  }

  mShim.httpClose(connection);
  return paperList;
}

void nsPrinterCUPS::EnsurePrinterInfo(
    CUPSPrinterInfo& aInOutPrinterInfo) const {
  if (aInOutPrinterInfo.mWasInited) {
    return;
  }

  aInOutPrinterInfo.mWasInited = true;

  // All CUPS calls that take the printer info do null-checks internally, so we
  // can fetch this info and only worry about the result of the later CUPS
  // functions.
  aInOutPrinterInfo.mPrinterInfo =
      mShim.cupsCopyDestInfo(CUPS_HTTP_DEFAULT, mPrinter);

  // Even if we failed to fetch printer info, it is still possible we can talk
  // to the print server and get its CUPS version.
  FetchCUPSVersionForPrinter(mShim, mPrinter, aInOutPrinterInfo.mCUPSMajor,
                             aInOutPrinterInfo.mCUPSMinor,
                             aInOutPrinterInfo.mCUPSPatch);
}
