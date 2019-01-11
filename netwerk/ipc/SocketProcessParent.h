/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessParent_h
#define mozilla_net_SocketProcessParent_h

#include "mozilla/UniquePtr.h"
#include "mozilla/net/PSocketProcessParent.h"

namespace mozilla {
namespace net {

class SocketProcessHost;

// IPC actor of socket process in parent process. This is allocated and managed
// by SocketProcessHost.
class SocketProcessParent final : public PSocketProcessParent {
 public:
  friend class SocketProcessHost;

  explicit SocketProcessParent(SocketProcessHost* aHost);
  ~SocketProcessParent();

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  SocketProcessHost* mHost;

  static void Destroy(UniquePtr<SocketProcessParent>&& aParent);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessParent_h
