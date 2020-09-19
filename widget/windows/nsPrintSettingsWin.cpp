/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsPrintSettingsWin.h"

#include "mozilla/ArrayUtils.h"
#include "nsCRT.h"
#include "nsDeviceContextSpecWin.h"
#include "nsPrintSettingsImpl.h"
#include "WinUtils.h"

using namespace mozilla;

// Using paper sizes from wingdi.h and the units given there, plus a little
// extra research for the ones it doesn't give. Looks like the list hasn't
// changed since Windows 2000, so should be fairly stable now.
const short kPaperSizeUnits[] = {
    nsIPrintSettings::kPaperSizeMillimeters,  // Not Used default to mm as
                                              // DEVMODE uses tenths of mm, just
                                              // in case
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_LETTER
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_LETTERSMALL
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_TABLOID
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_LEDGER
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_LEGAL
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_STATEMENT
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_EXECUTIVE
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A3
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A4
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A4SMALL
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A5
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_B4
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_B5
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_FOLIO
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_QUARTO
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_10X14
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_11X17
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_NOTE
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_ENV_9
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_ENV_10
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_ENV_11
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_ENV_12
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_ENV_14
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_CSHEET
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_DSHEET
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_ESHEET
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_DL
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_C5
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_C3
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_C4
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_C6
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_C65
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_B4
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_B5
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_B6
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_ITALY
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_ENV_MONARCH
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_ENV_PERSONAL
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_FANFOLD_US
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_FANFOLD_STD_GERMAN
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_FANFOLD_LGL_GERMAN
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ISO_B4
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JAPANESE_POSTCARD
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_9X11
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_10X11
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_15X11
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_ENV_INVITE
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_RESERVED_48
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_RESERVED_49
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_LETTER_EXTRA
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_LEGAL_EXTRA
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_TABLOID_EXTRA
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_A4_EXTRA
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_LETTER_TRANSVERSE
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A4_TRANSVERSE
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_LETTER_EXTRA_TRANSVERSE
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A_PLUS
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_B_PLUS
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_LETTER_PLUS
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A4_PLUS
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A5_TRANSVERSE
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_B5_TRANSVERSE
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A3_EXTRA
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A5_EXTRA
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_B5_EXTRA
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A2
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A3_TRANSVERSE
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A3_EXTRA_TRANSVERSE
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_DBL_JAPANESE_POSTCARD
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A6
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JENV_KAKU2
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JENV_KAKU3
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JENV_CHOU3
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JENV_CHOU4
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_LETTER_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A3_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A4_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A5_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_B4_JIS_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_B5_JIS_ROTATED
    nsIPrintSettings::
        kPaperSizeMillimeters,  // DMPAPER_JAPANESE_POSTCARD_ROTATED
    nsIPrintSettings::
        kPaperSizeMillimeters,  // DMPAPER_DBL_JAPANESE_POSTCARD_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_A6_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JENV_KAKU2_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JENV_KAKU3_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JENV_CHOU3_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JENV_CHOU4_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_B6_JIS
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_B6_JIS_ROTATED
    nsIPrintSettings::kPaperSizeInches,       // DMPAPER_12X11
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JENV_YOU4
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_JENV_YOU4_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_P16K
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_P32K
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_P32KBIG
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_1
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_2
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_3
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_4
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_5
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_6
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_7
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_8
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_9
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_10
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_P16K_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_P32K_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_P32KBIG_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_1_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_2_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_3_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_4_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_5_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_6_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_7_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_8_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_9_ROTATED
    nsIPrintSettings::kPaperSizeMillimeters,  // DMPAPER_PENV_10_ROTATED
};

NS_IMPL_ISUPPORTS_INHERITED(nsPrintSettingsWin, nsPrintSettings,
                            nsIPrintSettingsWin)

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsWin.h
 *	@update
 */
nsPrintSettingsWin::nsPrintSettingsWin()
    : nsPrintSettings(),
      mDeviceName(nullptr),
      mDriverName(nullptr),
      mDevMode(nullptr) {}

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsWin.h
 *	@update
 */
nsPrintSettingsWin::nsPrintSettingsWin(const nsPrintSettingsWin& aPS)
    : mDevMode(nullptr) {
  *this = aPS;
}

