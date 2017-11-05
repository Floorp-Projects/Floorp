/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PDFiumProcessParent.h"
#include "nsIRunnable.h"
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#include "WinUtils.h"
#endif
#include "nsDeviceContextSpecWin.h"
#include "PDFiumParent.h"

using mozilla::ipc::GeckoChildProcessHost;

namespace mozilla {
namespace widget {

PDFiumProcessParent::PDFiumProcessParent()
  : GeckoChildProcessHost(GeckoProcessType_PDFium)
{
  MOZ_COUNT_CTOR(PDFiumProcessParent);
}

PDFiumProcessParent::~PDFiumProcessParent()
{
  MOZ_COUNT_DTOR(PDFiumProcessParent);

  if (mPDFiumParentActor) {
    mPDFiumParentActor->Close();
  }
}

bool
PDFiumProcessParent::Launch(PrintTargetEMF* aTarget)
{
  mLaunchThread = NS_GetCurrentThread();

  if (!SyncLaunch()) {
    return false;
  }

  // Open the top level protocol for PDFium process.
  MOZ_ASSERT(!mPDFiumParentActor);
  mPDFiumParentActor = new PDFiumParent(aTarget);
  return mPDFiumParentActor->Init(GetChannel(),
                            base::GetProcId(GetChildProcessHandle()));
}

void
PDFiumProcessParent::Delete(bool aWaitingForEMFConversion)
{
  if (aWaitingForEMFConversion) {
    // Can not kill the PDFium process yet since we are still waiting for a
    // EMF conversion response.
    mPDFiumParentActor->AbortConversion([this]() { Delete(false); });
    mPDFiumParentActor->Close();
    return;
  }

  // PDFiumProcessParent::Launch is not called, protocol is not created.
  // It is safe to destroy this object on any thread.
  if (!mLaunchThread) {
    delete this;
    return;
  }

  if (mLaunchThread == NS_GetCurrentThread()) {
    delete this;
    return;
  }

  mLaunchThread->Dispatch(
    NewNonOwningRunnableMethod<bool>("PDFiumProcessParent::Delete",
                                     this,
                                     &PDFiumProcessParent::Delete,
                                     false));
}

} // namespace widget
} // namespace mozilla
