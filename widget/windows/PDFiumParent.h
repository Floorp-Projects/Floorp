/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PDFIUMPARENT_H_
#define PDFIUMPARENT_H_

#include "mozilla/widget/PPDFiumParent.h"

namespace mozilla {
namespace gfx {
  class PrintTargetEMF;
}
}

namespace mozilla {
namespace widget {

class PDFiumParent final : public PPDFiumParent,
                           public mozilla::ipc::IShmemAllocator
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PDFiumParent)

  typedef mozilla::gfx::PrintTargetEMF PrintTargetEMF;
  typedef std::function<void()> ConversionDoneCallback;

  explicit PDFiumParent(PrintTargetEMF* aTarget);

  bool Init(IPC::Channel* aChannel, base::ProcessId aPid);

  void AbortConversion(ConversionDoneCallback aCallback);
  void EndConversion();

  FORWARD_SHMEM_ALLOCATOR_TO(PPDFiumParent)
private:
  ~PDFiumParent() {}

  // PPDFiumParent functions.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvConvertToEMFDone(const nsresult& aResult,
                                               mozilla::ipc::Shmem&& aEMFContents) override;
  void OnChannelConnected(int32_t pid) override;
  void DeallocPPDFiumParent() override;

  PrintTargetEMF* mTarget;
  ConversionDoneCallback mConversionDoneCallback;
};

} // namespace widget
} // namespace mozilla

#endif // PDFIUMPARENT_H_
