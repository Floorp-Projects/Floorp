/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PDFIUMPARENT_H_
#define PDFIUMPARENT_H_

#include "mozilla/widget/PPDFiumParent.h"


namespace mozilla {
namespace widget {

class PDFiumParent final : public PPDFiumParent {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PDFiumParent)

  explicit PDFiumParent();

  bool Init(IPC::Channel* aChannel, base::ProcessId aPid);

private:
  ~PDFiumParent() {}

  // PPDFiumParent functions.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvConvertToEMFDone(const nsresult& aResult,
                                               mozilla::ipc::Shmem&& aEMFContents) override;
  void OnChannelConnected(int32_t pid) override;
  void DeallocPPDFiumParent() override;
};

} // namespace widget
} // namespace mozilla

#endif // PDFIUMPARENT_H_
