/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyConfigLookupParent.h"
#include "ProxyConfigLookup.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace net {

ProxyConfigLookupParent::ProxyConfigLookupParent() = default;

ProxyConfigLookupParent::~ProxyConfigLookupParent() = default;

void ProxyConfigLookupParent::DoProxyLookup() {
  RefPtr<ProxyConfigLookupParent> self = this;
  nsresult rv = ProxyConfigLookup::Create([self](bool aProxied) {
    if (self->CanSend()) {
      Unused << Send__delete__(self, aProxied);
    }
  });

  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << Send__delete__(self, false);
  }
}

}  // namespace net
}  // namespace mozilla
