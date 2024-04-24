/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDeviceContextSpec.h"

#include "gfxPoint.h"
#include "mozilla/gfx/PrintPromise.h"
#include "nsError.h"
#include "nsIPrintSettings.h"

#include "mozilla/Components.h"
#include "mozilla/TaskQueue.h"

using mozilla::MakeRefPtr;
using mozilla::gfx::PrintEndDocumentPromise;

// We have some platform specific code here rather than in the appropriate
// nsIDeviceContextSpec subclass. We structure the code this way so that
// nsIDeviceContextSpecProxy gets the correct behavior without us having to
// instantiate a platform specific nsIDeviceContextSpec subclass in content
// processes. That is necessary for sandboxing.

float nsIDeviceContextSpec::GetPrintingScale() {
#ifdef XP_WIN
  if (mPrintSettings->GetOutputFormat() != nsIPrintSettings::kOutputFormatPDF
#  ifdef MOZ_ENABLE_SKIA_PDF
      && !mPrintViaSkPDF
#  endif
  ) {
    // The print settings will have the resolution stored from the real device.
    int32_t resolution;
    mPrintSettings->GetResolution(&resolution);
    return float(resolution) / GetDPI();
  }
#endif

  return 72.0f / GetDPI();
}

gfxPoint nsIDeviceContextSpec::GetPrintingTranslate() {
#ifdef XP_WIN
  // The underlying surface on windows is the size of the printable region. When
  // the region is smaller than the actual paper size the (0, 0) coordinate
  // refers top-left of that unwritable region. To instead have (0, 0) become
  // the top-left of the actual paper, translate it's coordinate system by the
  // unprintable region's width.
  double marginTop, marginLeft;
  mPrintSettings->GetUnwriteableMarginTop(&marginTop);
  mPrintSettings->GetUnwriteableMarginLeft(&marginLeft);
  int32_t resolution;
  mPrintSettings->GetResolution(&resolution);
  return gfxPoint(-marginLeft * resolution, -marginTop * resolution);
#else
  return gfxPoint(0, 0);
#endif
}

RefPtr<PrintEndDocumentPromise>
nsIDeviceContextSpec::EndDocumentPromiseFromResult(
    nsresult aResult, mozilla::StaticString aSite) {
  return NS_SUCCEEDED(aResult)
             ? PrintEndDocumentPromise::CreateAndResolve(true, aSite)
             : PrintEndDocumentPromise::CreateAndReject(aResult, aSite);
}

RefPtr<PrintEndDocumentPromise> nsIDeviceContextSpec::EndDocumentAsync(
    const char* aCallSite, AsyncEndDocumentFunction aFunction) {
  auto promise =
      MakeRefPtr<PrintEndDocumentPromise::Private>("PrintEndDocumentPromise");

  NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(
          "EndDocumentAsync",
          [promise, function = std::move(aFunction)]() mutable {
            const auto result = function();
            if (NS_SUCCEEDED(result)) {
              promise->Resolve(true, __func__);
            } else {
              promise->Reject(result, __func__);
            }
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  return promise;
}
