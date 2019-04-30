/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SECURITY_SANDBOX_SANDBOXTARGET_H__
#define __SECURITY_SANDBOX_SANDBOXTARGET_H__

#include <functional>
#include <list>

#include <windows.h>

#include "mozilla/Assertions.h"
#include "mozilla/Move.h"

namespace sandbox {
class TargetServices;
}

namespace mozilla {

class SandboxTarget {
 public:
  /**
   * Obtains a pointer to the singleton instance
   */
  static SandboxTarget* Instance();

  /**
   * Used by the application to pass in the target services that provide certain
   * functions to the sandboxed code.
   * The target services must already be initialized.
   *
   * @param aTargetServices The target services that will be used
   */
  void SetTargetServices(sandbox::TargetServices* aTargetServices) {
    MOZ_ASSERT(aTargetServices);
    MOZ_ASSERT(!mTargetServices,
               "Sandbox TargetServices must only be set once.");

    mTargetServices = aTargetServices;
  }

  template <typename CallbackT>
  void RegisterSandboxStartCallback(CallbackT aCallback) {
    mStartObservers.push_back(std::forward<CallbackT>(aCallback));
  }

  /**
   * Called by the library that wants to "start" the sandbox, i.e. change to the
   * more secure delayed / lockdown policy.
   */
  void StartSandbox();

  /**
   * Used to duplicate handles via the broker process. The permission for the
   * handle type and target process has to have been set on the sandbox policy.
   */
  bool BrokerDuplicateHandle(HANDLE aSourceHandle, DWORD aTargetProcessId,
                             HANDLE* aTargetHandle, DWORD aDesiredAccess,
                             DWORD aOptions);

 protected:
  SandboxTarget() : mTargetServices(nullptr) {}

  sandbox::TargetServices* mTargetServices;

 private:
  void NotifyStartObservers();
  std::list<std::function<void()>> mStartObservers;
};

}  // namespace mozilla

#endif
