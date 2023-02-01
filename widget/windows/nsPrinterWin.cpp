/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterWin.h"

#include <algorithm>
#include <windows.h>
#include <winspool.h>

#include "mozilla/Array.h"
#include "mozilla/dom/Promise.h"
#include "nsPaper.h"
#include "nsPrintSettingsImpl.h"
#include "nsPrintSettingsWin.h"
#include "nsWindowsHelpers.h"
#include "PrintBackgroundTask.h"
#include "WinUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;
using mozilla::PrintSettingsInitializer;
using mozilla::dom::Promise;

static const double kPointsPerTenthMM = 72.0 / 254.0;
static const double kPointsPerInch = 72.0;

nsPrinterWin::nsPrinterWin(const CommonPaperInfoArray* aArray,
                           const nsAString& aName)
    : nsPrinterBase(aArray),
      mName(aName),
      mDefaultDevmodeWStorage("nsPrinterWin::mDefaultDevmodeWStorage") {}

// static
already_AddRefed<nsPrinterWin> nsPrinterWin::Create(
    const CommonPaperInfoArray* aArray, const nsAString& aName) {
  return do_AddRef(new nsPrinterWin(aArray, aName));
}

template <class T>
static nsTArray<T> GetDeviceCapabilityArray(const LPWSTR aPrinterName,
                                            WORD aCapabilityID,
                                            mozilla::Mutex& aDriverMutex,
                                            int& aCount) {
  MOZ_ASSERT(aCount >= 0, "Possibly passed aCount from previous error case.");

  nsTArray<T> caps;

  // We only want to access printer drivers in the parent process.
  if (!XRE_IsParentProcess()) {
    return caps;
  }

  // Both the call to get the size and the call to actually populate the array
  // are relatively expensive, so as sometimes the lengths of the arrays that we
  // retrieve depend on each other we allow a count to be passed in to save the
  // first call. As we allocate double the count anyway this should allay any
  // safety worries.
  if (!aCount) {
    // Passing nullptr as the port here seems to work. Given that we would have
    // to OpenPrinter with just the name anyway to get the port that makes
    // sense. Also, the printer set-up seems to stop you from having two
    // printers with the same name. Note: this (and the call below) are blocking
    // calls, which could be slow.
    MutexAutoLock autoLock(aDriverMutex);
    aCount = ::DeviceCapabilitiesW(aPrinterName, nullptr, aCapabilityID,
                                   nullptr, nullptr);
    if (aCount <= 0) {
      return caps;
    }
  }

  // As DeviceCapabilitiesW doesn't take a size, there is a greater risk of the
  // buffer being overflowed, so we over-allocate for safety.
  caps.SetLength(aCount * 2);
  MutexAutoLock autoLock(aDriverMutex);
  int count =
      ::DeviceCapabilitiesW(aPrinterName, nullptr, aCapabilityID,
                            reinterpret_cast<LPWSTR>(caps.Elements()), nullptr);
  if (count <= 0) {
    caps.Clear();
    return caps;
  }

  // We know from bug 1673708 that sometimes the final array returned is smaller
  // than the required array count. Assert here to see if this is reproduced on
  // test servers.
  MOZ_ASSERT(count == aCount, "Different array count returned than expected.");

  // Note that TruncateLength will crash if count > caps.Length().
  caps.TruncateLength(count);
  return caps;
}

