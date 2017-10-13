/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFiumChild.h"
#include "nsLocalFile.h"
#include "mozilla/widget/PDFViaEMFPrintHelper.h"

namespace mozilla {
namespace widget {

PDFiumChild::PDFiumChild()
{
}

PDFiumChild::~PDFiumChild()
{
}

bool
PDFiumChild::Init(base::ProcessId aParentPid,
                  MessageLoop* aIOLoop,
                  IPC::Channel* aChannel)
{
  if (NS_WARN_IF(!Open(aChannel, aParentPid, aIOLoop))) {
    return false;
  }

  return true;
}

mozilla::ipc::IPCResult
PDFiumChild::RecvConvertToEMF(const FileDescriptor& aFD,
                              const int& aPageWidth,
                              const int& aPageHeight)
{
  MOZ_ASSERT(aFD.IsValid() && aPageWidth != 0 && aPageHeight != 0);

  // TBD: Initiate PDFium library.

  return IPC_OK();
}

void
PDFiumChild::OnChannelConnected(int32_t pid)
{
  SetOtherProcessId(pid);
}

void
PDFiumChild::ActorDestroy(ActorDestroyReason why)
{
  XRE_ShutdownChildProcess();
}

} // namespace widget
} // namespace mozilla
