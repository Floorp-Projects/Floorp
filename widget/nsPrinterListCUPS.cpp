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

NS_IMETHODIMP
nsPrinterListCUPS::GetPrinters(nsTArray<RefPtr<nsIPrinter>>& aPrinters) {
  if (!sCupsShim.EnsureInitialized()) {
    return NS_ERROR_FAILURE;
  }

  cups_dest_t* printers = nullptr;
  auto numPrinters = sCupsShim.cupsGetDests(&printers);
  aPrinters.SetCapacity(numPrinters);

  for (auto i : mozilla::IntegerRange(0, numPrinters)) {
    cups_dest_t* dest = printers + i;

    nsString displayName;
    GetDisplayNameForPrinter(*dest, displayName);
    RefPtr<nsPrinterCUPS> cupsPrinter =
        nsPrinterCUPS::Create(sCupsShim, dest, displayName);

    aPrinters.AppendElement(cupsPrinter);
  }

  sCupsShim.cupsFreeDests(numPrinters, printers);
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterListCUPS::GetSystemDefaultPrinterName(nsAString& aName) {
  if (!sCupsShim.EnsureInitialized()) {
    return NS_ERROR_FAILURE;
  }

  // Passing in nullptr for the name will return the default, if any.
  const cups_dest_t* dest =
      sCupsShim.cupsGetNamedDest(CUPS_HTTP_DEFAULT, /* name */ nullptr,
                                 /* instance */ nullptr);
  if (!dest) {
    return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE;
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
