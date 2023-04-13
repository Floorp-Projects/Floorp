/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RemoteSandboxBrokerProcessParent.h"
#include <windows.h>

#include "mozilla/ipc/LaunchError.h"

using mozilla::ipc::GeckoChildProcessHost;
using mozilla::ipc::LaunchError;
using mozilla::ipc::ProcessHandlePromise;

namespace mozilla {

RemoteSandboxBrokerProcessParent::RemoteSandboxBrokerProcessParent()
    : GeckoChildProcessHost(GeckoProcessType_RemoteSandboxBroker) {
  MOZ_COUNT_CTOR(RemoteSandboxBrokerProcessParent);
}

RemoteSandboxBrokerProcessParent::~RemoteSandboxBrokerProcessParent() {
  MOZ_COUNT_DTOR(RemoteSandboxBrokerProcessParent);
}

RefPtr<ProcessHandlePromise> RemoteSandboxBrokerProcessParent::AsyncLaunch() {
  if (!GeckoChildProcessHost::AsyncLaunch()) {
    return ProcessHandlePromise::CreateAndReject(
        LaunchError("RSBPP::AsyncLaunch"), __func__);
  }
  return WhenProcessHandleReady();
}

}  // namespace mozilla
