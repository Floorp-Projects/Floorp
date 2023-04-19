/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProxyAutoConfigParent_h__
#define ProxyAutoConfigParent_h__

#include "mozilla/net/PProxyAutoConfigParent.h"

namespace mozilla {
namespace net {

class ProxyAutoConfigParent final : public PProxyAutoConfigParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ProxyAutoConfigParent, final)

  ProxyAutoConfigParent();
  void Init(Endpoint<PProxyAutoConfigParent>&& aEndpoint);

 private:
  virtual ~ProxyAutoConfigParent();
};

}  // namespace net
}  // namespace mozilla

#endif  // ProxyAutoConfigParent_h__
