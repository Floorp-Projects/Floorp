/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsServiceGTK_h
#define nsPrintSettingsServiceGTK_h

#include "nsPrintSettingsService.h"

namespace mozilla
{
namespace embedding
{
  class PrintData;
} // namespace embedding
} // namespace mozilla

class nsPrintSettingsServiceGTK final : public nsPrintSettingsService
{
public:
  nsPrintSettingsServiceGTK() {}
  virtual ~nsPrintSettingsServiceGTK() {}

  NS_IMETHODIMP SerializeToPrintData(nsIPrintSettings* aSettings,
                                     nsIWebBrowserPrint* aWBP,
                                     mozilla::embedding::PrintData* data);
  NS_IMETHODIMP DeserializeToPrintSettings(const mozilla::embedding::PrintData& data,
                                           nsIPrintSettings* settings);

  virtual nsresult _CreatePrintSettings(nsIPrintSettings **_retval);
};



#endif // nsPrintSettingsServiceGTK_h
