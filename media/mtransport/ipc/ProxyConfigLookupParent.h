/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ProxyConfigLookupParent_h
#define mozilla_net_ProxyConfigLookupParent_h

#include "mozilla/net/PProxyConfigLookup.h"
#include "mozilla/net/PProxyConfigLookupParent.h"

namespace mozilla {
namespace net {

class ProxyConfigLookupParent final : public PProxyConfigLookupParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(ProxyConfigLookupParent)

  ProxyConfigLookupParent();

  void DoProxyLookup();

 private:
  ~ProxyConfigLookupParent();
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ProxyConfigLookupParent_h