static void DevmodeToSettingsInitializer(
    const nsString& aPrinterName, const DEVMODEW* aDevmode,
    mozilla::Mutex& aDriverMutex,
    PrintSettingsInitializer& aSettingsInitializer) {
  aSettingsInitializer.mPrinter.Assign(aPrinterName);

  HDC dc;
  {
    MutexAutoLock autoLock(aDriverMutex);
    dc = ::CreateICW(nullptr, aPrinterName.get(), nullptr, aDevmode);
  }
  nsAutoHDC printerDc(dc);
  MOZ_ASSERT(printerDc, "CreateICW failed");
  if (!printerDc) {
    return;
  }

  if (aDevmode->dmFields & DM_PAPERSIZE) {
    aSettingsInitializer.mPaperInfo.mId.Truncate();
    aSettingsInitializer.mPaperInfo.mId.AppendInt(aDevmode->dmPaperSize);
    // If it is not a paper size we know about, the unit will remain unchanged.
    nsPrintSettingsWin::PaperSizeUnitFromDmPaperSize(
        aDevmode->dmPaperSize, aSettingsInitializer.mPaperSizeUnit);
  }

  int pixelsPerInchY = ::GetDeviceCaps(printerDc, LOGPIXELSY);
  int physicalHeight = ::GetDeviceCaps(printerDc, PHYSICALHEIGHT);
  double heightInInches = double(physicalHeight) / pixelsPerInchY;
  int pixelsPerInchX = ::GetDeviceCaps(printerDc, LOGPIXELSX);
  int physicalWidth = ::GetDeviceCaps(printerDc, PHYSICALWIDTH);
  double widthInches = double(physicalWidth) / pixelsPerInchX;
  if (aDevmode->dmFields & DM_ORIENTATION &&
      aDevmode->dmOrientation == DMORIENT_LANDSCAPE) {
    std::swap(widthInches, heightInInches);
  }
  aSettingsInitializer.mPaperInfo.mSize.SizeTo(widthInches * kPointsPerInch,
                                               heightInInches * kPointsPerInch);

  gfx::MarginDouble margin =
      WinUtils::GetUnwriteableMarginsForDeviceInInches(printerDc);
  aSettingsInitializer.mPaperInfo.mUnwriteableMargin = Some(MarginDouble{
      margin.top * kPointsPerInch, margin.right * kPointsPerInch,
      margin.bottom * kPointsPerInch, margin.left * kPointsPerInch});

  // Using Y to match existing code for print scaling calculations.
  aSettingsInitializer.mResolution = pixelsPerInchY;

  if (aDevmode->dmFields & DM_COLOR) {
    // See comment for PrintSettingsInitializer.mPrintInColor
    aSettingsInitializer.mPrintInColor =
        aDevmode->dmColor != DMCOLOR_MONOCHROME;
  }

  if (aDevmode->dmFields & DM_ORIENTATION) {
    aSettingsInitializer.mSheetOrientation =
        int32_t(aDevmode->dmOrientation == DMORIENT_PORTRAIT
                    ? nsPrintSettings::kPortraitOrientation
                    : nsPrintSettings::kLandscapeOrientation);
  }

  if (aDevmode->dmFields & DM_COPIES) {
    aSettingsInitializer.mNumCopies = aDevmode->dmCopies;
  }

  if (aDevmode->dmFields & DM_DUPLEX) {
    switch (aDevmode->dmDuplex) {
      default:
        MOZ_FALLTHROUGH_ASSERT("bad value for dmDuplex field");
      case DMDUP_SIMPLEX:
        aSettingsInitializer.mDuplex = nsPrintSettings::kDuplexNone;
        break;
      case DMDUP_VERTICAL:
        aSettingsInitializer.mDuplex = nsPrintSettings::kDuplexFlipOnLongEdge;
        break;
      case DMDUP_HORIZONTAL:
        aSettingsInitializer.mDuplex = nsPrintSettings::kDuplexFlipOnShortEdge;
        break;
    }
  }
}

NS_IMETHODIMP
nsPrinterWin::GetName(nsAString& aName) {
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterWin::GetSystemName(nsAString& aName) {
  aName.Assign(mName);
  return NS_OK;
}

namespace mozilla {
template <>
void ResolveOrReject(Promise& aPromise, nsPrinterWin& aPrinter,
                     const PrintSettingsInitializer& aResult) {
  aPromise.MaybeResolve(
      RefPtr<nsIPrintSettings>(CreatePlatformPrintSettings(aResult)));
}
}  // namespace mozilla

NS_IMETHODIMP nsPrinterWin::CopyFromWithValidation(
    nsIPrintSettings* aSettingsToCopyFrom, JSContext* aCx,
    Promise** aResultPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aResultPromise);

  PrintSettingsInitializer settingsInitializer =
      aSettingsToCopyFrom->GetSettingsInitializer();
  return PrintBackgroundTaskPromise(
      *this, aCx, aResultPromise, "CopyFromWithValidation"_ns,
      &nsPrinterWin::GetValidatedSettings, settingsInitializer);
}

bool nsPrinterWin::SupportsDuplex() const {
  MutexAutoLock autoLock(mDriverMutex);
  return ::DeviceCapabilitiesW(mName.get(), nullptr, DC_DUPLEX, nullptr,
                               nullptr) == 1;
}

