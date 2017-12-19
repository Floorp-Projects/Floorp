/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintOptionsX_h_
#define nsPrintOptionsX_h_

#include "nsPrintSettingsService.h"

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

  /*
   * These serialize and deserialize methods are not symmetrical in that
   * printSettingsX != deserialize(serialize(printSettingsX)). This is because
   * the native print settings stored in the nsPrintSettingsX's NSPrintInfo
   * object are not fully serialized. Only the values needed for successful
   * printing are.
   */
  NS_IMETHODIMP SerializeToPrintData(nsIPrintSettings* aSettings,
                                     nsIWebBrowserPrint* aWBP,
                                     mozilla::embedding::PrintData* data);
  NS_IMETHODIMP DeserializeToPrintSettings(const mozilla::embedding::PrintData& data,
                                           nsIPrintSettings* settings);

protected:
  nsresult   _CreatePrintSettings(nsIPrintSettings **_retval);
  nsresult   ReadPrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName, uint32_t aFlags);
  nsresult   WritePrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName, uint32_t aFlags);

private:
  /* Serialization done in child to be deserialized in the parent */
  nsresult SerializeToPrintDataChild(nsIPrintSettings* aSettings,
                                     nsIWebBrowserPrint* aWBP,
                                     mozilla::embedding::PrintData* data);

  /* Serialization done in parent to be deserialized in the child */
  nsresult SerializeToPrintDataParent(nsIPrintSettings* aSettings,
                                      nsIWebBrowserPrint* aWBP,
                                      mozilla::embedding::PrintData* data);
};

#endif // nsPrintOptionsX_h_
