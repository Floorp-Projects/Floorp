/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxTestingThread_h
#define mozilla_SandboxTestingThread_h

#include "nsThreadManager.h"

#if !defined(MOZ_SANDBOX) || !defined(MOZ_DEBUG) || !defined(ENABLE_TESTS)
#  error "This file should not be used outside of debug with tests"
#endif

namespace mozilla {

class SandboxTestingThread {
 public:
  void Dispatch(already_AddRefed<nsIRunnable>&& aRunnable) {
    mThread->Dispatch(std::move(aRunnable), nsIEventTarget::NS_DISPATCH_NORMAL);
  }

  bool IsOnThread() {
    bool on;
    return NS_SUCCEEDED(mThread->IsOnCurrentThread(&on)) && on;
  }

  static SandboxTestingThread* Create() {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIThread> thread;
    if (NS_FAILED(
            NS_NewNamedThread("Sandbox Testing", getter_AddRefs(thread)))) {
      return nullptr;
    }
    return new SandboxTestingThread(thread);
  }

  ~SandboxTestingThread() {
    NS_DispatchToMainThread(NewRunnableMethod("~SandboxTestingThread", mThread,
                                              &nsIThread::Shutdown));
  }

 private:
  explicit SandboxTestingThread(nsIThread* aThread) : mThread(aThread) {
    MOZ_ASSERT(mThread);
  }

  nsCOMPtr<nsIThread> mThread;
};
}  // namespace mozilla

#endif  // mozilla_SandboxTestingThread_h
