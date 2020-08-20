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

PrintSettingsInitializer nsPrinterWin::DefaultSettings() const {
  nsAutoPrinter autoPrinter(nsHPRINTER(nullptr));
  BOOL status = ::OpenPrinterW(mName.get(), &autoPrinter.get(), nullptr);
  if (NS_WARN_IF(!status)) {
    return PrintSettingsInitializer();
  }

  // Allocate devmode storage of the correct size.
  LONG bytesNeeded = ::DocumentPropertiesW(nullptr, autoPrinter.get(),
                                           mName.get(), nullptr, nullptr, 0);
  if (NS_WARN_IF(bytesNeeded < 0)) {
    return PrintSettingsInitializer();
  }

  nsTArray<uint8_t> devmodeWStorage(bytesNeeded);
  devmodeWStorage.SetLength(bytesNeeded);
  DEVMODEW* devmode = reinterpret_cast<DEVMODEW*>(devmodeWStorage.Elements());
  LONG ret = ::DocumentPropertiesW(nullptr, autoPrinter.get(), mName.get(),
                                   devmode, nullptr, DM_OUT_BUFFER);
  if (NS_WARN_IF(ret != IDOK)) {
    return PrintSettingsInitializer();
  }

  nsAutoHDC printerDc(::CreateICW(nullptr, mName.get(), nullptr, devmode));
  if (NS_WARN_IF(!printerDc)) {
    return PrintSettingsInitializer();
  }

  nsString paperName;
  SizeDouble paperSize;
  for (auto paperInfo : PaperList()) {
    if (paperInfo.mPaperId == devmode->dmPaperSize) {
      paperName.Assign(paperInfo.mName);
      paperSize = paperInfo.mSize;
      break;
    }
  }

  auto margin = WinUtils::GetUnwriteableMarginsForDeviceInInches(printerDc);
  margin.top *= POINTS_PER_INCH_FLOAT;
  margin.right *= POINTS_PER_INCH_FLOAT;
  margin.bottom *= POINTS_PER_INCH_FLOAT;
  margin.left *= POINTS_PER_INCH_FLOAT;

  // Using Y to match existing code for print scaling calculations.
  int resolution = GetDeviceCaps(printerDc, LOGPIXELSY);

  return PrintSettingsInitializer{mName,
                                  PaperInfo(paperName, paperSize, Some(margin)),
                                  devmode->dmColor == DMCOLOR_COLOR, resolution,
                                  std::move(devmodeWStorage)};
}

template <class T>
static nsTArray<T> GetDeviceCapabilityArray(const LPWSTR aPrinterName,
                                            WORD aCapabilityID) {
  nsTArray<T> caps;

  // We only want to access printer drivers in the parent process.
  if (!XRE_IsParentProcess()) {
    return caps;
  }

  // Passing nullptr as the port here seems to work. Given that we would have to
  // OpenPrinter with just the name anyway to get the port that makes sense.
  // Also, the printer set-up seems to stop you from having two printers with
  // the same name.
  // Note: this (and the call below) are blocking calls, which could be slow.
  int count = ::DeviceCapabilitiesW(aPrinterName, nullptr, aCapabilityID,
                                    nullptr, nullptr);
  if (count <= 0) {
    return caps;
  }

  caps.SetLength(count);
  count =
      ::DeviceCapabilitiesW(aPrinterName, nullptr, aCapabilityID,
                            reinterpret_cast<LPWSTR>(caps.Elements()), nullptr);
  if (count <= 0) {
    caps.Clear();
    return caps;
  }

  MOZ_DIAGNOSTIC_ASSERT(
      count <= caps.Length(),
      "DeviceCapabilitiesW returned more than buffer could hold.");

  caps.SetLength(count);
  return caps;
}

NS_IMETHODIMP
nsPrinterWin::GetName(nsAString& aName) {
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

bool nsPrinterWin::SupportsCollation() const {
  return ::DeviceCapabilitiesW(mName.get(), nullptr, DC_COLLATE, nullptr,
                               nullptr) == 1;
}

nsTArray<mozilla::PaperInfo> nsPrinterWin::PaperList() const {
  // Paper names are returned in 64 long character buffers.
  auto paperNames =
      GetDeviceCapabilityArray<Array<wchar_t, 64>>(mName.get(), DC_PAPERNAMES);

  // Paper IDs are returned as WORDs.
  auto paperIds = GetDeviceCapabilityArray<WORD>(mName.get(), DC_PAPERS);

  // Paper sizes are returned as POINT structs with a tenth of a millimeter as
  // the unit.
  auto paperSizes = GetDeviceCapabilityArray<POINT>(mName.get(), DC_PAPERSIZE);

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

    // We don't resolve the margins eagerly because they're really expensive (on
    // the order of seconds for some drivers).
    nsDependentSubstring name(paperNames[i].cbegin(), nameLength);
    paperList.AppendElement(mozilla::PaperInfo(nsString(name), {width, height},
                                               Nothing(), paperIds[i]));
  }

  return paperList;
}

mozilla::gfx::MarginDouble nsPrinterWin::GetMarginsForPaper(
    uint64_t aPaperId) const {
  static const wchar_t kDriverName[] = L"WINSPOOL";
  // Now get the margin info.
  // We need a DEVMODE to set the paper size on the context.
  DEVMODEW devmode = {};
  devmode.dmSize = sizeof(DEVMODEW);
  devmode.dmFields = DM_PAPERSIZE;
  devmode.dmPaperSize = aPaperId;

  // Create an information context, so that we can get margin information.
  // Note: this blocking call interacts with the driver and can be slow.
  nsAutoHDC printerDc(::CreateICW(kDriverName, mName.get(), nullptr, &devmode));

  auto margin = WinUtils::GetUnwriteableMarginsForDeviceInInches(printerDc);
  margin.top *= POINTS_PER_INCH_FLOAT;
  margin.right *= POINTS_PER_INCH_FLOAT;
  margin.bottom *= POINTS_PER_INCH_FLOAT;
  margin.left *= POINTS_PER_INCH_FLOAT;
  return margin;
}