bool nsPrinterWin::SupportsColor() const {
  MutexAutoLock autoLock(mDriverMutex);
  return ::DeviceCapabilitiesW(mName.get(), nullptr, DC_COLORDEVICE, nullptr,
                               nullptr) == 1;
}

bool nsPrinterWin::SupportsMonochrome() const {
  if (!SupportsColor()) {
    return true;
  }

  nsHPRINTER hPrinter = nullptr;
  if (NS_WARN_IF(!::OpenPrinterW(mName.get(), &hPrinter, nullptr))) {
    return false;
  }
  nsAutoPrinter autoPrinter(hPrinter);

  nsTArray<uint8_t> devmodeWStorage = CopyDefaultDevmodeW();
  if (devmodeWStorage.IsEmpty()) {
    return false;
  }

  auto* devmode = reinterpret_cast<DEVMODEW*>(devmodeWStorage.Elements());

  devmode->dmFields |= DM_COLOR;
  devmode->dmColor = DMCOLOR_MONOCHROME;
  // Try to modify the devmode settings and see if the setting sticks.
  //
  // This has been the only reliable way to detect it that we've found.
  MutexAutoLock autoLock(mDriverMutex);
  LONG ret =
      ::DocumentPropertiesW(nullptr, autoPrinter.get(), mName.get(), devmode,
                            devmode, DM_IN_BUFFER | DM_OUT_BUFFER);
  if (ret != IDOK) {
    return false;
  }
  return !(devmode->dmFields & DM_COLOR) ||
         devmode->dmColor == DMCOLOR_MONOCHROME;
}

bool nsPrinterWin::SupportsCollation() const {
  MutexAutoLock autoLock(mDriverMutex);
  return ::DeviceCapabilitiesW(mName.get(), nullptr, DC_COLLATE, nullptr,
                               nullptr) == 1;
}

nsPrinterBase::PrinterInfo nsPrinterWin::CreatePrinterInfo() const {
  return PrinterInfo{PaperList(), DefaultSettings()};
}

mozilla::gfx::MarginDouble nsPrinterWin::GetMarginsForPaper(
    nsString aPaperId) const {
  gfx::MarginDouble margin;

  nsTArray<uint8_t> devmodeWStorage = CopyDefaultDevmodeW();
  if (devmodeWStorage.IsEmpty()) {
    return margin;
  }

  auto* devmode = reinterpret_cast<DEVMODEW*>(devmodeWStorage.Elements());

  devmode->dmFields = DM_PAPERSIZE;
  devmode->dmPaperSize = _wtoi((const wchar_t*)aPaperId.BeginReading());
  HDC dc;
  {
    MutexAutoLock autoLock(mDriverMutex);
    dc = ::CreateICW(nullptr, mName.get(), nullptr, devmode);
  }
  nsAutoHDC printerDc(dc);
  MOZ_ASSERT(printerDc, "CreateICW failed");
  if (!printerDc) {
    return margin;
  }
  margin = WinUtils::GetUnwriteableMarginsForDeviceInInches(printerDc);
  margin.top *= kPointsPerInch;
  margin.right *= kPointsPerInch;
  margin.bottom *= kPointsPerInch;
  margin.left *= kPointsPerInch;

  return margin;
}

