/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RemoteSandboxBrokerProcessChild.h"

#include "mozilla/ipc/IOThreadChild.h"
#include "mozilla/BackgroundHangMonitor.h"

using mozilla::ipc::IOThreadChild;

namespace mozilla {

RemoteSandboxBrokerProcessChild::~RemoteSandboxBrokerProcessChild() {}

bool RemoteSandboxBrokerProcessChild::Init(int aArgc, char* aArgv[]) {
  BackgroundHangMonitor::Startup();
  return mSandboxBrokerChild.Init(TakeInitialEndpoint());
}

void RemoteSandboxBrokerProcessChild::CleanUp() {
  BackgroundHangMonitor::Shutdown();
}

}  // namespace mozilla
