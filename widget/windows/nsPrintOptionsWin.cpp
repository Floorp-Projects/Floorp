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
#include "nsWindowsHelpers.h"
#include "ipc/IPCMessageUtils.h"

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

  // When creating the print dialog on Windows, the parent creates a DEVMODE
  // which is used to convey print settings to the Windows printing backend.
  // We don't, therefore, want or care about DEVMODEs sent up from the child.
  if (XRE_IsParentProcess()) {
    // A DEVMODE can actually be of arbitrary size. If it turns out that it'll
    // make our IPC message larger than the limit, then we'll error out.
    LPDEVMODEW devModeRaw;
    psWin->GetDevMode(&devModeRaw); // This actually allocates a copy of the
                                    // the nsIPrintSettingsWin DEVMODE, so
                                    // we're now responsible for deallocating
                                    // it. We'll use an nsAutoDevMode helper
                                    // to do this.
    if (devModeRaw) {
      nsAutoDevMode devMode(devModeRaw);
      devModeRaw = nullptr;

      size_t devModeTotalSize = devMode->dmSize + devMode->dmDriverExtra;
      size_t msgTotalSize = sizeof(PrintData) + devModeTotalSize;

      if (msgTotalSize > IPC::MAX_MESSAGE_SIZE) {
        return NS_ERROR_FAILURE;
      }

      // Instead of reaching in and manually reading each member, we'll just
      // copy the bits over.
      const char* devModeData = reinterpret_cast<const char*>(devMode.get());
      nsTArray<uint8_t> arrayBuf;
      arrayBuf.AppendElements(devModeData, devModeTotalSize);
      data->devModeData().SwapElements(arrayBuf);
    }
  }

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

  if (XRE_IsContentProcess()) {
    psWin->SetDeviceName(data.deviceName().get());
    psWin->SetDriverName(data.driverName().get());

    nsXPIDLString printerName;
    settings->GetPrinterName(getter_Copies(printerName));

    DEVMODEW* devModeRaw = (DEVMODEW*)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY,
                                                  data.devModeData().Length());
    if (!devModeRaw) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsAutoDevMode devMode(devModeRaw);
    devModeRaw = nullptr;

    // Seems a bit silly to copy the buffer out, just so that SetDevMode can
    // copy it again. However, if I attempt to just pass
    // data.devModeData.Elements() casted to an DEVMODEW* to SetDevMode, I get
    // a "Conversion loses qualifiers" build-time error because
    // data.devModeData.Elements() is of type const char *.
    memcpy(devMode.get(), data.devModeData().Elements(), data.devModeData().Length());

    psWin->SetDevMode(devMode); // Copies
  }

  return NS_OK;
}

nsresult nsPrintOptionsWin::_CreatePrintSettings(nsIPrintSettings **_retval)
{
  *_retval = nullptr;
  nsPrintSettingsWin* printSettings = new nsPrintSettingsWin(); // does not initially ref count
  NS_ENSURE_TRUE(printSettings, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*_retval = printSettings); // ref count

  return NS_OK;
}

