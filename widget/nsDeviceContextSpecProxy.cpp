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
#include "nsIPrintSession.h"
#include "nsIPrintSettings.h"
#include "nsIUUIDGenerator.h"

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

  mRealDeviceContextSpec->Init(nullptr, aPrintSettings, aIsPrintPreview);
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

  rv = NS_GetSpecialDirectory(NS_APP_CONTENT_PROCESS_TEMP_DIR,
                              getter_AddRefs(mRecordingDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mUuidGenerator = do_GetService("@mozilla.org/uuid-generator;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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

  RefPtr<gfxASurface> surface = gfxPlatform::GetPlatform()->
    CreateOffscreenSurface(mozilla::gfx::IntSize::Truncate(width, height),
                           mozilla::gfx::SurfaceFormat::A8R8G8B8_UINT32);
  if (!surface) {
    return nullptr;
  }

  // The type of PrintTarget that we return here shouldn't really matter since
  // our implementation of GetDrawEventRecorder returns an object, which means
  // the DrawTarget returned by the PrintTarget will be a DrawTargetWrapAndRecord.
  // The recording will be serialized and sent over to the parent process where
  // PrintTranslator::TranslateRecording will call MakePrintTarget (indirectly
  // via PrintTranslator::CreateDrawTarget) on whatever type of
  // nsIDeviceContextSpecProxy is created for the platform that we are running
  // on.  It is that DrawTarget that the recording will be replayed on to
  // print.
  // XXX(jwatt): The above isn't quite true.  We do want to use a
  // PrintTargetRecording here, but we can't until bug 1280324 is figured out
  // and fixed otherwise we will cause bug 1280181 to happen again.
  RefPtr<PrintTarget> target = PrintTargetThebes::CreateOrNull(surface);

  return target.forget();
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

nsresult
nsDeviceContextSpecProxy::CreateUniqueTempPath(nsACString& aFilePath)
{
  MOZ_ASSERT(mRecordingDir);
  MOZ_ASSERT(mUuidGenerator);

  nsID uuid;
  nsresult rv = mUuidGenerator->GenerateUUIDInPlace(&uuid);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  char uuidChars[NSID_LENGTH];
  uuid.ToProvidedString(uuidChars);
  mRecordingFileName.AssignASCII(uuidChars);

  nsCOMPtr<nsIFile> recordingFile;
  rv = mRecordingDir->Clone(getter_AddRefs(recordingFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = recordingFile->AppendNative(mRecordingFileName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return recordingFile->GetNativePath(aFilePath);
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::BeginDocument(const nsAString& aTitle,
                                        const nsAString& aPrintToFileName,
                                        int32_t aStartPage, int32_t aEndPage)
{
  nsAutoCString recordingPath;
  nsresult rv = CreateUniqueTempPath(recordingPath);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mRecorder = new mozilla::gfx::DrawEventRecorderFile(recordingPath.get());
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
  // Reopen the file, if necessary, ready for the next page.
  if (!mRecorder->IsOpen()) {
    nsAutoCString recordingPath;
    nsresult rv = CreateUniqueTempPath(recordingPath);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mRecorder->OpenNew(recordingPath.get());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDeviceContextSpecProxy::EndPage()
{
  // Send the page recording to the parent.
  mRecorder->Close();
  mRemotePrintJob->ProcessPage(mRecordingFileName);

  return NS_OK;
}
