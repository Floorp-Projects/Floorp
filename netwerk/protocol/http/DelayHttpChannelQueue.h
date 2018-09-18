/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et cin ts=4 sw=4 sts=4: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DelayHttpChannelQueue_h
#define mozilla_net_DelayHttpChannelQueue_h

#include "nsIObserver.h"
#include "nsTArray.h"

namespace mozilla {
namespace net {

class nsHttpChannel;

/**
 * DelayHttpChannelQueue stores a set of nsHttpChannels that
 * are ready to fire out onto the network. However, with FuzzyFox,
 * we can only fire those events at a specific interval, so we
 * delay them here, in an instance of this class, until we observe
 * the topic notificaion that we can send them outbound.
 */
class DelayHttpChannelQueue final : public nsIObserver
{
public:
  static bool
  AttemptQueueChannel(nsHttpChannel* aChannel);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

private:
  DelayHttpChannelQueue();
  ~DelayHttpChannelQueue();

  bool
  Initialize();

  void
  FireQueue();

  FallibleTArray<RefPtr<nsHttpChannel>> mQueue;
};

} // net namespace
} // mozilla namespace

#endif // mozilla_net_DelayHttpChannelQueue_h
