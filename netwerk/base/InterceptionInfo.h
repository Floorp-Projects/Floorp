/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_InterceptionInfo_h
#define mozilla_net_InterceptionInfo_h

#include "nsIContentSecurityPolicy.h"
#include "nsIInterceptionInfo.h"
#include "nsIPrincipal.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"

namespace mozilla::net {

using RedirectHistoryArray = nsTArray<nsCOMPtr<nsIRedirectHistoryEntry>>;

class InterceptionInfo final : public nsIInterceptionInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINTERCEPTIONINFO

  InterceptionInfo(nsIPrincipal* aTriggeringPrincipal,
                   nsContentPolicyType aContentPolicyType,
                   const RedirectHistoryArray& aRedirectChain,
                   bool aFromThirdParty);

  using nsIInterceptionInfo::GetExtContentPolicyType;

 private:
  ~InterceptionInfo() = default;

  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsContentPolicyType mContentPolicyType{nsIContentPolicy::TYPE_INVALID};
  RedirectHistoryArray mRedirectChain;
  bool mFromThirdParty = false;
};

}  // namespace mozilla::net

#endif
