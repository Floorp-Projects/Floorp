/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessBridgeParent_h
#define mozilla_net_SocketProcessBridgeParent_h

#include "mozilla/net/PSocketProcessBridgeParent.h"

namespace mozilla {
namespace net {

// The IPC actor implements PSocketProcessBridgeParent in socket process.
// This is allocated and kept alive by SocketProcessChild. When |ActorDestroy|
// is called, |SocketProcessChild::DestroySocketProcessBridgeParent| will be
// called to destroy this actor.
class SocketProcessBridgeParent final : public PSocketProcessBridgeParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SocketProcessBridgeParent, final)

  explicit SocketProcessBridgeParent(ProcessId aId);

  mozilla::ipc::IPCResult RecvTest();
  mozilla::ipc::IPCResult RecvInitBackground(
      Endpoint<PBackgroundStarterParent>&& aEndpoint);

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeferredDestroy();

  bool Closed() const { return mClosed; }

 private:
  ~SocketProcessBridgeParent();

  ProcessId mId;
  bool mClosed;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessBridgeParent_h
