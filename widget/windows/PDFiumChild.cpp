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

  ipc::Shmem smem;
  PDFViaEMFPrintHelper convertor;
  if (NS_FAILED(convertor.OpenDocument(aFD))) {
    Unused << SendConvertToEMFDone(NS_ERROR_FAILURE, smem);
    return IPC_OK();
  }

  MOZ_ASSERT(convertor.GetPageCount() == 1, "we assume each given PDF contains"                                        "one page only");

  if (!convertor.SavePageToBuffer(0, aPageWidth, aPageHeight, smem, this)) {
    Unused << SendConvertToEMFDone(NS_ERROR_FAILURE, smem);
    return IPC_OK();
  }

  if (!smem.IsReadable()) {
    Unused << SendConvertToEMFDone(NS_ERROR_FAILURE, smem);
    return IPC_OK();
  }

  Unused << SendConvertToEMFDone(NS_OK, smem);
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
