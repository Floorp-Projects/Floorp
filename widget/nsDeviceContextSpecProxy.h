/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceContextSpecProxy_h
#define nsDeviceContextSpecProxy_h

#include "nsIDeviceContextSpec.h"
#include "nsCOMPtr.h"

class nsIPrintSession;

namespace mozilla {
namespace gfx {
class DrawEventRecorderMemory;
}

namespace layout {
class RemotePrintJobChild;
}
}

class nsDeviceContextSpecProxy final : public nsIDeviceContextSpec
{
public:
  NS_DECL_ISUPPORTS

  NS_METHOD Init(nsIWidget* aWidget, nsIPrintSettings* aPrintSettings,
                 bool aIsPrintPreview) final;

  virtual already_AddRefed<PrintTarget> MakePrintTarget() final;

  NS_METHOD GetDrawEventRecorder(mozilla::gfx::DrawEventRecorder** aDrawEventRecorder) final;

  float GetDPI() final;

  float GetPrintingScale() final;

  NS_METHOD BeginDocument(const nsAString& aTitle,
                          const nsAString& aPrintToFileName,
                          int32_t aStartPage, int32_t aEndPage) final;

  NS_METHOD EndDocument() final;

  NS_METHOD AbortDocument() final;

  NS_METHOD BeginPage() final;

  NS_METHOD EndPage() final;

private:
  ~nsDeviceContextSpecProxy() {}

  nsCOMPtr<nsIPrintSettings> mPrintSettings;
  nsCOMPtr<nsIPrintSession> mPrintSession;
  nsCOMPtr<nsIDeviceContextSpec> mRealDeviceContextSpec;
  RefPtr<mozilla::layout::RemotePrintJobChild> mRemotePrintJob;
  RefPtr<mozilla::gfx::DrawEventRecorderMemory> mRecorder;
};

#endif // nsDeviceContextSpecProxy_h
