/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/ChannelDiverterChild.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/net/HttpChannelChild.h"
#include "mozilla/net/FTPChannelChild.h"
#include "mozilla/net/PHttpChannelChild.h"
#include "mozilla/net/PFTPChannelChild.h"
#include "nsIDivertableChannel.h"

namespace mozilla {
namespace net {

ChannelDiverterChild::ChannelDiverterChild()
{
}

ChannelDiverterChild::~ChannelDiverterChild()
{
}

} // namespace net
} // namespace mozilla