void nsPrintSettingsWin::InitWithInitializer(
    const PrintSettingsInitializer& aSettings) {
  nsPrintSettings::InitWithInitializer(aSettings);

  if (aSettings.mDevmodeWStorage.Length() < sizeof(DEVMODEW)) {
    return;
  }

  // SetDevMode copies the DEVMODE.
  SetDevMode(const_cast<DEVMODEW*>(reinterpret_cast<const DEVMODEW*>(
      aSettings.mDevmodeWStorage.Elements())));

  if (mDevMode->dmFields & DM_ORIENTATION) {
    SetOrientation(mDevMode->dmOrientation == DMORIENT_PORTRAIT
                       ? kPortraitOrientation
                       : kLandscapeOrientation);
  }

  if (mDevMode->dmFields & DM_COPIES) {
    SetNumCopies(mDevMode->dmCopies);
  }

  if (mDevMode->dmFields & DM_SCALE) {
    // Since we do the scaling, grab the DEVMODE value and reset it back to 100.
    double scale = double(mDevMode->dmScale) / 100.0f;
    if (mScaling == 1.0 || scale != 1.0) {
      SetScaling(scale);
    }
    mDevMode->dmScale = 100;
  }

  if (mDevMode->dmFields & DM_PAPERSIZE) {
    nsString paperIdString;
    paperIdString.AppendInt(mDevMode->dmPaperSize);
    SetPaperId(paperIdString);
    if (mDevMode->dmPaperSize > 0 &&
        mDevMode->dmPaperSize < int32_t(ArrayLength(kPaperSizeUnits))) {
      SetPaperSizeUnit(kPaperSizeUnits[mDevMode->dmPaperSize]);
    }
  }

  if (mDevMode->dmFields & DM_COLOR) {
    SetPrintInColor(mDevMode->dmColor == DMCOLOR_COLOR);
  }

  if (mDevMode->dmFields & DM_DUPLEX) {
    switch (mDevMode->dmDuplex) {
      default:
        MOZ_ASSERT_UNREACHABLE("bad value for dmDuplex field");
        [[fallthrough]];
      case DMDUP_SIMPLEX:
        SetDuplex(kSimplex);
        break;
      case DMDUP_HORIZONTAL:
        SetDuplex(kDuplexHorizontal);
        break;
      case DMDUP_VERTICAL:
        SetDuplex(kDuplexVertical);
        break;
    }
  }

  // Set the paper sizes to match the unit.
  double pointsToSizeUnit =
      mPaperSizeUnit == kPaperSizeInches ? 1.0 / 72.0 : 25.4 / 72.0;
  SetPaperWidth(aSettings.mPaperInfo.mSize.width * pointsToSizeUnit);
  SetPaperHeight(aSettings.mPaperInfo.mSize.height * pointsToSizeUnit);
}

already_AddRefed<nsIPrintSettings> CreatePlatformPrintSettings(
    const PrintSettingsInitializer& aSettings) {
  auto settings = MakeRefPtr<nsPrintSettingsWin>();
  settings->InitWithInitializer(aSettings);
  return settings.forget();
}

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsWin.h
 *	@update
 */
nsPrintSettingsWin::~nsPrintSettingsWin() {
  if (mDevMode) ::HeapFree(::GetProcessHeap(), 0, mDevMode);
}

