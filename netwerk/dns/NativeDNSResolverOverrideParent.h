/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NativeDNSResolverOverrideParent_h
#define mozilla_net_NativeDNSResolverOverrideParent_h

#include "mozilla/net/PNativeDNSResolverOverrideParent.h"
#include "nsINativeDNSResolverOverride.h"

namespace mozilla {
namespace net {

class NativeDNSResolverOverrideParent : public PNativeDNSResolverOverrideParent,
                                        public nsINativeDNSResolverOverride {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINATIVEDNSRESOLVEROVERRIDE

  explicit NativeDNSResolverOverrideParent() = default;
  static already_AddRefed<nsINativeDNSResolverOverride> GetSingleton();

 private:
  virtual ~NativeDNSResolverOverrideParent() = default;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_NativeDNSResolverOverrideParent_h
