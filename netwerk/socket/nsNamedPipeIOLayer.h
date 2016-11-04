/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_netwerk_socket_nsNamedPipeIOLayer_h
#define mozilla_netwerk_socket_nsNamedPipeIOLayer_h

#include "nscore.h"
#include "prio.h"

namespace mozilla {
namespace net {

bool IsNamedPipePath(const nsACString& aPath);
PRFileDesc* CreateNamedPipeLayer();

extern PRDescIdentity nsNamedPipeLayerIdentity;

} // namespace net
} // namespace mozilla

#endif // mozilla_netwerk_socket_nsNamedPipeIOLayer_h
