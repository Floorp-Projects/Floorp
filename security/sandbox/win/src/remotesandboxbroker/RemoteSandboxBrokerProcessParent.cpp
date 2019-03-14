/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RemoteSandboxBrokerProcessParent.h"
#include "nsIRunnable.h"
#include <windows.h>

using mozilla::ipc::GeckoChildProcessHost;

namespace mozilla {

RemoteSandboxBrokerProcessParent::RemoteSandboxBrokerProcessParent()
    : GeckoChildProcessHost(GeckoProcessType_RemoteSandboxBroker) {
  MOZ_COUNT_CTOR(RemoteSandboxBrokerProcessParent);
}

RemoteSandboxBrokerProcessParent::~RemoteSandboxBrokerProcessParent() {
  MOZ_COUNT_DTOR(RemoteSandboxBrokerProcessParent);
}

RefPtr<RemoteSandboxBrokerProcessParent::HandlePromise>
RemoteSandboxBrokerProcessParent::AsyncLaunch() {
  if (!GeckoChildProcessHost::AsyncLaunch()) {
    return HandlePromise::CreateAndReject(GeckoChildProcessHost::LaunchError{},
                                          __func__);
  }
  return WhenProcessHandleReady();
}

}  // namespace mozilla
