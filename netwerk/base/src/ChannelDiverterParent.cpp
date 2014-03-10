/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/ChannelDiverterParent.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/FTPChannelParent.h"
#include "mozilla/net/PHttpChannelParent.h"
#include "mozilla/net/PFTPChannelParent.h"
#include "ADivertableParentChannel.h"

namespace mozilla {
namespace net {

ChannelDiverterParent::ChannelDiverterParent()
{
}

ChannelDiverterParent::~ChannelDiverterParent()
{
}

bool
ChannelDiverterParent::Init(const ChannelDiverterArgs& aChannel)
{
  switch (aChannel.type()) {
  case ChannelDiverterArgs::TPHttpChannelParent:
  {
    mDivertableChannelParent = static_cast<ADivertableParentChannel*>(
      static_cast<HttpChannelParent*>(aChannel.get_PHttpChannelParent()));
    break;
  }
  case ChannelDiverterArgs::TPFTPChannelParent:
  {
    mDivertableChannelParent = static_cast<ADivertableParentChannel*>(
      static_cast<FTPChannelParent*>(aChannel.get_PFTPChannelParent()));
    break;
  }
  default:
    NS_NOTREACHED("unknown ChannelDiverterArgs type");
    return false;
  }
  MOZ_ASSERT(mDivertableChannelParent);

  nsresult rv = mDivertableChannelParent->SuspendForDiversion();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  return true;
}

void
ChannelDiverterParent::DivertTo(nsIStreamListener* newListener)
{
  MOZ_ASSERT(newListener);
  MOZ_ASSERT(mDivertableChannelParent);

  mDivertableChannelParent->DivertTo(newListener);
}

} // namespace net
} // namespace mozilla
