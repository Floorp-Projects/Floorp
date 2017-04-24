/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpBackgroundChannelParent_h
#define mozilla_net_HttpBackgroundChannelParent_h

#include "mozilla/net/PHttpBackgroundChannelParent.h"
#include "mozilla/Atomics.h"
#include "nsID.h"
#include "nsISupportsImpl.h"

class nsIEventTarget;

namespace mozilla {
namespace net {

class HttpChannelParent;

class HttpBackgroundChannelParent final : public PHttpBackgroundChannelParent
{
public:
  explicit HttpBackgroundChannelParent();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HttpBackgroundChannelParent)

  // Try to find associated HttpChannelParent with the same
  // channel Id.
  nsresult Init(const uint64_t& aChannelId);

  // Callbacks for BackgroundChannelRegistrar to notify
  // the associated HttpChannelParent is found.
  void LinkToChannel(HttpChannelParent* aChannelParent);

  // Callbacks for HttpChannelParent to close the background
  // IPC channel.
  void OnChannelClosed();

protected:
  void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  virtual ~HttpBackgroundChannelParent();

  Atomic<bool> mIPCOpened;

  nsCOMPtr<nsIEventTarget> mBackgroundThread;

  // associated HttpChannelParent for generating the channel events
  RefPtr<HttpChannelParent> mChannelParent;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpBackgroundChannelParent_h
