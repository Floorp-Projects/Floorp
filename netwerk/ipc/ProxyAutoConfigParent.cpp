/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyAutoConfigParent.h"

#include "mozilla/ipc/Endpoint.h"

namespace mozilla::net {

ProxyAutoConfigParent::ProxyAutoConfigParent() = default;

ProxyAutoConfigParent::~ProxyAutoConfigParent() = default;

void ProxyAutoConfigParent::Init(Endpoint<PProxyAutoConfigParent>&& aEndpoint) {
  DebugOnly<bool> ok = aEndpoint.Bind(this);
  MOZ_ASSERT(ok);
}

}  // namespace mozilla::net
