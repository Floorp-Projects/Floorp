/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_TrackingDummyChannelParent_h
#define mozilla_net_TrackingDummyChannelParent_h

#include "mozilla/net/PTrackingDummyChannelParent.h"
#include "nsISupportsImpl.h"

class nsILoadInfo;
class nsIURI;

namespace mozilla {
namespace net {

class TrackingDummyChannelParent final : public PTrackingDummyChannelParent
{
public:
  NS_INLINE_DECL_REFCOUNTING(TrackingDummyChannelParent)

  TrackingDummyChannelParent();

  void
  Init(nsIURI* aURI,
       nsIURI* aTopWindowURI,
       nsresult aTopWindowURIResult,
       nsILoadInfo* aLoadInfo);

private:
  ~TrackingDummyChannelParent();

  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  bool mIPCActive;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_TrackingDummyChannelParent_h
