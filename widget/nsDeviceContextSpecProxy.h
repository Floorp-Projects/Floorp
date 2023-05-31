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
#include "mozilla/gfx/PrintPromise.h"

class nsIFile;
class nsIUUIDGenerator;

namespace mozilla {
namespace layout {
class RemotePrintJobChild;
}
}  // namespace mozilla

class nsDeviceContextSpecProxy final : public nsIDeviceContextSpec {
 public:
  using IntSize = mozilla::gfx::IntSize;
  using RemotePrintJobChild = mozilla::layout::RemotePrintJobChild;

  explicit nsDeviceContextSpecProxy(RemotePrintJobChild* aRemotePrintJob);

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(nsIPrintSettings* aPrintSettings, bool aIsPrintPreview) final;

  already_AddRefed<PrintTarget> MakePrintTarget() final;

  NS_IMETHOD GetDrawEventRecorder(
      mozilla::gfx::DrawEventRecorder** aDrawEventRecorder) final;

  NS_IMETHOD BeginDocument(const nsAString& aTitle,
                           const nsAString& aPrintToFileName,
                           int32_t aStartPage, int32_t aEndPage) final;

  RefPtr<mozilla::gfx::PrintEndDocumentPromise> EndDocument() final;

  NS_IMETHOD BeginPage(const IntSize& aSizeInPoints) final;

  NS_IMETHOD EndPage() final;

 private:
  ~nsDeviceContextSpecProxy();

  RefPtr<RemotePrintJobChild> mRemotePrintJob;
  RefPtr<mozilla::layout::DrawEventRecorderPRFileDesc> mRecorder;
  IntSize mCurrentPageSizeInPoints;
};

#endif  // nsDeviceContextSpecProxy_h
