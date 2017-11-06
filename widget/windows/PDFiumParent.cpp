/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFiumParent.h"
#include "nsDeviceContextSpecWin.h"
#include "mozilla/gfx/PrintTargetEMF.h"

namespace mozilla {
namespace widget {

PDFiumParent::PDFiumParent(PrintTargetEMF* aTarget)
  : mTarget(aTarget)
{
}

bool
PDFiumParent::Init(IPC::Channel* aChannel, base::ProcessId aPid)
{
  if (NS_WARN_IF(!Open(aChannel, aPid))) {
    return false;
  }

  AddRef();
  return true;
}

void
PDFiumParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mTarget) {
    mTarget->ChannelIsBroken();
  }

  if (mConversionDoneCallback) {
    // Since this printing job was aborted, we do not need to report EMF buffer
    // back to mTarget.
    mConversionDoneCallback();
  }
}

mozilla::ipc::IPCResult
PDFiumParent::RecvConvertToEMFDone(const nsresult& aResult,
                                   mozilla::ipc::Shmem&& aEMFContents)
{
  MOZ_ASSERT(aEMFContents.IsReadable());

  if (mTarget) {
    MOZ_ASSERT(!mConversionDoneCallback);
    mTarget->ConvertToEMFDone(aResult, Move(aEMFContents));
  }

  return IPC_OK();
}

void
PDFiumParent::AbortConversion(ConversionDoneCallback aCallback)
{
  // There is no need to report EMF contents back to mTarget since the print
  // job was aborted, unset mTarget.
  mTarget = nullptr;
  mConversionDoneCallback = aCallback;
}

void PDFiumParent::EndConversion()
{
  // The printing job is finished correctly, mTarget is no longer needed.
  mTarget = nullptr;
}

void
PDFiumParent::OnChannelConnected(int32_t pid)
{
  SetOtherProcessId(pid);
}

void
PDFiumParent::DeallocPPDFiumParent()
{
  Release();
}

} // namespace widget
} // namespace mozilla
