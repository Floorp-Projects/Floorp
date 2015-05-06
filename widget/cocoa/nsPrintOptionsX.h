/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintOptionsX_h_
#define nsPrintOptionsX_h_

#include "nsPrintOptionsImpl.h"

namespace mozilla
{
namespace embedding
{
  class PrintData;
} // namespace embedding
} // namespace mozilla

class nsPrintOptionsX : public nsPrintOptions
{
public:
             nsPrintOptionsX();
  virtual    ~nsPrintOptionsX();

  NS_IMETHODIMP SerializeToPrintData(nsIPrintSettings* aSettings,
                                     nsIWebBrowserPrint* aWBP,
                                     mozilla::embedding::PrintData* data);
  NS_IMETHODIMP DeserializeToPrintSettings(const mozilla::embedding::PrintData& data,
                                           nsIPrintSettings* settings);

protected:
  nsresult   _CreatePrintSettings(nsIPrintSettings **_retval);
  nsresult   ReadPrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName, uint32_t aFlags);
  nsresult   WritePrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName, uint32_t aFlags);
};

#endif // nsPrintOptionsX_h_
