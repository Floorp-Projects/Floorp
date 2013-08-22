/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsPrintSettingsWin.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsPrintSettingsWin, 
                             nsPrintSettings, 
                             nsIPrintSettingsWin)

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsWin.h
 *	@update 
 */
nsPrintSettingsWin::nsPrintSettingsWin() :
  nsPrintSettings(),
  mDeviceName(nullptr),
  mDriverName(nullptr),
  mDevMode(nullptr)
{

}

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsWin.h
 *	@update 
 */
nsPrintSettingsWin::nsPrintSettingsWin(const nsPrintSettingsWin& aPS) :
  mDeviceName(nullptr),
  mDriverName(nullptr),
  mDevMode(nullptr)
{
  *this = aPS;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsWin.h
 *	@update 
 */
nsPrintSettingsWin::~nsPrintSettingsWin()
{
  if (mDeviceName) nsMemory::Free(mDeviceName);
  if (mDriverName) nsMemory::Free(mDriverName);
  if (mDevMode) ::HeapFree(::GetProcessHeap(), 0, mDevMode);
}

/* [noscript] attribute charPtr deviceName; */
NS_IMETHODIMP nsPrintSettingsWin::SetDeviceName(const PRUnichar * aDeviceName)
{
  if (mDeviceName) {
    nsMemory::Free(mDeviceName);
  }
  mDeviceName = aDeviceName?wcsdup(aDeviceName):nullptr;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsWin::GetDeviceName(PRUnichar **aDeviceName)
{
  NS_ENSURE_ARG_POINTER(aDeviceName);
  *aDeviceName = mDeviceName?wcsdup(mDeviceName):nullptr;
  return NS_OK;
}

/* [noscript] attribute charPtr driverName; */
NS_IMETHODIMP nsPrintSettingsWin::SetDriverName(const PRUnichar * aDriverName)
{
  if (mDriverName) {
    nsMemory::Free(mDriverName);
  }
  mDriverName = aDriverName?wcsdup(aDriverName):nullptr;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsWin::GetDriverName(PRUnichar **aDriverName)
{
  NS_ENSURE_ARG_POINTER(aDriverName);
  *aDriverName = mDriverName?wcsdup(mDriverName):nullptr;
  return NS_OK;
}

void nsPrintSettingsWin::CopyDevMode(DEVMODEW* aInDevMode, DEVMODEW *& aOutDevMode)
{
  aOutDevMode = nullptr;
  size_t size = aInDevMode->dmSize + aInDevMode->dmDriverExtra;
  aOutDevMode = (LPDEVMODEW)::HeapAlloc (::GetProcessHeap(), HEAP_ZERO_MEMORY, size);
  if (aOutDevMode) {
    memcpy(aOutDevMode, aInDevMode, size);
  }

}

/* [noscript] attribute nsDevMode devMode; */
NS_IMETHODIMP nsPrintSettingsWin::GetDevMode(DEVMODEW * *aDevMode)
{
  NS_ENSURE_ARG_POINTER(aDevMode);

  if (mDevMode) {
    CopyDevMode(mDevMode, *aDevMode);
  } else {
    *aDevMode = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsWin::SetDevMode(DEVMODEW * aDevMode)
{
  if (mDevMode) {
    ::HeapFree(::GetProcessHeap(), 0, mDevMode);
    mDevMode = NULL;
  }

  if (aDevMode) {
    CopyDevMode(aDevMode, mDevMode);
  }
  return NS_OK;
}

//-------------------------------------------
nsresult 
nsPrintSettingsWin::_Clone(nsIPrintSettings **_retval)
{
  nsPrintSettingsWin* printSettings = new nsPrintSettingsWin(*this);
  return printSettings->QueryInterface(NS_GET_IID(nsIPrintSettings), (void**)_retval); // ref counts
}

//-------------------------------------------
nsPrintSettingsWin& nsPrintSettingsWin::operator=(const nsPrintSettingsWin& rhs)
{
  if (this == &rhs) {
    return *this;
  }

  ((nsPrintSettings&) *this) = rhs;

  if (mDeviceName) {
    free(mDeviceName);
  }

  if (mDriverName) {
    free(mDriverName);
  }

  // Use free because we used the native malloc to create the memory
  if (mDevMode) {
    ::HeapFree(::GetProcessHeap(), 0, mDevMode);
  }

  mDeviceName = rhs.mDeviceName?wcsdup(rhs.mDeviceName):nullptr;
  mDriverName = rhs.mDriverName?wcsdup(rhs.mDriverName):nullptr;

  if (rhs.mDevMode) {
    CopyDevMode(rhs.mDevMode, mDevMode);
  } else {
    mDevMode = nullptr;
  }

  return *this;
}

//-------------------------------------------
/* void assign (in nsIPrintSettings aPS); */
nsresult 
nsPrintSettingsWin::_Assign(nsIPrintSettings *aPS)
{
  nsPrintSettingsWin *psWin = static_cast<nsPrintSettingsWin*>(aPS);
  *this = *psWin;
  return NS_OK;
}

//----------------------------------------------------------------------
// Testing of assign and clone
// This define turns on the testing module below
// so at start up it writes and reads the prefs.
#ifdef DEBUG_rodsX
#include "nsIPrintOptions.h"
#include "nsIServiceManager.h"
class Tester {
public:
  Tester();
};
Tester::Tester()
{
  nsCOMPtr<nsIPrintSettings> ps;
  nsresult rv;
  nsCOMPtr<nsIPrintOptions> printService = do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = printService->CreatePrintSettings(getter_AddRefs(ps));
  }

  if (ps) {
    ps->SetPrintOptions(nsIPrintSettings::kPrintOddPages,  true);
    ps->SetPrintOptions(nsIPrintSettings::kPrintEvenPages,  false);
    ps->SetMarginTop(1.0);
    ps->SetMarginLeft(1.0);
    ps->SetMarginBottom(1.0);
    ps->SetMarginRight(1.0);
    ps->SetScaling(0.5);
    ps->SetPrintBGColors(true);
    ps->SetPrintBGImages(true);
    ps->SetPrintRange(15);
    ps->SetHeaderStrLeft(NS_ConvertUTF8toUTF16("Left").get());
    ps->SetHeaderStrCenter(NS_ConvertUTF8toUTF16("Center").get());
    ps->SetHeaderStrRight(NS_ConvertUTF8toUTF16("Right").get());
    ps->SetFooterStrLeft(NS_ConvertUTF8toUTF16("Left").get());
    ps->SetFooterStrCenter(NS_ConvertUTF8toUTF16("Center").get());
    ps->SetFooterStrRight(NS_ConvertUTF8toUTF16("Right").get());
    ps->SetPaperName(NS_ConvertUTF8toUTF16("Paper Name").get());
    ps->SetPaperSizeType(10);
    ps->SetPaperData(1);
    ps->SetPaperWidth(100.0);
    ps->SetPaperHeight(50.0);
    ps->SetPaperSizeUnit(nsIPrintSettings::kPaperSizeMillimeters);
    ps->SetPrintReversed(true);
    ps->SetPrintInColor(true);
    ps->SetOrientation(nsIPrintSettings::kLandscapeOrientation);
    ps->SetPrintCommand(NS_ConvertUTF8toUTF16("Command").get());
    ps->SetNumCopies(2);
    ps->SetPrinterName(NS_ConvertUTF8toUTF16("Printer Name").get());
    ps->SetPrintToFile(true);
    ps->SetToFileName(NS_ConvertUTF8toUTF16("File Name").get());
    ps->SetPrintPageDelay(1000);

    nsCOMPtr<nsIPrintSettings> ps2;
    if (NS_SUCCEEDED(rv)) {
      rv = printService->CreatePrintSettings(getter_AddRefs(ps2));
    }

    ps2->Assign(ps);

    nsCOMPtr<nsIPrintSettings> psClone;
    ps2->Clone(getter_AddRefs(psClone));

  }

}
Tester gTester;
#endif

