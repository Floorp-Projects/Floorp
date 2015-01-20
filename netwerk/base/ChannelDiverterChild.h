/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _channeldiverterchild_h_
#define _channeldiverterchild_h_

#include "mozilla/net/PChannelDiverterChild.h"

class nsIDivertableChannel;

namespace mozilla {
namespace net {

class ChannelDiverterArgs;

class ChannelDiverterChild :
  public PChannelDiverterChild
{
public:
  ChannelDiverterChild();
  virtual ~ChannelDiverterChild();
};

} // namespace net
} // namespace mozilla

#endif /* _channeldiverterchild_h_ */
