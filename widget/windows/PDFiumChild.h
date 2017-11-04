/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PDFIUMCHILD_H_
#define PDFIUMCHILD_H_

#include "mozilla/widget/PPDFiumChild.h"

namespace mozilla {
namespace widget {

class PDFiumChild final : public PPDFiumChild,
                          public mozilla::ipc::IShmemAllocator
{
public:
  PDFiumChild();
  virtual ~PDFiumChild();

  bool Init(base::ProcessId aParentPid,
            MessageLoop* aIOLoop,
            IPC::Channel* aChannel);

  FORWARD_SHMEM_ALLOCATOR_TO(PPDFiumChild)

private:
  // PPDFiumChild functions.
  mozilla::ipc::IPCResult RecvConvertToEMF(const FileDescriptor& aFD,
                                           const int& aPageWidth,
                                           const int& aPageHeight) override;
  void ActorDestroy(ActorDestroyReason aWhy) override;
  void OnChannelConnected(int32_t pid) override;
};

} // namespace widget
} // namespace mozilla

#endif // PDFIUMCHILD_H_