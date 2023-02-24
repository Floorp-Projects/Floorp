/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetworkDataCountLayer_h__
#define NetworkDataCountLayer_h__

#include "prerror.h"
#include "prio.h"
#include "ErrorList.h"

namespace mozilla {
namespace net {

nsresult AttachNetworkDataCountLayer(PRFileDesc* fd);

// Get amount of sent bytes.
void NetworkDataCountSent(PRFileDesc* fd, uint64_t& sentBytes);

// Get amount of received bytes.
void NetworkDataCountReceived(PRFileDesc* fd, uint64_t& receivedBytes);

}  // namespace net
}  // namespace mozilla

#endif  // NetworkDataCountLayer_h__
