/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessBackgroundChild_h
#define mozilla_net_SocketProcessBackgroundChild_h

#include "mozilla/MoveOnlyFunction.h"
#include "mozilla/net/PSocketProcessBackgroundChild.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace net {

class SocketProcessBackgroundChild final
    : public PSocketProcessBackgroundChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SocketProcessBackgroundChild, final)

  static void Create(ipc::Endpoint<PSocketProcessBackgroundChild>&& aEndpoint);
  static void Shutdown();
  // |aCallback| will be called with the actor asynchronously on |sTaskQueue|.
  static nsresult WithActor(
      const char* aName,
      MoveOnlyFunction<void(SocketProcessBackgroundChild*)> aCallback);

 private:
  SocketProcessBackgroundChild();
  virtual ~SocketProcessBackgroundChild();

  static RefPtr<SocketProcessBackgroundChild> GetSingleton();
  static already_AddRefed<nsISerialEventTarget> TaskQueue();

  static StaticMutex sMutex;
  static StaticRefPtr<SocketProcessBackgroundChild> sInstance
      MOZ_GUARDED_BY(sMutex);
  static StaticRefPtr<nsISerialEventTarget> sTaskQueue MOZ_GUARDED_BY(sMutex);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessBackgroundChild_h
