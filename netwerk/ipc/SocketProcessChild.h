/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessChild_h
#define mozilla_net_SocketProcessChild_h

#include "mozilla/net/PSocketProcessChild.h"

namespace mozilla {
namespace net {

// The IPC actor implements PSocketProcessChild in child process.
// This is allocated and kept alive by SocketProcessImpl.
class SocketProcessChild final : public PSocketProcessChild {
 public:
  SocketProcessChild();
  ~SocketProcessChild();

  static SocketProcessChild* GetSingleton();

  bool Init(base::ProcessId aParentPid, const char* aParentBuildID,
            MessageLoop* aIOLoop, IPC::Channel* aChannel);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& aPref) override;
  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage, const MaybeFileDesc& DMDFile) override;

  void CleanUp();

 private:
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessChild_h
