/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessBridgeChild_h
#define mozilla_net_SocketProcessBridgeChild_h

#include <functional>
#include "mozilla/net/PSocketProcessBridgeChild.h"
#include "nsIObserver.h"

namespace mozilla {
namespace net {

// The IPC actor implements PSocketProcessBridgeChild in content process.
// This is allocated and kept alive by NeckoChild. When "content-child-shutdown"
// topic is observed, this actor will be destroyed.
class SocketProcessBridgeChild final : public PSocketProcessBridgeChild,
                                       public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static already_AddRefed<SocketProcessBridgeChild> GetSingleton();
  using GetPromise =
      MozPromise<RefPtr<SocketProcessBridgeChild>, nsCString, false>;
  static RefPtr<GetPromise> GetSocketProcessBridge();

  mozilla::ipc::IPCResult RecvTest();
  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeferredDestroy();
  bool IsShuttingDown() const { return mShuttingDown; };
  bool Inited() const { return mInited; };
  ProcessId SocketProcessPid() const { return mSocketProcessPid; };

 private:
  DISALLOW_COPY_AND_ASSIGN(SocketProcessBridgeChild);
  static bool Create(Endpoint<PSocketProcessBridgeChild>&& aEndpoint);
  explicit SocketProcessBridgeChild(
      Endpoint<PSocketProcessBridgeChild>&& aEndpoint);
  virtual ~SocketProcessBridgeChild();

  static StaticRefPtr<SocketProcessBridgeChild> sSocketProcessBridgeChild;
  bool mShuttingDown;
  bool mInited = false;
  ProcessId mSocketProcessPid;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessBridgeChild_h