nsTArray<uint8_t> nsPrinterWin::CopyDefaultDevmodeW() const {
  nsTArray<uint8_t> devmodeStorageW;

  auto devmodeStorageWLock = mDefaultDevmodeWStorage.Lock();
  if (devmodeStorageWLock->IsEmpty()) {
    nsHPRINTER hPrinter = nullptr;
    // OpenPrinter could fail if, for example, the printer has been removed
    // or otherwise become inaccessible since it was selected.
    if (NS_WARN_IF(!::OpenPrinterW(mName.get(), &hPrinter, nullptr))) {
      return devmodeStorageW;
    }
    nsAutoPrinter autoPrinter(hPrinter);
    // Allocate devmode storage of the correct size.
    MutexAutoLock autoLock(mDriverMutex);
    LONG bytesNeeded = ::DocumentPropertiesW(nullptr, autoPrinter.get(),
                                             mName.get(), nullptr, nullptr, 0);
    // Note that we must cast the sizeof() to a signed type so that comparison
    // with the signed, potentially-negative bytesNeeded will work!
    MOZ_ASSERT(bytesNeeded >= LONG(sizeof(DEVMODEW)),
               "DocumentPropertiesW failed to get valid size");
    if (bytesNeeded < LONG(sizeof(DEVMODEW))) {
      return devmodeStorageW;
    }

    // Allocate extra space in case of bad drivers that return a too-small
    // result from DocumentProperties.
    // (See https://bugzilla.mozilla.org/show_bug.cgi?id=1664530#c5)
    if (!devmodeStorageWLock->SetLength(bytesNeeded * 2, fallible)) {
      return devmodeStorageW;
    }

    memset(devmodeStorageWLock->Elements(), 0, devmodeStorageWLock->Length());
    auto* devmode =
        reinterpret_cast<DEVMODEW*>(devmodeStorageWLock->Elements());
    LONG ret = ::DocumentPropertiesW(nullptr, autoPrinter.get(), mName.get(),
                                     devmode, nullptr, DM_OUT_BUFFER);
    MOZ_ASSERT(ret == IDOK, "DocumentPropertiesW failed");
    // Make sure that the lengths in the DEVMODEW make sense.
    if (ret != IDOK || devmode->dmSize != sizeof(DEVMODEW) ||
        devmode->dmSize + devmode->dmDriverExtra >
            devmodeStorageWLock->Length()) {
      // Clear mDefaultDevmodeWStorage to make sure we try again next time.
      devmodeStorageWLock->Clear();
      return devmodeStorageW;
    }
  }

  devmodeStorageW.Assign(devmodeStorageWLock.ref());
  return devmodeStorageW;
}

nsTArray<mozilla::PaperInfo> nsPrinterWin::PaperList() const {
  // Paper IDs are returned as WORDs.
  int requiredArrayCount = 0;
  auto paperIds = GetDeviceCapabilityArray<WORD>(
      mName.get(), DC_PAPERS, mDriverMutex, requiredArrayCount);
  if (!paperIds.Length()) {
    return {};
  }

  // Paper names are returned in 64 long character buffers.
  auto paperNames = GetDeviceCapabilityArray<Array<wchar_t, 64>>(
      mName.get(), DC_PAPERNAMES, mDriverMutex, requiredArrayCount);
  // Check that we have the same number of names as IDs.
  if (paperNames.Length() != paperIds.Length()) {
    return {};
  }

  // Paper sizes are returned as POINT structs with a tenth of a millimeter as
  // the unit.
  auto paperSizes = GetDeviceCapabilityArray<POINT>(
      mName.get(), DC_PAPERSIZE, mDriverMutex, requiredArrayCount);
  // Check that we have the same number of sizes as IDs.
  if (paperSizes.Length() != paperIds.Length()) {
    return {};
  }

  nsTArray<mozilla::PaperInfo> paperList;
  paperList.SetCapacity(paperNames.Length());
  for (size_t i = 0; i < paperNames.Length(); ++i) {
    // Paper names are null terminated unless they are 64 characters long.
    auto firstNull =
        std::find(paperNames[i].cbegin(), paperNames[i].cend(), L'\0');
    auto nameLength = firstNull - paperNames[i].cbegin();
    double width = paperSizes[i].x * kPointsPerTenthMM;
    double height = paperSizes[i].y * kPointsPerTenthMM;

    // Skip if no name or invalid size.
    if (!nameLength || width <= 0 || height <= 0) {
      continue;
    }

    // Windows paper IDs are 16-bit integers; we stringify them to store in the
    // PaperInfo.mId field.
    nsString paperIdString;
    paperIdString.AppendInt(paperIds[i]);

    // We don't resolve the margins eagerly because they're really expensive (on
    // the order of seconds for some drivers).
    nsDependentSubstring name(paperNames[i].cbegin(), nameLength);
    paperList.AppendElement(mozilla::PaperInfo(paperIdString, nsString(name),
                                               {width, height}, Nothing()));
  }

  return paperList;
}

PrintSettingsInitializer nsPrinterWin::DefaultSettings() const {
  nsTArray<uint8_t> devmodeWStorage = CopyDefaultDevmodeW();
  if (devmodeWStorage.IsEmpty()) {
    return {};
  }

  const auto* devmode =
      reinterpret_cast<const DEVMODEW*>(devmodeWStorage.Elements());

  PrintSettingsInitializer settingsInitializer;
  DevmodeToSettingsInitializer(mName, devmode, mDriverMutex,
                               settingsInitializer);
  settingsInitializer.mDevmodeWStorage = std::move(devmodeWStorage);
  return settingsInitializer;
}

