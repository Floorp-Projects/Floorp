/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RemoteSandboxBrokerProcessChild_h_
#define RemoteSandboxBrokerProcessChild_h_

#include "mozilla/ipc/ProcessChild.h"
#include "RemoteSandboxBrokerChild.h"

namespace mozilla {

class RemoteSandboxBrokerProcessChild final
    : public mozilla::ipc::ProcessChild {
 protected:
  typedef mozilla::ipc::ProcessChild ProcessChild;

 public:
  using ProcessChild::ProcessChild;
  ~RemoteSandboxBrokerProcessChild();

  bool Init(int aArgc, char* aArgv[]) override;
  void CleanUp() override;

 private:
  RemoteSandboxBrokerChild mSandboxBrokerChild;
};

}  // namespace mozilla

#endif  // GMPProcessChild_h_