NS_IMETHODIMP nsPrintSettingsWin::SetDeviceName(const nsAString& aDeviceName) {
  mDeviceName = aDeviceName;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsWin::GetDeviceName(nsAString& aDeviceName) {
  aDeviceName = mDeviceName;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsWin::SetDriverName(const nsAString& aDriverName) {
  mDriverName = aDriverName;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsWin::GetDriverName(nsAString& aDriverName) {
  aDriverName = mDriverName;
  return NS_OK;
}

void nsPrintSettingsWin::CopyDevMode(DEVMODEW* aInDevMode,
                                     DEVMODEW*& aOutDevMode) {
  aOutDevMode = nullptr;
  size_t size = aInDevMode->dmSize + aInDevMode->dmDriverExtra;
  aOutDevMode =
      (LPDEVMODEW)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, size);
  if (aOutDevMode) {
    memcpy(aOutDevMode, aInDevMode, size);
  }
}

NS_IMETHODIMP nsPrintSettingsWin::GetDevMode(DEVMODEW** aDevMode) {
  NS_ENSURE_ARG_POINTER(aDevMode);

  if (mDevMode) {
    CopyDevMode(mDevMode, *aDevMode);
  } else {
    *aDevMode = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsWin::SetDevMode(DEVMODEW* aDevMode) {
  if (mDevMode) {
    ::HeapFree(::GetProcessHeap(), 0, mDevMode);
    mDevMode = nullptr;
  }

  if (aDevMode) {
    CopyDevMode(aDevMode, mDevMode);
  }
  return NS_OK;
}

void nsPrintSettingsWin::InitUnwriteableMargin(HDC aHdc) {
  mozilla::gfx::MarginDouble margin =
      mozilla::widget::WinUtils::GetUnwriteableMarginsForDeviceInInches(aHdc);

  mUnwriteableMargin.SizeTo(NS_INCHES_TO_INT_TWIPS(margin.top),
                            NS_INCHES_TO_INT_TWIPS(margin.right),
                            NS_INCHES_TO_INT_TWIPS(margin.bottom),
                            NS_INCHES_TO_INT_TWIPS(margin.left));
}

void nsPrintSettingsWin::CopyFromNative(HDC aHdc, DEVMODEW* aDevMode) {
  MOZ_ASSERT(aHdc);
  MOZ_ASSERT(aDevMode);

  mIsInitedFromPrinter = true;
  if (aDevMode->dmFields & DM_ORIENTATION) {
    mOrientation = int32_t(aDevMode->dmOrientation == DMORIENT_PORTRAIT
                               ? kPortraitOrientation
                               : kLandscapeOrientation);
  }

  if (aDevMode->dmFields & DM_COPIES) {
    mNumCopies = aDevMode->dmCopies;
  }

  if (aDevMode->dmFields & DM_DUPLEX) {
    switch (aDevMode->dmDuplex) {
      default:
        MOZ_ASSERT_UNREACHABLE("bad value for dmDuplex field");
        [[fallthrough]];
      case DMDUP_SIMPLEX:
        mDuplex = kSimplex;
        break;
      case DMDUP_HORIZONTAL:
        mDuplex = kDuplexHorizontal;
        break;
      case DMDUP_VERTICAL:
        mDuplex = kDuplexVertical;
        break;
    }
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
    mPaperId.Truncate(0);
    mPaperId.AppendInt(aDevMode->dmPaperSize);
    // If not a paper size we know about, the unit will be the last one saved.
    if (aDevMode->dmPaperSize > 0 &&
        aDevMode->dmPaperSize < int32_t(ArrayLength(kPaperSizeUnits))) {
      mPaperSizeUnit = kPaperSizeUnits[aDevMode->dmPaperSize];
    }
  }

  if (aDevMode->dmFields & DM_COLOR) {
    mPrintInColor = aDevMode->dmColor == DMCOLOR_COLOR;
  }

  InitUnwriteableMargin(aHdc);

  int pixelsPerInchY = ::GetDeviceCaps(aHdc, LOGPIXELSY);

  // The length and width in DEVMODE are always in tenths of a millimeter.
  double sizeUnitToTenthsOfAmm =
      10L * (mPaperSizeUnit == kPaperSizeInches ? MM_PER_INCH_FLOAT : 1L);
  if (aDevMode->dmFields & DM_PAPERLENGTH) {
    mPaperHeight = aDevMode->dmPaperLength / sizeUnitToTenthsOfAmm;
  } else {
    // We need the paper height to be set, because it is used in the child for
    // layout. If it is not set in the DEVMODE, get it from the device context.
    int physicalHeight = ::GetDeviceCaps(aHdc, PHYSICALHEIGHT);
    double physicalHeightInch = double(physicalHeight) / pixelsPerInchY;
    mPaperHeight = mPaperSizeUnit == kPaperSizeInches
                       ? physicalHeightInch
                       : physicalHeightInch * MM_PER_INCH_FLOAT;
  }

  if (aDevMode->dmFields & DM_PAPERWIDTH) {
    mPaperWidth = aDevMode->dmPaperWidth / sizeUnitToTenthsOfAmm;
  } else {
    // We need the paper width to be set, because it is used in the child for
    // layout. If it is not set in the DEVMODE, get it from the device context.
    int pixelsPerInchX = ::GetDeviceCaps(aHdc, LOGPIXELSX);
    int physicalWidth = ::GetDeviceCaps(aHdc, PHYSICALWIDTH);
    double physicalWidthInch = double(physicalWidth) / pixelsPerInchX;
    mPaperWidth = mPaperSizeUnit == kPaperSizeInches
                      ? physicalWidthInch
                      : physicalWidthInch * MM_PER_INCH_FLOAT;
  }

  // Using LOGPIXELSY to match existing code for print scaling calculations.
  mResolution = pixelsPerInchY;
}

void nsPrintSettingsWin::CopyToNative(DEVMODEW* aDevMode) {
  MOZ_ASSERT(aDevMode);

  if (!mPaperId.IsEmpty()) {
    aDevMode->dmPaperSize = _wtoi((const wchar_t*)mPaperId.BeginReading());
    aDevMode->dmFields |= DM_PAPERSIZE;
  } else {
    aDevMode->dmPaperSize = 0;
    aDevMode->dmFields &= ~DM_PAPERSIZE;
  }

  aDevMode->dmFields |= DM_COLOR;
  aDevMode->dmColor = mPrintInColor ? DMCOLOR_COLOR : DMCOLOR_MONOCHROME;

  // The length and width in DEVMODE are always in tenths of a millimeter.
  double sizeUnitToTenthsOfAmm =
      10L * (mPaperSizeUnit == kPaperSizeInches ? MM_PER_INCH_FLOAT : 1L);

  // Note: small page sizes can be required here for sticker, label and slide
  // printers etc. see bug 1271900.
  if (mPaperHeight > 0) {
    aDevMode->dmPaperLength = mPaperHeight * sizeUnitToTenthsOfAmm;
    aDevMode->dmFields |= DM_PAPERLENGTH;
  } else {
    aDevMode->dmPaperLength = 0;
    aDevMode->dmFields &= ~DM_PAPERLENGTH;
  }

  if (mPaperWidth > 0) {
    aDevMode->dmPaperWidth = mPaperWidth * sizeUnitToTenthsOfAmm;
    aDevMode->dmFields |= DM_PAPERWIDTH;
  } else {
    aDevMode->dmPaperWidth = 0;
    aDevMode->dmFields &= ~DM_PAPERWIDTH;
  }

  // Setup Orientation
  aDevMode->dmOrientation = mOrientation == kPortraitOrientation
                                ? DMORIENT_PORTRAIT
                                : DMORIENT_LANDSCAPE;
  aDevMode->dmFields |= DM_ORIENTATION;

  // Setup Number of Copies
  aDevMode->dmCopies = mNumCopies;
  aDevMode->dmFields |= DM_COPIES;

  // Setup Simplex/Duplex mode
  switch (mDuplex) {
    case kSimplex:
      aDevMode->dmDuplex = DMDUP_SIMPLEX;
      aDevMode->dmFields |= DM_DUPLEX;
      break;
    case kDuplexHorizontal:
      aDevMode->dmDuplex = DMDUP_HORIZONTAL;
      aDevMode->dmFields |= DM_DUPLEX;
      break;
    case kDuplexVertical:
      aDevMode->dmDuplex = DMDUP_VERTICAL;
      aDevMode->dmFields |= DM_DUPLEX;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("bad value for duplex option");
      break;
  }
}

//-------------------------------------------
nsresult nsPrintSettingsWin::_Clone(nsIPrintSettings** _retval) {
  RefPtr<nsPrintSettingsWin> printSettings = new nsPrintSettingsWin(*this);
  printSettings.forget(_retval);
  return NS_OK;
}

//-------------------------------------------
nsPrintSettingsWin& nsPrintSettingsWin::operator=(
    const nsPrintSettingsWin& rhs) {
  if (this == &rhs) {
    return *this;
  }

  ((nsPrintSettings&)*this) = rhs;

  // Use free because we used the native malloc to create the memory
  if (mDevMode) {
    ::HeapFree(::GetProcessHeap(), 0, mDevMode);
  }

  mDeviceName = rhs.mDeviceName;
  mDriverName = rhs.mDriverName;

  if (rhs.mDevMode) {
    CopyDevMode(rhs.mDevMode, mDevMode);
  } else {
    mDevMode = nullptr;
  }

  return *this;
}

//-------------------------------------------
nsresult nsPrintSettingsWin::_Assign(nsIPrintSettings* aPS) {
  nsPrintSettingsWin* psWin = static_cast<nsPrintSettingsWin*>(aPS);
  *this = *psWin;
  return NS_OK;
}

//----------------------------------------------------------------------
// Testing of assign and clone
// This define turns on the testing module below
// so at start up it writes and reads the prefs.
#ifdef DEBUG_rodsX
#  include "nsIPrintSettingsService.h"
#  include "nsIServiceManager.h"
class Tester {
 public:
  Tester();
};
Tester::Tester() {
  nsCOMPtr<nsIPrintSettings> ps;
  nsresult rv;
  nsCOMPtr<nsIPrintSettingsService> printService =
      do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = printService->CreatePrintSettings(getter_AddRefs(ps));
  }

  if (ps) {
    ps->SetPrintOptions(nsIPrintSettings::kPrintOddPages, true);
    ps->SetPrintOptions(nsIPrintSettings::kPrintEvenPages, false);
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
    ps->SetPaperId(NS_ConvertUTF8toUTF16("Paper Id").get());
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
