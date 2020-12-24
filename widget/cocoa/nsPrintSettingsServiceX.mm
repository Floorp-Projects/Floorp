/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsServiceX.h"

#include "mozilla/Debug.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsQueryObject.h"
#include "nsPrintSettingsX.h"
#include "nsCocoaUtils.h"

using namespace mozilla::embedding;

NS_IMETHODIMP
nsPrintSettingsServiceX::SerializeToPrintData(nsIPrintSettings* aSettings, PrintData* data) {
  nsresult rv = nsPrintSettingsService::SerializeToPrintData(aSettings, data);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<nsPrintSettingsX> settingsX(do_QueryObject(aSettings));
  if (NS_WARN_IF(!settingsX)) {
    return NS_ERROR_FAILURE;
  }

  settingsX->GetDisposition(data->disposition());
  settingsX->GetDestination(&data->destination());

  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsServiceX::DeserializeToPrintSettings(const PrintData& data,
                                                    nsIPrintSettings* settings) {
  nsresult rv = nsPrintSettingsService::DeserializeToPrintSettings(data, settings);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<nsPrintSettingsX> settingsX(do_QueryObject(settings));
  if (NS_WARN_IF(!settingsX)) {
    return NS_ERROR_FAILURE;
  }

  settingsX->SetDisposition(data.disposition());
  settingsX->SetDestination(data.destination());

  return NS_OK;
}

nsresult nsPrintSettingsServiceX::ReadPrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName,
                                            uint32_t aFlags) {
  mozilla::DebugOnly<nsresult> rv = nsPrintSettingsService::ReadPrefs(aPS, aPrinterName, aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintSettingsService::ReadPrefs() failed");

  RefPtr<nsPrintSettingsX> printSettingsX(do_QueryObject(aPS));
  if (!printSettingsX) {
    return NS_ERROR_NO_INTERFACE;
  }

  // ReadPageFormatFromPrefs may fail (e.g. prefs are missing/broken) but we can
  // safely ignore that and just leave existing/default values in the settings.
  mozilla::Unused << printSettingsX->ReadPageFormatFromPrefs();

  return NS_OK;
}

nsresult nsPrintSettingsServiceX::_CreatePrintSettings(nsIPrintSettings** _retval) {
  nsresult rv;
  *_retval = nullptr;

  nsPrintSettingsX* printSettings = new nsPrintSettingsX;  // does not initially ref count
  NS_ENSURE_TRUE(printSettings, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*_retval = printSettings);

  rv = printSettings->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(*_retval);
    return rv;
  }

  auto globalPrintSettings =
      nsIPrintSettings::kInitSaveShrinkToFit | nsIPrintSettings::kInitSaveHeaderLeft |
      nsIPrintSettings::kInitSaveHeaderCenter | nsIPrintSettings::kInitSaveHeaderRight |
      nsIPrintSettings::kInitSaveFooterLeft | nsIPrintSettings::kInitSaveFooterCenter |
      nsIPrintSettings::kInitSaveFooterRight | nsIPrintSettings::kInitSaveEdges |
      nsIPrintSettings::kInitSaveReversed | nsIPrintSettings::kInitSaveInColor;

  // XXX Why is Mac special? Why are we copying global print settings here?
  // nsPrintSettingsService::InitPrintSettingsFromPrefs already gets the few
  // global defaults that we want, doesn't it?
  InitPrintSettingsFromPrefs(*_retval, false, globalPrintSettings);
  return rv;
}

nsresult nsPrintSettingsServiceX::WritePrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName,
                                             uint32_t aFlags) {
  nsresult rv;

  rv = nsPrintSettingsService::WritePrefs(aPS, aPrinterName, aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintSettingsService::WritePrefs() failed");

  RefPtr<nsPrintSettingsX> printSettingsX(do_QueryObject(aPS));
  if (!printSettingsX) {
    return NS_ERROR_NO_INTERFACE;
  }
  rv = printSettingsX->WritePageFormatToPrefs();
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintSettingsX::WritePageFormatToPrefs() failed");

  return NS_OK;
}
