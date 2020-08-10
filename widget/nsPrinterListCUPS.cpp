/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterListCUPS.h"

#include "mozilla/IntegerRange.h"
#include "nsCUPSShim.h"
#include "nsPrinterCUPS.h"
#include "nsString.h"
#include "prenv.h"

static nsCUPSShim sCupsShim;
using PrinterInfo = nsPrinterListBase::PrinterInfo;

/**
 * Retrieves a human-readable name for the printer from CUPS.
 * https://www.cups.org/doc/cupspm.html#basic-destination-information
 */
static void GetDisplayNameForPrinter(const cups_dest_t& aDest,
                                     nsAString& aName) {
// macOS clients expect prettified printer names
// while GTK clients expect non-prettified names.
#ifdef XP_MACOSX
  const char* displayName =
      sCupsShim.cupsGetOption("printer-info", aDest.num_options, aDest.options);
  if (displayName) {
    CopyUTF8toUTF16(MakeStringSpan(displayName), aName);
  }
#endif
}

nsTArray<PrinterInfo> nsPrinterListCUPS::Printers() const {
  if (!sCupsShim.EnsureInitialized()) {
    return {};
  }

  nsTArray<PrinterInfo> printerInfoList;

  cups_dest_t* printers = nullptr;
  auto numPrinters = sCupsShim.cupsGetDests(&printers);
  printerInfoList.SetCapacity(numPrinters);

  for (auto i : mozilla::IntegerRange(0, numPrinters)) {
    cups_dest_t* dest = printers + i;

    cups_dest_t* ownedDest = nullptr;
    mozilla::DebugOnly<const int> numCopied =
        sCupsShim.cupsCopyDest(dest, 0, &ownedDest);
    MOZ_ASSERT(numCopied == 1);

    cups_dinfo_t* ownedInfo =
        sCupsShim.cupsCopyDestInfo(CUPS_HTTP_DEFAULT, ownedDest);

    nsString name;
    GetDisplayNameForPrinter(*dest, name);

    printerInfoList.AppendElement(
        PrinterInfo{std::move(name), {ownedDest, ownedInfo}});
  }

  sCupsShim.cupsFreeDests(numPrinters, printers);
  return printerInfoList;
}

RefPtr<nsIPrinter> nsPrinterListCUPS::CreatePrinter(PrinterInfo aInfo) const {
  return mozilla::MakeRefPtr<nsPrinterCUPS>(
      sCupsShim, std::move(aInfo.mName),
      static_cast<cups_dest_t*>(aInfo.mCupsHandles[0]),
      static_cast<cups_dinfo_t*>(aInfo.mCupsHandles[1]));
}

NS_IMETHODIMP
nsPrinterListCUPS::GetSystemDefaultPrinterName(nsAString& aName) {
  aName.Truncate();

  if (!sCupsShim.EnsureInitialized()) {
    return NS_ERROR_FAILURE;
  }

  // Passing in nullptr for the name will return the default, if any.
  const cups_dest_t* dest =
      sCupsShim.cupsGetNamedDest(CUPS_HTTP_DEFAULT, /* name */ nullptr,
                                 /* instance */ nullptr);
  if (!dest) {
    return NS_OK;
  }

  GetDisplayNameForPrinter(*dest, aName);
  if (aName.IsEmpty()) {
    CopyUTF8toUTF16(mozilla::MakeStringSpan(dest->name), aName);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrinterListCUPS::InitPrintSettingsFromPrinter(
    const nsAString& aPrinterName, nsIPrintSettings* aPrintSettings) {
  MOZ_ASSERT(aPrintSettings);

  // Set a default file name.
  nsAutoString filename;
  nsresult rv = aPrintSettings->GetToFileName(filename);
  if (NS_FAILED(rv) || filename.IsEmpty()) {
    const char* path = PR_GetEnv("PWD");
    if (!path) {
      path = PR_GetEnv("HOME");
    }

    if (path) {
      CopyUTF8toUTF16(mozilla::MakeStringSpan(path), filename);
      filename.AppendLiteral("/mozilla.pdf");
    } else {
      filename.AssignLiteral("mozilla.pdf");
    }

    aPrintSettings->SetToFileName(filename);
  }

  aPrintSettings->SetIsInitializedFromPrinter(true);
  return NS_OK;
}
