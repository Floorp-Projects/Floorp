/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextSpecProxy.h"

#include "gfxPlatform.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/gfx/PrintTargetRecording.h"
#include "mozilla/layout/RemotePrintJobChild.h"
#include "mozilla/RefPtr.h"
#include "mozilla/unused.h"
#include "nsComponentManagerUtils.h"
#include "nsIPrintSession.h"
#include "nsIPrintSettings.h"

using mozilla::Unused;

using namespace mozilla;
using namespace mozilla::gfx;

NS_IMPL_ISUPPORTS(nsDeviceContextSpecProxy, nsIDeviceContextSpec)

NS_IMETHODIMP
nsDeviceContextSpecProxy::Init(nsIWidget* aWidget,
                               nsIPrintSettings* aPrintSettings,
                               bool aIsPrintPreview)
{
  nsresult rv;
  mRealDeviceContextSpec =
    do_CreateInstance("@mozilla.org/gfx/devicecontextspec;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mRealDeviceContextSpec->Init(nullptr, aPrintSettings, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mRealDeviceContextSpec = nullptr;
    return rv;
  }

  mPrintSettings = aPrintSettings;

  if (aIsPrintPreview) {
    return NS_OK;
  }

  // nsIPrintSettings only has a weak reference to nsIPrintSession, so we hold
  // it to make sure it's available for the lifetime of the print.
  rv = mPrintSettings->GetPrintSession(getter_AddRefs(mPrintSession));
  if (NS_FAILED(rv) || !mPrintSession) {
    NS_WARNING("We can't print via the parent without an nsIPrintSession.");
    return NS_ERROR_FAILURE;
  }

  rv = mPrintSession->GetRemotePrintJob(getter_AddRefs(mRemotePrintJob));
  if (NS_FAILED(rv) || !mRemotePrintJob) {
    NS_WARNING("We can't print via the parent without a RemotePrintJobChild.");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

already_AddRefed<PrintTarget>
nsDeviceContextSpecProxy::MakePrintTarget()
{
  MOZ_ASSERT(mRealDeviceContextSpec);

  double width, height;
  nsresult rv = mPrintSettings->GetEffectivePageSize(&width, &height);
  if (NS_WARN_IF(NS_FAILED(rv)) || width <= 0 || height <= 0) {
    return nullptr;
  }

  // convert twips to points
  width /= TWIPS_PER_POINT_FLOAT;
  height /= TWIPS_PER_POINT_FLOAT;

  return PrintTargetRecording::CreateOrNull(IntSize(width, height));
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::GetDrawEventRecorder(mozilla::gfx::DrawEventRecorder** aDrawEventRecorder)
{
  MOZ_ASSERT(aDrawEventRecorder);
  RefPtr<mozilla::gfx::DrawEventRecorder> result = mRecorder;
  result.forget(aDrawEventRecorder);
  return NS_OK;
}

float
nsDeviceContextSpecProxy::GetDPI()
{
  MOZ_ASSERT(mRealDeviceContextSpec);

  return mRealDeviceContextSpec->GetDPI();
}

float
nsDeviceContextSpecProxy::GetPrintingScale()
{
  MOZ_ASSERT(mRealDeviceContextSpec);

  return mRealDeviceContextSpec->GetPrintingScale();
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::BeginDocument(const nsAString& aTitle,
                                        const nsAString& aPrintToFileName,
                                        int32_t aStartPage, int32_t aEndPage)
{
  mRecorder = new mozilla::gfx::DrawEventRecorderMemory();
  return mRemotePrintJob->InitializePrint(nsString(aTitle),
                                          nsString(aPrintToFileName),
                                          aStartPage, aEndPage);
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::EndDocument()
{
  Unused << mRemotePrintJob->SendFinalizePrint();
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::AbortDocument()
{
  Unused << mRemotePrintJob->SendAbortPrint(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::BeginPage()
{
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::EndPage()
{
  // Save the current page recording to shared memory.
  mozilla::ipc::Shmem storedPage;
  size_t recordingSize = mRecorder->RecordingSize();
  if (!mRemotePrintJob->AllocShmem(recordingSize,
                                   mozilla::ipc::SharedMemory::TYPE_BASIC,
                                   &storedPage)) {
    NS_WARNING("Failed to create shared memory for remote printing.");
    return NS_ERROR_FAILURE;
  }

  bool success = mRecorder->CopyRecording(storedPage.get<char>(), recordingSize);
  if (!success) {
    NS_WARNING("Copying recording to shared memory was not succesful.");
    return NS_ERROR_FAILURE;
  }

  // Wipe the recording to free memory. The recorder does not forget which data
  // backed objects that it has stored.
  mRecorder->WipeRecording();

  // Send the page recording to the parent.
  mRemotePrintJob->ProcessPage(storedPage);

  return NS_OK;
}
