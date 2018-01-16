/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceContextSpecProxy_h
#define nsDeviceContextSpecProxy_h

#include "nsIDeviceContextSpec.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "mozilla/layout/printing/DrawEventRecorder.h"

class nsIFile;
class nsIPrintSession;
class nsIUUIDGenerator;

namespace mozilla {
namespace layout {
class RemotePrintJobChild;
}
}

class nsDeviceContextSpecProxy final : public nsIDeviceContextSpec
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIWidget* aWidget, nsIPrintSettings* aPrintSettings,
                 bool aIsPrintPreview) final override;

  virtual already_AddRefed<PrintTarget> MakePrintTarget() final override;

  NS_IMETHOD GetDrawEventRecorder(mozilla::gfx::DrawEventRecorder** aDrawEventRecorder) final override;

  float GetDPI() final override;

  float GetPrintingScale() final override;


  NS_IMETHOD BeginDocument(const nsAString& aTitle,
                           const nsAString& aPrintToFileName,
                           int32_t aStartPage, int32_t aEndPage) final override;

  NS_IMETHOD EndDocument() final override;

  NS_IMETHOD AbortDocument() final override;

  NS_IMETHOD BeginPage() final override;

  NS_IMETHOD EndPage() final override;

private:
  ~nsDeviceContextSpecProxy() {}

  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  nsCOMPtr<nsIPrintSession> mPrintSession;
  nsCOMPtr<nsIDeviceContextSpec> mRealDeviceContextSpec;
  RefPtr<mozilla::layout::RemotePrintJobChild> mRemotePrintJob;
  RefPtr<mozilla::layout::DrawEventRecorderPRFileDesc> mRecorder;
};

#endif // nsDeviceContextSpecProxy_h