PrintSettingsInitializer nsPrinterWin::GetValidatedSettings(
    PrintSettingsInitializer aSettingsToValidate) const {
  // This function validates the settings by relying on the printer driver
  // rejecting any invalid settings and resetting them to valid values.

  // Create a copy of the default DEVMODE for this printer.
  nsTArray<uint8_t> devmodeWStorage = CopyDefaultDevmodeW();
  if (devmodeWStorage.IsEmpty()) {
    return aSettingsToValidate;
  }

  nsHPRINTER hPrinter = nullptr;
  if (NS_WARN_IF(!::OpenPrinterW(mName.get(), &hPrinter, nullptr))) {
    return aSettingsToValidate;
  }
  nsAutoPrinter autoPrinter(hPrinter);

  // Copy the settings from aSettingsToValidate into our DEVMODE.
  DEVMODEW* devmode = reinterpret_cast<DEVMODEW*>(devmodeWStorage.Elements());
  if (!aSettingsToValidate.mPaperInfo.mId.IsEmpty()) {
    devmode->dmPaperSize = _wtoi(
        (const wchar_t*)aSettingsToValidate.mPaperInfo.mId.BeginReading());
    devmode->dmFields |= DM_PAPERSIZE;
  } else {
    devmode->dmPaperSize = 0;
    devmode->dmFields &= ~DM_PAPERSIZE;
  }

  devmode->dmFields |= DM_COLOR;
  devmode->dmColor =
      aSettingsToValidate.mPrintInColor ? DMCOLOR_COLOR : DMCOLOR_MONOCHROME;

  // Note: small page sizes can be required here for sticker, label and slide
  // printers etc. see bug 1271900.
  if (aSettingsToValidate.mPaperInfo.mSize.height > 0) {
    devmode->dmPaperLength = std::round(
        aSettingsToValidate.mPaperInfo.mSize.height / kPointsPerTenthMM);
    devmode->dmFields |= DM_PAPERLENGTH;
  } else {
    devmode->dmPaperLength = 0;
    devmode->dmFields &= ~DM_PAPERLENGTH;
  }

  if (aSettingsToValidate.mPaperInfo.mSize.width > 0) {
    devmode->dmPaperWidth = std::round(
        aSettingsToValidate.mPaperInfo.mSize.width / kPointsPerTenthMM);
    devmode->dmFields |= DM_PAPERWIDTH;
  } else {
    devmode->dmPaperWidth = 0;
    devmode->dmFields &= ~DM_PAPERWIDTH;
  }

  // Setup Orientation
  devmode->dmOrientation = aSettingsToValidate.mSheetOrientation ==
                                   nsPrintSettings::kPortraitOrientation
                               ? DMORIENT_PORTRAIT
                               : DMORIENT_LANDSCAPE;
  devmode->dmFields |= DM_ORIENTATION;

  // Setup Number of Copies
  devmode->dmCopies = aSettingsToValidate.mNumCopies;
  devmode->dmFields |= DM_COPIES;

  // Setup Simplex/Duplex mode
  devmode->dmFields |= DM_DUPLEX;
  switch (aSettingsToValidate.mDuplex) {
    case nsPrintSettings::kDuplexNone:
      devmode->dmDuplex = DMDUP_SIMPLEX;
      break;
    case nsPrintSettings::kDuplexFlipOnLongEdge:
      devmode->dmDuplex = DMDUP_VERTICAL;
      break;
    case nsPrintSettings::kDuplexFlipOnShortEdge:
      devmode->dmDuplex = DMDUP_HORIZONTAL;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("bad value for duplex option");
      break;
  }

  // Apply the settings in the DEVMODE to the printer and retrieve the updated
  // DEVMODE back into the same structure.
  LONG ret;
  {
    MutexAutoLock autoLock(mDriverMutex);
    ret = ::DocumentPropertiesW(nullptr, autoPrinter.get(), mName.get(),
                                devmode, devmode, DM_IN_BUFFER | DM_OUT_BUFFER);
  }
  if (ret != IDOK) {
    return aSettingsToValidate;
  }

  // Copy the settings back from the DEVMODE into aSettingsToValidate and
  // return.
  DevmodeToSettingsInitializer(mName, devmode, mDriverMutex,
                               aSettingsToValidate);
  aSettingsToValidate.mDevmodeWStorage = std::move(devmodeWStorage);
  return aSettingsToValidate;
}
