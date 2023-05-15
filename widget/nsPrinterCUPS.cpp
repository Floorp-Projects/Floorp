/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterCUPS.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/GkRustUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_print.h"
#include "nsTHashtable.h"
#include "nsPaper.h"
#include "nsPrinterBase.h"
#include "nsPrintSettingsImpl.h"

using namespace mozilla;
using MarginDouble = mozilla::gfx::MarginDouble;

// Requested attributes for IPP requests, just the CUPS version now.
static constexpr Array<const char* const, 1> requestedAttributes{
    "cups-version"};

static constexpr double kPointsPerHundredthMillimeter = 72.0 / 2540.0;

static PaperInfo MakePaperInfo(const nsAString& aName,
                               const cups_size_t& aMedia) {
  // XXX Do we actually have the guarantee that this is utf-8?
  NS_ConvertUTF8toUTF16 paperId(aMedia.media);  // internal paper name/ID
  return PaperInfo(
      paperId, aName,
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
  PrinterInfoLock lock = mPrinterInfoMutex.Lock();
  if (lock->mPrinterInfo) {
    mShim.cupsFreeDestInfo(lock->mPrinterInfo);
  }
  if (lock->mPrinter) {
    mShim.cupsFreeDests(1, lock->mPrinter);
  }
}

NS_IMETHODIMP
nsPrinterCUPS::GetName(nsAString& aName) {
  GetPrinterName(aName);
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterCUPS::GetSystemName(nsAString& aName) {
  PrinterInfoLock lock = mPrinterInfoMutex.Lock();
  CopyUTF8toUTF16(MakeStringSpan(lock->mPrinter->name), aName);
  return NS_OK;
}

void nsPrinterCUPS::GetPrinterName(nsAString& aName) const {
  if (mDisplayName.IsEmpty()) {
    aName.Truncate();
    PrinterInfoLock lock = mPrinterInfoMutex.Lock();
    CopyUTF8toUTF16(MakeStringSpan(lock->mPrinter->name), aName);
  } else {
    aName = mDisplayName;
  }
}

const char* nsPrinterCUPS::LocalizeMediaName(http_t& aConnection,
                                             cups_size_t& aMedia) const {
  // The returned string is owned by mPrinterInfo.
  // https://www.cups.org/doc/cupspm.html#cupsLocalizeDestMedia
  if (!mShim.cupsLocalizeDestMedia) {
    return aMedia.media;
  }
  PrinterInfoLock lock = TryEnsurePrinterInfo();
  return mShim.cupsLocalizeDestMedia(&aConnection, lock->mPrinter,
                                     lock->mPrinterInfo,
                                     CUPS_MEDIA_FLAGS_DEFAULT, &aMedia);
}

bool nsPrinterCUPS::SupportsDuplex() const {
  return Supports(CUPS_SIDES, CUPS_SIDES_TWO_SIDED_PORTRAIT);
}

bool nsPrinterCUPS::SupportsMonochrome() const {
  if (!SupportsColor()) {
    return true;
  }
  return StaticPrefs::print_cups_monochrome_enabled();
}

bool nsPrinterCUPS::SupportsColor() const {
  // CUPS 2.1 (particularly as used in Ubuntu 16) is known to have inaccurate
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
  PrinterInfoLock lock = mPrinterInfoMutex.Lock();
  const char* const value = FindCUPSOption(lock, "printer-type");
  if (!value) {
    return false;
  }
  // If the value is non-numeric, then atoi will return 0, which will still
  // cause this function to return false.
  const int type = atoi(value);
  return type & CUPS_PRINTER_COLLATE;
}

nsPrinterBase::PrinterInfo nsPrinterCUPS::CreatePrinterInfo() const {
  Connection connection{mShim};
  return PrinterInfo{PaperList(connection), DefaultSettings(connection)};
}

bool nsPrinterCUPS::Supports(const char* aOption, const char* aValue) const {
  PrinterInfoLock lock = TryEnsurePrinterInfo();
  return mShim.cupsCheckDestSupported(CUPS_HTTP_DEFAULT, lock->mPrinter,
                                      lock->mPrinterInfo, aOption, aValue);
}

bool nsPrinterCUPS::IsCUPSVersionAtLeast(uint64_t aCUPSMajor,
                                         uint64_t aCUPSMinor,
                                         uint64_t aCUPSPatch) const {
  PrinterInfoLock lock = TryEnsurePrinterInfo();
  // Compare major version.
  if (lock->mCUPSMajor > aCUPSMajor) {
    return true;
  }
  if (lock->mCUPSMajor < aCUPSMajor) {
    return false;
  }

  // Compare minor version.
  if (lock->mCUPSMinor > aCUPSMinor) {
    return true;
  }
  if (lock->mCUPSMinor < aCUPSMinor) {
    return false;
  }

  // Compare patch.
  return aCUPSPatch <= lock->mCUPSPatch;
}

http_t* nsPrinterCUPS::Connection::GetConnection(cups_dest_t* aDest) {
  if (mWasInited) {
    return mConnection;
  }
  mWasInited = true;

  // blocking call
  http_t* const connection = mShim.cupsConnectDest(aDest, CUPS_DEST_FLAGS_NONE,
                                                   /* timeout(ms) */ 5000,
                                                   /* cancel */ nullptr,
                                                   /* resource */ nullptr,
                                                   /* resourcesize */ 0,
                                                   /* callback */ nullptr,
                                                   /* user_data */ nullptr);
  if (connection) {
    mConnection = connection;
  }
  return mConnection;
}

nsPrinterCUPS::Connection::~Connection() {
  if (mWasInited && mConnection) {
    mShim.httpClose(mConnection);
  }
}

PrintSettingsInitializer nsPrinterCUPS::DefaultSettings(
    Connection& aConnection) const {
  nsString printerName;
  GetPrinterName(printerName);
  PrinterInfoLock lock = TryEnsurePrinterInfo();

  cups_size_t media;

  bool hasDefaultMedia = false;
// cupsGetDestMediaDefault appears to return more accurate defaults on macOS,
// and the IPP attribute appears to return more accurate defaults on Linux.
#ifdef XP_MACOSX
  hasDefaultMedia = mShim.cupsGetDestMediaDefault(
      CUPS_HTTP_DEFAULT, lock->mPrinter, lock->mPrinterInfo,
      CUPS_MEDIA_FLAGS_DEFAULT, &media);
#else
  {
    ipp_attribute_t* defaultMediaIPP =
        mShim.cupsFindDestDefault
            ? mShim.cupsFindDestDefault(CUPS_HTTP_DEFAULT, lock->mPrinter,
                                        lock->mPrinterInfo, "media")
            : nullptr;

    const char* defaultMediaName =
        defaultMediaIPP ? mShim.ippGetString(defaultMediaIPP, 0, nullptr)
                        : nullptr;

    hasDefaultMedia = defaultMediaName &&
                      mShim.cupsGetDestMediaByName(
                          CUPS_HTTP_DEFAULT, lock->mPrinter, lock->mPrinterInfo,
                          defaultMediaName, CUPS_MEDIA_FLAGS_DEFAULT, &media);
  }
#endif

  if (!hasDefaultMedia) {
    return PrintSettingsInitializer{
        std::move(printerName),
        PaperInfo(),
        SupportsColor(),
    };
  }

  // Check if this is a localized fallback paper size, in which case we can
  // avoid using the CUPS localization methods.
  const gfx::SizeDouble sizeDouble{
      media.width * kPointsPerHundredthMillimeter,
      media.length * kPointsPerHundredthMillimeter};
  if (const PaperInfo* const paperInfo = FindCommonPaperSize(sizeDouble)) {
    return PrintSettingsInitializer{
        std::move(printerName),
        MakePaperInfo(paperInfo->mName, media),
        SupportsColor(),
    };
  }

  http_t* const connection = aConnection.GetConnection(lock->mPrinter);
  // XXX Do we actually have the guarantee that this is utf-8?
  NS_ConvertUTF8toUTF16 localizedName{
      connection ? LocalizeMediaName(*connection, media) : ""};

  return PrintSettingsInitializer{
      std::move(printerName),
      MakePaperInfo(localizedName, media),
      SupportsColor(),
  };
}

nsTArray<mozilla::PaperInfo> nsPrinterCUPS::PaperList(
    Connection& aConnection) const {
  PrinterInfoLock lock = mPrinterInfoMutex.Lock();
  http_t* const connection = aConnection.GetConnection(lock->mPrinter);
  TryEnsurePrinterInfo(lock, connection);

  if (!lock->mPrinterInfo) {
    return {};
  }

  const int paperCount = mShim.cupsGetDestMediaCount
                             ? mShim.cupsGetDestMediaCount(
                                   connection, lock->mPrinter,
                                   lock->mPrinterInfo, CUPS_MEDIA_FLAGS_DEFAULT)
                             : 0;
  nsTArray<PaperInfo> paperList;
  nsTHashtable<nsCharPtrHashKey> paperSet(std::max(paperCount, 0));

  paperList.SetCapacity(paperCount);
  for (int i = 0; i < paperCount; ++i) {
    cups_size_t media;
    const int getInfoSucceeded = mShim.cupsGetDestMediaByIndex(
        connection, lock->mPrinter, lock->mPrinterInfo, i,
        CUPS_MEDIA_FLAGS_DEFAULT, &media);

    if (!getInfoSucceeded || !paperSet.EnsureInserted(media.media)) {
      continue;
    }
    // Check if this is a PWG paper size, in which case we can avoid using the
    // CUPS localization methods.
    const gfx::SizeDouble sizeDouble{
        media.width * kPointsPerHundredthMillimeter,
        media.length * kPointsPerHundredthMillimeter};
    if (const PaperInfo* const paperInfo = FindCommonPaperSize(sizeDouble)) {
      paperList.AppendElement(MakePaperInfo(paperInfo->mName, media));
    } else {
      const char* const mediaName =
          connection ? LocalizeMediaName(*connection, media) : media.media;
      paperList.AppendElement(
          MakePaperInfo(NS_ConvertUTF8toUTF16(mediaName), media));
    }
  }

  return paperList;
}

void nsPrinterCUPS::TryEnsurePrinterInfo(PrinterInfoLock& aLock,
                                         http_t* const aConnection) const {
  if (aLock->mPrinterInfo ||
      (aConnection == CUPS_HTTP_DEFAULT ? aLock->mTriedInitWithDefault
                                        : aLock->mTriedInitWithConnection)) {
    return;
  }

  if (aConnection == CUPS_HTTP_DEFAULT) {
    aLock->mTriedInitWithDefault = true;
  } else {
    aLock->mTriedInitWithConnection = true;
  }

  MOZ_ASSERT(aLock->mPrinter);

  // httpGetAddress was only added in CUPS 2.0, and some systems still use
  // CUPS 1.7.
  if (aConnection && MOZ_LIKELY(mShim.httpGetAddress && mShim.httpAddrPort)) {
    // This is a workaround for the CUPS Bug seen in bug 1691347.
    // This is to avoid a null string being passed to strstr in CUPS. The path
    // in CUPS that leads to this is as follows:
    //
    // In cupsCopyDestInfo, CUPS_DEST_FLAG_DEVICE is set when the connection is
    // not null (same as CUPS_HTTP_DEFAULT), the print server is not the same
    // as our hostname and is not path-based (starts with a '/'), or the IPP
    // port is different than the global server IPP port.
    //
    // https://github.com/apple/cups/blob/c9da6f63b263faef5d50592fe8cf8056e0a58aa2/cups/dest-options.c#L718-L722
    //
    // In _cupsGetDestResource, CUPS fetches the IPP options "device-uri" and
    // "printer-uri-supported". Note that IPP options are returned as null when
    // missing.
    //
    // https://github.com/apple/cups/blob/23c45db76a8520fd6c3b1d9164dbe312f1ab1481/cups/dest.c#L1138-L1141
    //
    // If the CUPS_DEST_FLAG_DEVICE is set or the "printer-uri-supported"
    // option is not set, CUPS checks for "._tcp" in the "device-uri" option
    // without doing a NULL-check first.
    //
    // https://github.com/apple/cups/blob/23c45db76a8520fd6c3b1d9164dbe312f1ab1481/cups/dest.c#L1144
    //
    // If we find that those branches will be taken, don't actually fetch the
    // CUPS data and instead just return an empty printer info.

    const char* const serverNameBytes = mShim.cupsServer();

    if (MOZ_LIKELY(serverNameBytes)) {
      const nsDependentCString serverName{serverNameBytes};

      // We only need enough characters to determine equality with serverName.
      // + 2 because we need one byte for the null-character, and we also want
      // to get more characters of the host name than the server name if
      // possible. Otherwise, if the hostname starts with the same text as the
      // entire server name, it would compare equal when it's not.
      const size_t hostnameMemLength = serverName.Length() + 2;
      auto hostnameMem = MakeUnique<char[]>(hostnameMemLength);

      // We don't expect httpGetHostname to return null when a connection is
      // passed, but it's better not to make assumptions.
      const char* const hostnameBytes = mShim.httpGetHostname(
          aConnection, hostnameMem.get(), hostnameMemLength);

      if (MOZ_LIKELY(hostnameBytes)) {
        const nsDependentCString hostname{hostnameBytes};

        // Attempt to match the condional at
        // https://github.com/apple/cups/blob/c9da6f63b263faef5d50592fe8cf8056e0a58aa2/cups/dest-options.c#L718
        //
        // To find the result of the comparison CUPS performs of
        // `strcmp(http->hostname, cg->server)`, we use httpGetHostname to try
        // to get the value of `http->hostname`, but this isn't quite the same.
        // For local addresses, httpGetHostName will normalize the result to be
        // localhost", rather than the actual value of `http->hostname`.
        //
        // https://github.com/apple/cups/blob/2201569857f225c9874bfae19713ffb2f4bdfdeb/cups/http-addr.c#L794-L818
        //
        // Because of this, if both serverName and hostname equal "localhost",
        // then the actual hostname might be a different local address that CUPS
        // normalized in httpGetHostName, and `http->hostname` won't be equal to
        // `cg->server` in CUPS.
        const bool namesMightNotMatch =
            hostname != serverName || hostname == "localhost";
        const bool portsDiffer =
            mShim.httpAddrPort(mShim.httpGetAddress(aConnection)) !=
            mShim.ippPort();
        const bool cupsDestDeviceFlag =
            (namesMightNotMatch && serverName[0] != '/') || portsDiffer;

        // Match the conditional at
        // https://github.com/apple/cups/blob/23c45db76a8520fd6c3b1d9164dbe312f1ab1481/cups/dest.c#L1144
        // but if device-uri is null do not call into CUPS.
        if ((cupsDestDeviceFlag ||
             !FindCUPSOption(aLock, "printer-uri-supported")) &&
            !FindCUPSOption(aLock, "device-uri")) {
          return;
        }
      }
    }
  }

  // All CUPS calls that take the printer info do null-checks internally, so we
  // can fetch this info and only worry about the result of the later CUPS
  // functions.
  aLock->mPrinterInfo = mShim.cupsCopyDestInfo(aConnection, aLock->mPrinter);

  // Even if we failed to fetch printer info, it is still possible we can talk
  // to the print server and get its CUPS version.
  FetchCUPSVersionForPrinter(mShim, aLock->mPrinter, aLock->mCUPSMajor,
                             aLock->mCUPSMinor, aLock->mCUPSPatch);
}

void nsPrinterCUPS::ForEachExtraMonochromeSetting(
    FunctionRef<void(const nsACString&, const nsACString&)> aCallback) {
  nsAutoCString pref;
  Preferences::GetCString("print.cups.monochrome.extra_settings", pref);
  if (pref.IsEmpty()) {
    return;
  }

  for (const auto& pair : pref.Split(',')) {
    auto splitter = pair.Split(':');
    auto end = splitter.end();

    auto key = splitter.begin();
    if (key == end) {
      continue;
    }

    auto value = ++splitter.begin();
    if (value == end) {
      continue;
    }

    aCallback(*key, *value);
  }
}
