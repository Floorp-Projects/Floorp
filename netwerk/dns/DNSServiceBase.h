/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DNSServiceBase_h
#define mozilla_net_DNSServiceBase_h

#include "mozilla/Atomics.h"
#include "nsIObserver.h"
#include "nsString.h"

class nsIPrefBranch;

namespace mozilla::net {

class DNSServiceBase : public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  DNSServiceBase() = default;

 protected:
  virtual ~DNSServiceBase() = default;
  void AddPrefObserver(nsIPrefBranch* aPrefs);
  virtual void ReadPrefs(const char* aName);
  bool DNSForbiddenByActiveProxy(const nsACString& aHostname, uint32_t aFlags);

  mozilla::Atomic<bool, mozilla::Relaxed> mDisablePrefetch{false};

  mozilla::Atomic<uint32_t, mozilla::Relaxed> mSocksProxyVersion{0};
};

}  // namespace mozilla::net

#endif  // mozilla_net_DNSServiceBase_h
