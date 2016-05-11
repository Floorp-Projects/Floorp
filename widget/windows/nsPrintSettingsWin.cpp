/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsPrintSettingsWin.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS_INHERITED(nsPrintSettingsWin, 
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
  if (mDeviceName) free(mDeviceName);
  if (mDriverName) free(mDriverName);
  if (mDevMode) ::HeapFree(::GetProcessHeap(), 0, mDevMode);
}

NS_IMETHODIMP nsPrintSettingsWin::SetDeviceName(const char16_t * aDeviceName)
{
  if (mDeviceName) {
    free(mDeviceName);
  }
  mDeviceName = aDeviceName?wcsdup(char16ptr_t(aDeviceName)):nullptr;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsWin::GetDeviceName(char16_t **aDeviceName)
{
  NS_ENSURE_ARG_POINTER(aDeviceName);
  *aDeviceName = mDeviceName?reinterpret_cast<char16_t*>(wcsdup(mDeviceName)):nullptr;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsWin::SetDriverName(const char16_t * aDriverName)
{
  if (mDriverName) {
    free(mDriverName);
  }
  mDriverName = aDriverName?wcsdup(char16ptr_t(aDriverName)):nullptr;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsWin::GetDriverName(char16_t **aDriverName)
{
  NS_ENSURE_ARG_POINTER(aDriverName);
  *aDriverName = mDriverName?reinterpret_cast<char16_t*>(wcsdup(mDriverName)):nullptr;
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
    mDevMode = nullptr;
  }

  if (aDevMode) {
    CopyDevMode(aDevMode, mDevMode);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsWin::GetPrintableWidthInInches(double* aPrintableWidthInInches)
{
  MOZ_ASSERT(aPrintableWidthInInches);
  *aPrintableWidthInInches = mPrintableWidthInInches;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsWin::SetPrintableWidthInInches(double aPrintableWidthInInches)
{
  mPrintableWidthInInches = aPrintableWidthInInches;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsWin::GetPrintableHeightInInches(double* aPrintableHeightInInches)
{
  MOZ_ASSERT(aPrintableHeightInInches);
  *aPrintableHeightInInches = mPrintableHeightInInches;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsWin::SetPrintableHeightInInches(double aPrintableHeightInInches)
{
  mPrintableHeightInInches = aPrintableHeightInInches;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsWin::GetEffectivePageSize(double *aWidth, double *aHeight)
{
  // If printable page size not set, fall back to nsPrintSettings.
  if (mPrintableWidthInInches == 0l || mPrintableHeightInInches == 0l) {
    return nsPrintSettings::GetEffectivePageSize(aWidth, aHeight);
  }

  if (mOrientation == kPortraitOrientation) {
    *aWidth = NS_INCHES_TO_TWIPS(mPrintableWidthInInches);
    *aHeight = NS_INCHES_TO_TWIPS(mPrintableHeightInInches);
  } else {
    *aHeight = NS_INCHES_TO_TWIPS(mPrintableWidthInInches);
    *aWidth = NS_INCHES_TO_TWIPS(mPrintableHeightInInches);
  }
  return NS_OK;
}

void
nsPrintSettingsWin::CopyFromNative(HDC aHdc, DEVMODEW* aDevMode)
{
  MOZ_ASSERT(aHdc);
  MOZ_ASSERT(aDevMode);

  mIsInitedFromPrinter = true;
  if (aDevMode->dmFields & DM_ORIENTATION) {
    mOrientation = int32_t(aDevMode->dmOrientation == DMORIENT_PORTRAIT
                           ? kPortraitOrientation : kLandscapeOrientation);
  }

  if (aDevMode->dmFields & DM_COPIES) {
    mNumCopies = aDevMode->dmCopies;
  }

  // Since we do the scaling, grab their value and reset back to 100.
  if (aDevMode->dmFields & DM_SCALE) {
    double scale = double(aDevMode->dmScale) / 100.0f;
    if (mScaling == 1.0 || scale != 1.0) {
      mScaling = scale;
    }
    aDevMode->dmScale = 100;
  }

  if (aDevMode->dmFields & DM_PAPERSIZE) {
    mPaperData = aDevMode->dmPaperSize;
  } else {
    mPaperData = -1;
  }

  // The length and width in DEVMODE are always in tenths of a millimeter, so
  // storing them in millimeters is easiest.
  mPaperSizeUnit = kPaperSizeMillimeters;
  if (aDevMode->dmFields & DM_PAPERLENGTH) {
    mPaperHeight = aDevMode->dmPaperLength / 10l;
  } else {
    mPaperHeight = -1l;
  }

  if (aDevMode->dmFields & DM_PAPERWIDTH) {
    mPaperWidth = aDevMode->dmPaperWidth / 10l;
  } else {
    mPaperWidth = -1l;
  }

  // On Windows we currently create a surface using the printable area of the
  // page and don't set the unwriteable [sic] margins. Using the unwriteable
  // margins doesn't appear to work on Windows, but I am not sure if this is a
  // bug elsewhere in our code or a Windows quirk.
  // Note: we only scale the printing using the LOGPIXELSY, so we use that
  // when calculating the surface width as well as the height.
  int32_t printableWidthInDots = GetDeviceCaps(aHdc, HORZRES);
  int32_t printableHeightInDots = GetDeviceCaps(aHdc, VERTRES);
  int32_t heightDPI = GetDeviceCaps(aHdc, LOGPIXELSY);

  // Keep these values in portrait format, so we can reflect our own changes
  // to mOrientation.
  if (mOrientation == kPortraitOrientation) {
    mPrintableWidthInInches = double(printableWidthInDots) / heightDPI;
    mPrintableHeightInInches = double(printableHeightInDots) / heightDPI;
  } else {
    mPrintableHeightInInches = double(printableWidthInDots) / heightDPI;
    mPrintableWidthInInches = double(printableHeightInDots) / heightDPI;
  }

  // Using Y to match existing code for print scaling calculations.
  mResolution = heightDPI;
}

void
nsPrintSettingsWin::CopyToNative(DEVMODEW* aDevMode)
{
  MOZ_ASSERT(aDevMode);

  if (mPaperData >= 0) {
    aDevMode->dmPaperSize = mPaperData;
    aDevMode->dmFields |= DM_PAPERSIZE;
  } else {
    aDevMode->dmPaperSize = 0;
    aDevMode->dmFields &= ~DM_PAPERSIZE;
  }

  double paperHeight;
  double paperWidth;
  if (mPaperSizeUnit == kPaperSizeMillimeters) {
    paperHeight = mPaperHeight;
    paperWidth = mPaperWidth;
  } else {
    paperHeight = mPaperHeight * MM_PER_INCH_FLOAT;
    paperWidth = mPaperWidth * MM_PER_INCH_FLOAT;
  }

  // Set a sensible limit on the minimum height and width.
  if (paperHeight >= MM_PER_INCH_FLOAT) {
    // In DEVMODEs, physical lengths are stored in tenths of millimeters.
    aDevMode->dmPaperLength = paperHeight * 10l;
    aDevMode->dmFields |= DM_PAPERLENGTH;
  } else {
    aDevMode->dmPaperLength = 0;
    aDevMode->dmFields &= ~DM_PAPERLENGTH;
  }

  if (paperWidth >= MM_PER_INCH_FLOAT) {
    aDevMode->dmPaperWidth = paperWidth * 10l;
    aDevMode->dmFields |= DM_PAPERWIDTH;
  } else {
    aDevMode->dmPaperWidth = 0;
    aDevMode->dmFields &= ~DM_PAPERWIDTH;
  }

  // Setup Orientation
  aDevMode->dmOrientation = mOrientation == kPortraitOrientation
                            ? DMORIENT_PORTRAIT : DMORIENT_LANDSCAPE;
  aDevMode->dmFields |= DM_ORIENTATION;

  // Setup Number of Copies
  aDevMode->dmCopies = mNumCopies;
  aDevMode->dmFields |= DM_COPIES;
}

//-------------------------------------------
nsresult 
nsPrintSettingsWin::_Clone(nsIPrintSettings **_retval)
{
  RefPtr<nsPrintSettingsWin> printSettings = new nsPrintSettingsWin(*this);
  printSettings.forget(_retval);
  return NS_OK;
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
