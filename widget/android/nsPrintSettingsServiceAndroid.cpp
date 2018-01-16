/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsPrintSettingsServiceAndroid.h"

#include "nsPrintSettingsImpl.h"

class nsPrintSettingsAndroid : public nsPrintSettings {
public:
  nsPrintSettingsAndroid()
  {
    // The aim here is to set up the objects enough that silent printing works
    SetOutputFormat(nsIPrintSettings::kOutputFormatPDF);
    SetPrinterName(NS_LITERAL_STRING("PDF printer"));
  }
};

nsresult
nsPrintSettingsServiceAndroid::_CreatePrintSettings(nsIPrintSettings** _retval)
{
  nsPrintSettings * printSettings = new nsPrintSettingsAndroid();
  NS_ENSURE_TRUE(printSettings, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*_retval = printSettings);
  (void)InitPrintSettingsFromPrefs(*_retval, false,
                                   nsIPrintSettings::kInitSaveAll);
  return NS_OK;
}
