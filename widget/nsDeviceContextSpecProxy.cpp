/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextSpecProxy.h"

#include "gfxASurface.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/gfx/PrintTargetThebes.h"
#include "mozilla/layout/RemotePrintJobChild.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIPrintSettings.h"
#include "private/pprio.h"

using mozilla::Unused;

using namespace mozilla;
using namespace mozilla::gfx;

NS_IMPL_ISUPPORTS(nsDeviceContextSpecProxy, nsIDeviceContextSpec)

nsDeviceContextSpecProxy::nsDeviceContextSpecProxy(
    RemotePrintJobChild* aRemotePrintJob)
    : mRemotePrintJob(aRemotePrintJob) {}
nsDeviceContextSpecProxy::~nsDeviceContextSpecProxy() = default;

NS_IMETHODIMP
nsDeviceContextSpecProxy::Init(nsIPrintSettings* aPrintSettings,
                               bool aIsPrintPreview) {
  mPrintSettings = aPrintSettings;

  if (aIsPrintPreview) {
    return NS_OK;
  }

  if (!mRemotePrintJob) {
    NS_WARNING("We can't print via the parent without a RemotePrintJobChild.");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

already_AddRefed<PrintTarget> nsDeviceContextSpecProxy::MakePrintTarget() {
  double width, height;
  mPrintSettings->GetEffectiveSheetSize(&width, &height);
  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  // convert twips to points
  width /= TWIPS_PER_POINT_FLOAT;
  height /= TWIPS_PER_POINT_FLOAT;

  RefPtr<gfxASurface> surface =
      gfxPlatform::GetPlatform()->CreateOffscreenSurface(
          mozilla::gfx::IntSize::Ceil(width, height),
          mozilla::gfx::SurfaceFormat::A8R8G8B8_UINT32);
  if (!surface) {
    return nullptr;
  }

  // The type of PrintTarget that we return here shouldn't really matter since
  // our implementation of GetDrawEventRecorder returns an object, which means
  // the DrawTarget returned by the PrintTarget will be a
  // DrawTargetWrapAndRecord. The recording will be serialized and sent over to
  // the parent process where PrintTranslator::TranslateRecording will call
  // MakePrintTarget (indirectly via PrintTranslator::CreateDrawTarget) on
  // whatever type of nsIDeviceContextSpecProxy is created for the platform that
  // we are running on.  It is that DrawTarget that the recording will be
  // replayed on to print.
  // XXX(jwatt): The above isn't quite true.  We do want to use a
  // PrintTargetRecording here, but we can't until bug 1280324 is figured out
  // and fixed otherwise we will cause bug 1280181 to happen again.
  RefPtr<PrintTarget> target = PrintTargetThebes::CreateOrNull(surface);

  return target.forget();
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::GetDrawEventRecorder(
    mozilla::gfx::DrawEventRecorder** aDrawEventRecorder) {
  MOZ_ASSERT(aDrawEventRecorder);
  RefPtr<mozilla::gfx::DrawEventRecorder> result = mRecorder;
  result.forget(aDrawEventRecorder);
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::BeginDocument(const nsAString& aTitle,
                                        const nsAString& aPrintToFileName,
                                        int32_t aStartPage, int32_t aEndPage) {
  if (!mRemotePrintJob || mRemotePrintJob->IsDestroyed()) {
    mRemotePrintJob = nullptr;
    return NS_ERROR_NOT_AVAILABLE;
  }

  mRecorder = new mozilla::layout::DrawEventRecorderPRFileDesc();
  nsresult rv =
      mRemotePrintJob->InitializePrint(nsString(aTitle), aStartPage, aEndPage);
  if (NS_FAILED(rv)) {
    // The parent process will send a 'delete' message to tell this process to
    // delete our RemotePrintJobChild.  As soon as we return to the event loop
    // and evaluate that message we will crash if we try to access
    // mRemotePrintJob.  We must not try to use it again.
    mRemotePrintJob = nullptr;
  }
  return rv;
}

RefPtr<mozilla::gfx::PrintEndDocumentPromise>
nsDeviceContextSpecProxy::EndDocument() {
  if (!mRemotePrintJob || mRemotePrintJob->IsDestroyed()) {
    mRemotePrintJob = nullptr;
    return mozilla::gfx::PrintEndDocumentPromise::CreateAndReject(
        NS_ERROR_NOT_AVAILABLE, __func__);
  }

  Unused << mRemotePrintJob->SendFinalizePrint();

  if (mRecorder) {
    MOZ_ASSERT(!mRecorder->IsOpen());
    mRecorder->DetachResources();
    mRecorder = nullptr;
  }

  return mozilla::gfx::PrintEndDocumentPromise::CreateAndResolve(true,
                                                                 __func__);
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::BeginPage(const IntSize& aSizeInPoints) {
  if (!mRemotePrintJob || mRemotePrintJob->IsDestroyed()) {
    mRemotePrintJob = nullptr;
    return NS_ERROR_NOT_AVAILABLE;
  }

  mRecorder->OpenFD(mRemotePrintJob->GetNextPageFD());
  mCurrentPageSizeInPoints = aSizeInPoints;

  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::EndPage() {
  if (!mRemotePrintJob || mRemotePrintJob->IsDestroyed()) {
    mRemotePrintJob = nullptr;
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Send the page recording to the parent.
  mRecorder->Close();
  mRemotePrintJob->ProcessPage(mCurrentPageSizeInPoints,
                               std::move(mRecorder->TakeDependentSurfaces()));

  return NS_OK;
}
