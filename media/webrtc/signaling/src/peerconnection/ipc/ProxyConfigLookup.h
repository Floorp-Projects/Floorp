/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ProxyConfigLookup_h
#define mozilla_net_ProxyConfigLookup_h

#include "nsIProtocolProxyCallback.h"
#include <functional>

namespace mozilla {
namespace net {

class ProxyConfigLookup final : public nsIProtocolProxyCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLPROXYCALLBACK

  static nsresult Create(std::function<void(bool)>&& aCallback);

 private:
  explicit ProxyConfigLookup();
  ~ProxyConfigLookup();

  std::function<void(bool)> mCallback;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ProxyConfigLookup_h
