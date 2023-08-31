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

  mozilla::ipc::IPCResult RecvInitBackgroundDataBridge(
      Endpoint<PBackgroundDataBridgeParent>&& aEndpoint, uint64_t aChannelID);

#ifdef MOZ_WEBRTC
  mozilla::ipc::IPCResult RecvInitMediaTransport(
      Endpoint<PMediaTransportParent>&& aEndpoint);
#endif

  void ActorDestroy(ActorDestroyReason aReason) override;

 private:
  ~SocketProcessBridgeParent();

  nsCOMPtr<nsISerialEventTarget> mMediaTransportTaskQueue;
  ProcessId mId;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessBridgeParent_h
