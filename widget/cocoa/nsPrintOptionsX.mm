/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsPrintOptionsX.h"
#include "nsPrintSettingsX.h"

nsPrintOptionsX::nsPrintOptionsX()
{
}

nsPrintOptionsX::~nsPrintOptionsX()
{
}

nsresult
nsPrintOptionsX::ReadPrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName, uint32_t aFlags)
{
  nsresult rv;
  
  rv = nsPrintOptions::ReadPrefs(aPS, aPrinterName, aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintOptions::ReadPrefs() failed");
  
  nsRefPtr<nsPrintSettingsX> printSettingsX(do_QueryObject(aPS));
  if (!printSettingsX)
    return NS_ERROR_NO_INTERFACE;
  rv = printSettingsX->ReadPageFormatFromPrefs();
  
  return NS_OK;
}

nsresult nsPrintOptionsX::_CreatePrintSettings(nsIPrintSettings **_retval)
{
  nsresult rv;
  *_retval = nullptr;

  nsPrintSettingsX* printSettings = new nsPrintSettingsX; // does not initially ref count
  NS_ENSURE_TRUE(printSettings, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*_retval = printSettings);

  rv = printSettings->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(*_retval);
    return rv;
  }

  InitPrintSettingsFromPrefs(*_retval, false, nsIPrintSettings::kInitSaveAll);
  return rv;
}

nsresult
nsPrintOptionsX::WritePrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName, uint32_t aFlags)
{
  nsresult rv;

  rv = nsPrintOptions::WritePrefs(aPS, aPrinterName, aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintOptions::WritePrefs() failed");

  nsRefPtr<nsPrintSettingsX> printSettingsX(do_QueryObject(aPS));
  if (!printSettingsX)
    return NS_ERROR_NO_INTERFACE;
  rv = printSettingsX->WritePageFormatToPrefs();
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintSettingsX::WritePageFormatToPrefs() failed");

  return NS_OK;
}
