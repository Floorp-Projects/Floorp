/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFiumParent.h"
#include "nsDeviceContextSpecWin.h"

namespace mozilla {
namespace widget {

PDFiumParent::PDFiumParent()
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
}

mozilla::ipc::IPCResult
PDFiumParent::RecvConvertToEMFDone(const nsresult& aResult,
                                   mozilla::ipc::Shmem&& aEMFContents)
{
  MOZ_ASSERT(aEMFContents.IsReadable());
  // TBD: playback aEMFContents onto printer DC.

  return IPC_OK();
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
