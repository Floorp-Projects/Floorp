/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterWin.h"

#include <algorithm>
#include <windows.h>

#include "mozilla/Array.h"
#include "nsPaper.h"

using namespace mozilla;
using namespace mozilla::gfx;

static const float kTenthMMToPoint =
    (POINTS_PER_INCH_FLOAT / MM_PER_INCH_FLOAT) / 10;

nsPrinterWin::nsPrinterWin(const nsAString& aName) : mName(aName) {}

// static
already_AddRefed<nsPrinterWin> nsPrinterWin::Create(const nsAString& aName) {
  return do_AddRef(new nsPrinterWin(aName));
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

  static const wchar_t kDriverName[] = L"WINSPOOL";
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

    WORD id = paperIds[i];

    // Now get the margin info.
    // We need a DEVMODE to set the paper size on the context.
    DEVMODEW devmode = {};
    devmode.dmSize = sizeof(DEVMODEW);
    devmode.dmFields = DM_PAPERSIZE;
    devmode.dmPaperSize = id;

    // Create an information context, so that we can get margin information.
    // Note: this blocking call interacts with the driver and can be slow.
    nsAutoHDC printerDc(
        ::CreateICW(kDriverName, mName.get(), nullptr, &devmode));

    auto marginInInches =
        WinUtils::GetUnwriteableMarginsForDeviceInInches(printerDc);

    nsDependentSubstring name(paperNames[i].cbegin(), nameLength);
    paperList.AppendElement(mozilla::PaperInfo{
        nsString(name),
        width,
        height,
        marginInInches.top * POINTS_PER_INCH_FLOAT,
        marginInInches.right * POINTS_PER_INCH_FLOAT,
        marginInInches.bottom * POINTS_PER_INCH_FLOAT,
        marginInInches.left * POINTS_PER_INCH_FLOAT,
    });
  }

  return paperList;
}
