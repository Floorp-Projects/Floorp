/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _channeldiverterparent_h_
#define _channeldiverterparent_h_

#include "mozilla/net/PChannelDiverterParent.h"

class nsIStreamListener;

namespace mozilla {
namespace net {

class ChannelDiverterArgs;
class ADivertableParentChannel;

class ChannelDiverterParent :
  public PChannelDiverterParent
{
public:
  ChannelDiverterParent();
  virtual ~ChannelDiverterParent();

  bool Init(const ChannelDiverterArgs& aArgs);

  void DivertTo(nsIStreamListener* newListener);

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

private:
  nsRefPtr<ADivertableParentChannel> mDivertableChannelParent;
};

} // namespace net
} // namespace mozilla

#endif /* _channeldiverterparent_h_ */
