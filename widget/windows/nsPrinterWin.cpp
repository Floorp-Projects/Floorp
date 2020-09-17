/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterWin.h"

#include <algorithm>
#include <windows.h>
#include <winspool.h>

#include "mozilla/Array.h"
#include "nsPaper.h"
#include "nsPrintSettingsImpl.h"
#include "nsWindowsHelpers.h"
#include "WinUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

static const double kTenthMMToPoint = 72.0 / 254.0;

nsPrinterWin::nsPrinterWin(const nsAString& aName) : mName(aName) {}

// static
already_AddRefed<nsPrinterWin> nsPrinterWin::Create(const nsAString& aName) {
  return do_AddRef(new nsPrinterWin(aName));
}

template <typename Callback>
static bool WithDefaultDevMode(const nsString& aName,
                               nsTArray<uint8_t>& aStorage,
                               Callback&& aCallback) {
  nsHPRINTER hPrinter = nullptr;
  BOOL status = ::OpenPrinterW(aName.get(), &hPrinter, nullptr);
  MOZ_DIAGNOSTIC_ASSERT(status, "OpenPrinterW failed");
  if (!status) {
    return false;
  }
  nsAutoPrinter autoPrinter(hPrinter);
  // Allocate devmode storage of the correct size.
  LONG bytesNeeded = ::DocumentPropertiesW(nullptr, autoPrinter.get(),
                                           aName.get(), nullptr, nullptr, 0);
  MOZ_DIAGNOSTIC_ASSERT(bytesNeeded >= sizeof(DEVMODEW),
                        "DocumentPropertiesW failed to get valid size");
  if (bytesNeeded < sizeof(DEVMODEW)) {
    return false;
  }

  // Allocate extra space in case of bad drivers that return a too-small
  // result from DocumentProperties.
  // (See https://bugzilla.mozilla.org/show_bug.cgi?id=1664530#c5)
  aStorage.SetLength(bytesNeeded * 2);
  auto* devmode = reinterpret_cast<DEVMODEW*>(aStorage.Elements());
  LONG ret = ::DocumentPropertiesW(nullptr, autoPrinter.get(), aName.get(),
                                   devmode, nullptr, DM_OUT_BUFFER);
  MOZ_DIAGNOSTIC_ASSERT(ret == IDOK, "DocumentPropertiesW failed");
  if (ret != IDOK) {
    return false;
  }

  return aCallback(autoPrinter.get(), devmode);
}

PrintSettingsInitializer nsPrinterWin::DefaultSettings() const {
  // Initialize to something reasonable, in case we fail to get usable data
  // from the devmode below.
  nsString paperName(u"Letter");
  nsString paperIdString(u"1");  // DMPAPER_LETTER
  SizeDouble paperSize{8.5 * 72.0, 11.0 * 72.0};
  gfx::MarginDouble margin;
  int resolution = 0;
  bool color = true;

  nsTArray<uint8_t> devmodeWStorage;
  bool success = WithDefaultDevMode(
      mName, devmodeWStorage, [&](HANDLE, DEVMODEW* devmode) {
        // XXX If DM_PAPERSIZE is not set, or is not a known value, should we
        // be returning a "Custom" name of some kind?
        if (devmode->dmFields & DM_PAPERSIZE) {
          paperIdString.Truncate(0);
          paperIdString.AppendInt(devmode->dmPaperSize);
          for (auto paperInfo : PaperList()) {
            if (paperIdString.Equals(paperInfo.mId)) {
              paperName.Assign(paperInfo.mName);
              paperSize = paperInfo.mSize;
              break;
            }
          }
        }

        nsAutoHDC printerDc(
            ::CreateICW(nullptr, mName.get(), nullptr, devmode));
        MOZ_DIAGNOSTIC_ASSERT(printerDc, "CreateICW failed");
        if (!printerDc) {
          return false;
        }

        margin = WinUtils::GetUnwriteableMarginsForDeviceInInches(printerDc);
        margin.top *= POINTS_PER_INCH_FLOAT;
        margin.right *= POINTS_PER_INCH_FLOAT;
        margin.bottom *= POINTS_PER_INCH_FLOAT;
        margin.left *= POINTS_PER_INCH_FLOAT;

        // Using Y to match existing code for print scaling calculations.
        resolution = GetDeviceCaps(printerDc, LOGPIXELSY);
        if (devmode->dmFields & DM_COLOR) {
          // See comment for PrintSettingsInitializer.mPrintInColor
          color = devmode->dmColor != DMCOLOR_MONOCHROME;
        }
        return true;
      });

  if (!success) {
    return {};
  }

  return PrintSettingsInitializer{
      mName, PaperInfo(paperIdString, paperName, paperSize, Some(margin)),
      color, resolution, std::move(devmodeWStorage)};
}

