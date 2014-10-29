/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsPrintOptionsWin.h"
#include "nsPrintSettingsWin.h"
#include "nsPrintDialogUtil.h"

#include "nsGfxCIID.h"
#include "nsIServiceManager.h"
#include "nsIWebBrowserPrint.h"

const char kPrinterEnumeratorContractID[] = "@mozilla.org/gfx/printerenumerator;1";

using namespace mozilla::embedding;

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsWin.h
 *	@update 6/21/00 dwc
 */
nsPrintOptionsWin::nsPrintOptionsWin()
{

}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintOptionsWin::~nsPrintOptionsWin()
{
}

NS_IMETHODIMP
nsPrintOptionsWin::SerializeToPrintData(nsIPrintSettings* aSettings,
                                        nsIWebBrowserPrint* aWBP,
                                        PrintData* data)
{
  nsresult rv = nsPrintOptions::SerializeToPrintData(aSettings, aWBP, data);
  NS_ENSURE_SUCCESS(rv, rv);

  // Windows wants this information for its print dialogs
  if (aWBP) {
    aWBP->GetIsFramesetDocument(&data->isFramesetDocument());
    aWBP->GetIsFramesetFrameSelected(&data->isFramesetFrameSelected());
    aWBP->GetIsIFrameSelected(&data->isIFrameSelected());
    aWBP->GetIsRangeSelection(&data->isRangeSelection());
  }

  nsCOMPtr<nsIPrintSettingsWin> psWin = do_QueryInterface(aSettings);
  if (!psWin) {
    return NS_ERROR_FAILURE;
  }

  char16_t* deviceName;
  char16_t* driverName;

  psWin->GetDeviceName(&deviceName);
  psWin->GetDriverName(&driverName);

  data->deviceName().Assign(deviceName);
  data->driverName().Assign(driverName);

  free(deviceName);
  free(driverName);

  return NS_OK;
}

NS_IMETHODIMP
nsPrintOptionsWin::DeserializeToPrintSettings(const PrintData& data,
                                              nsIPrintSettings* settings)
{
  nsresult rv = nsPrintOptions::DeserializeToPrintSettings(data, settings);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrintSettingsWin> psWin = do_QueryInterface(settings);
  if (!settings) {
    return NS_ERROR_FAILURE;
  }

  psWin->SetDeviceName(data.deviceName().get());
  psWin->SetDriverName(data.driverName().get());

  // We also need to prepare a DevMode and stuff it into our newly
  // created nsIPrintSettings...
  nsXPIDLString printerName;
  settings->GetPrinterName(getter_Copies(printerName));
  HGLOBAL gDevMode = CreateGlobalDevModeAndInit(printerName, settings);
  LPDEVMODEW devMode = (LPDEVMODEW)::GlobalLock(gDevMode);
  psWin->SetDevMode(devMode);

  ::GlobalUnlock(gDevMode);
  ::GlobalFree(gDevMode);

  return NS_OK;
}

/* nsIPrintSettings CreatePrintSettings (); */
nsresult nsPrintOptionsWin::_CreatePrintSettings(nsIPrintSettings **_retval)
{
  *_retval = nullptr;
  nsPrintSettingsWin* printSettings = new nsPrintSettingsWin(); // does not initially ref count
  NS_ENSURE_TRUE(printSettings, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*_retval = printSettings); // ref count

  return NS_OK;
}

