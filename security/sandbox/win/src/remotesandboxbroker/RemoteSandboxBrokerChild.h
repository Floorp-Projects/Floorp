/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef RemoteSandboxBrokerChild_h_
#define RemoteSandboxBrokerChild_h_

#include "mozilla/PRemoteSandboxBrokerChild.h"
#include "sandboxBroker.h"

namespace mozilla {

class RemoteSandboxBrokerChild : public PRemoteSandboxBrokerChild {
  friend class PRemoteSandboxBrokerChild;

 public:
  RemoteSandboxBrokerChild();
  virtual ~RemoteSandboxBrokerChild();
  bool Init(mozilla::ipc::UntypedEndpoint&& aEndpoint);

 private:
  mozilla::ipc::IPCResult RecvLaunchApp(LaunchParameters&& aParams,
                                        bool* aOutOk, uint64_t* aOutHandle);

  void ActorDestroy(ActorDestroyReason aWhy);
  SandboxBroker mSandboxBroker;
};

}  // namespace mozilla

#endif
