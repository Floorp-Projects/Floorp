/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrinterWin.h"

#include <algorithm>
#include <windows.h>

#include "mozilla/Array.h"
#include "nsPaperWin.h"

using namespace mozilla;
using namespace mozilla::gfx;

static const float kTenthMMToPoint =
    (POINTS_PER_INCH_FLOAT / MM_PER_INCH_FLOAT) / 10;

nsPrinterWin::nsPrinterWin(const nsAString& aName) : mName(aName) {}

NS_IMPL_ISUPPORTS(nsPrinterWin, nsIPrinter);

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

nsresult nsPrinterWin::EnsurePaperList() {
  if (!mPaperList.IsEmpty()) {
    return NS_OK;
  }

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
    return NS_ERROR_UNEXPECTED;
  }

  mPaperList.SetCapacity(paperNames.Length());
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

    nsAutoString paperName(paperNames[i].cbegin(), nameLength);
    mPaperList.AppendElement(
        new nsPaperWin(paperIds[i], paperName, mName, width, height));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrinterWin::GetName(nsAString& aName) {
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterWin::GetPaperList(nsTArray<RefPtr<nsIPaper>>& aPaperList) {
  nsresult rv = EnsurePaperList();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aPaperList.Assign(mPaperList);
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterWin::GetSupportsDuplex(bool* aSupportsDuplex) {
  MOZ_ASSERT(aSupportsDuplex);

  if (mSupportsDuplex.isNothing()) {
    // Note: this is a blocking call, which could be slow.
    mSupportsDuplex =
        Some(::DeviceCapabilitiesW(mName.get(), nullptr, DC_DUPLEX, nullptr,
                                   nullptr) == 1);
  }

  *aSupportsDuplex = mSupportsDuplex.value();
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterWin::GetSupportsColor(bool* aSupportsColor) {
  MOZ_ASSERT(aSupportsColor);

  if (mSupportsColor.isNothing()) {
    // Note: this is a blocking call, which could be slow.
    mSupportsColor =
        Some(::DeviceCapabilitiesW(mName.get(), nullptr, DC_COLORDEVICE,
                                   nullptr, nullptr) == 1);
  }

  *aSupportsColor = mSupportsColor.value();
  return NS_OK;
}
