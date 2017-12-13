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
  // mTarget is not nullptr, which means the print job is not done
  // (EndConversion is not called yet). The IPC channel is broken for some
  // reasons. We should tell mTarget to abort this print job.
  if (mTarget) {
    mTarget->ChannelIsBroken();
  }
}

mozilla::ipc::IPCResult
PDFiumParent::RecvConvertToEMFDone(const nsresult& aResult,
                                   mozilla::ipc::Shmem&& aEMFContents)
{
  MOZ_ASSERT(aEMFContents.IsReadable());

  if (mTarget) {
    mTarget->ConvertToEMFDone(aResult, Move(aEMFContents));
  }

  return IPC_OK();
}

void PDFiumParent::EndConversion()
{
  // The printing job is done(all pages printed, or print job cancel, or print
  // job abort), reset mTarget since it may not valid afterward.
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