template <class T>
static nsTArray<T> GetDeviceCapabilityArray(const LPWSTR aPrinterName,
                                            WORD aCapabilityID,
                                            size_t aCount = 0) {
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
  int count;
  if (aCount) {
    count = aCount;
  } else {
    // Passing nullptr as the port here seems to work. Given that we would have
    // to OpenPrinter with just the name anyway to get the port that makes
    // sense. Also, the printer set-up seems to stop you from having two
    // printers with the same name. Note: this (and the call below) are blocking
    // calls, which could be slow.
    count = ::DeviceCapabilitiesW(aPrinterName, nullptr, aCapabilityID, nullptr,
                                  nullptr);
    if (count <= 0) {
      return caps;
    }
  }

  // As DeviceCapabilitiesW doesn't take a size, there is a greater risk of the
  // buffer being overflowed, so we over-allocate for safety.
  caps.SetLength(count * 2);
  count =
      ::DeviceCapabilitiesW(aPrinterName, nullptr, aCapabilityID,
                            reinterpret_cast<LPWSTR>(caps.Elements()), nullptr);
  if (count <= 0) {
    caps.Clear();
    return caps;
  }

  // Note that TruncateLength will crash if count > caps.Length().
  caps.TruncateLength(count);
  return caps;
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

bool nsPrinterWin::SupportsDuplex() const {
  return ::DeviceCapabilitiesW(mName.get(), nullptr, DC_DUPLEX, nullptr,
                               nullptr) == 1;
}

bool nsPrinterWin::SupportsColor() const {
  return ::DeviceCapabilitiesW(mName.get(), nullptr, DC_COLORDEVICE, nullptr,
                               nullptr) == 1;
}

bool nsPrinterWin::SupportsMonochrome() const {
  if (!SupportsColor()) {
    return true;
  }
  nsTArray<uint8_t> storage;
  return WithDefaultDevMode(
      mName, storage, [&](HANDLE aPrinter, DEVMODEW* aDevMode) {
        aDevMode->dmFields |= DM_COLOR;
        aDevMode->dmColor = DMCOLOR_MONOCHROME;
        // Try to modify the devmode settings and see if the setting sticks.
        //
        // This has been the only reliable way to detect it that we've found.
        LONG ret =
            ::DocumentPropertiesW(nullptr, aPrinter, mName.get(), aDevMode,
                                  aDevMode, DM_IN_BUFFER | DM_OUT_BUFFER);
        if (ret != IDOK) {
          return false;
        }
        return !(aDevMode->dmFields & DM_COLOR) ||
               aDevMode->dmColor == DMCOLOR_MONOCHROME;
      });
}

bool nsPrinterWin::SupportsCollation() const {
  return ::DeviceCapabilitiesW(mName.get(), nullptr, DC_COLLATE, nullptr,
                               nullptr) == 1;
}

nsTArray<mozilla::PaperInfo> nsPrinterWin::PaperList() const {
  // Paper IDs are returned as WORDs.
  auto paperIds = GetDeviceCapabilityArray<WORD>(mName.get(), DC_PAPERS);

  // Paper names are returned in 64 long character buffers.
  auto paperNames = GetDeviceCapabilityArray<Array<wchar_t, 64>>(
      mName.get(), DC_PAPERNAMES, paperIds.Length());

  // Paper sizes are returned as POINT structs with a tenth of a millimeter as
  // the unit.
  auto paperSizes = GetDeviceCapabilityArray<POINT>(mName.get(), DC_PAPERSIZE,
                                                    paperIds.Length());

  // Check that we have papers and that the array lengths match.
  if (!paperNames.Length() || paperNames.Length() != paperIds.Length() ||
      paperNames.Length() != paperSizes.Length()) {
    return {};
  }

  nsTArray<mozilla::PaperInfo> paperList;
  paperList.SetCapacity(paperNames.Length());
  for (size_t i = 0; i < paperNames.Length(); ++i) {
    // Paper names are null terminated unless they are 64 characters long.
    auto firstNull =
        std::find(paperNames[i].cbegin(), paperNames[i].cend(), L'\0');
    auto nameLength = firstNull - paperNames[i].cbegin();
    double width = paperSizes[i].x * kTenthMMToPoint;
    double height = paperSizes[i].y * kTenthMMToPoint;

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

mozilla::gfx::MarginDouble nsPrinterWin::GetMarginsForPaper(
    nsString aPaperId) const {
  gfx::MarginDouble margin;

  nsTArray<uint8_t> storage;
  bool success =
      WithDefaultDevMode(mName, storage, [&](HANDLE, DEVMODEW* devmode) {
        devmode->dmFields = DM_PAPERSIZE;
        devmode->dmPaperSize = _wtoi((const wchar_t*)aPaperId.BeginReading());
        nsAutoHDC printerDc(
            ::CreateICW(nullptr, mName.get(), nullptr, devmode));
        MOZ_DIAGNOSTIC_ASSERT(printerDc, "CreateICW failed");
        if (!printerDc) {
          return false;
        }
        margin = WinUtils::GetUnwriteableMarginsForDeviceInInches(printerDc);
        margin.top *= POINTS_PER_INCH_FLOAT;
        margin.right *= POINTS_PER_INCH_FLOAT;
        margin.bottom *= POINTS_PER_INCH_FLOAT;
        margin.left *= POINTS_PER_INCH_FLOAT;
        return true;
      });

  if (!success) {
    return {};
  }

  return margin;
}
