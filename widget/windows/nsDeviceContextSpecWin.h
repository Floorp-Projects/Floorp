/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceContextSpecWin_h___
#define nsDeviceContextSpecWin_h___

#include "nsCOMPtr.h"
#include "nsIDeviceContextSpec.h"
#include "nsIPrintOptions.h" // For nsIPrinterEnumerator
#include "nsIPrintSettings.h"
#include "nsISupportsPrimitives.h"
#include <windows.h>
#include "mozilla/Attributes.h"

class nsIWidget;

class nsDeviceContextSpecWin : public nsIDeviceContextSpec
{
public:
  nsDeviceContextSpecWin();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetSurfaceForPrinter(gfxASurface **surface);
  NS_IMETHOD BeginDocument(const nsAString& aTitle,
                           char16_t*       aPrintToFileName,
                           int32_t          aStartPage,
                           int32_t          aEndPage) { return NS_OK; }
  NS_IMETHOD EndDocument() { return NS_OK; }
  NS_IMETHOD BeginPage() { return NS_OK; }
  NS_IMETHOD EndPage() { return NS_OK; }

  NS_IMETHOD Init(nsIWidget* aWidget, nsIPrintSettings* aPS, bool aIsPrintPreview);

  void GetDriverName(wchar_t *&aDriverName) const   { aDriverName = mDriverName;     }
  void GetDeviceName(wchar_t *&aDeviceName) const   { aDeviceName = mDeviceName;     }

  // The GetDevMode will return a pointer to a DevMode
  // whether it is from the Global memory handle or just the DevMode
  // To get the DevMode from the Global memory Handle it must lock it 
  // So this call must be paired with a call to UnlockGlobalHandle
  void GetDevMode(LPDEVMODEW &aDevMode);

  // helper functions
  nsresult GetDataFromPrinter(char16ptr_t aName, nsIPrintSettings* aPS = nullptr);

  static nsresult SetPrintSettingsFromDevMode(nsIPrintSettings* aPrintSettings, 
                                              LPDEVMODEW         aDevMode);

protected:

  void SetDeviceName(char16ptr_t aDeviceName);
  void SetDriverName(char16ptr_t aDriverName);
  void SetDevMode(LPDEVMODEW aDevMode);

  void SetupPaperInfoFromSettings();

  virtual ~nsDeviceContextSpecWin();

  wchar_t*      mDriverName;
  wchar_t*      mDeviceName;
  LPDEVMODEW mDevMode;

  nsCOMPtr<nsIPrintSettings> mPrintSettings;
};


//-------------------------------------------------------------------------
// Printer Enumerator
//-------------------------------------------------------------------------
class nsPrinterEnumeratorWin MOZ_FINAL : public nsIPrinterEnumerator
{
  ~nsPrinterEnumeratorWin();

public:
  nsPrinterEnumeratorWin();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTERENUMERATOR
};

#endif
