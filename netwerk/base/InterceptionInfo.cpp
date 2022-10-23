/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/InterceptionInfo.h"
#include "nsContentUtils.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(InterceptionInfo, nsIInterceptionInfo)

InterceptionInfo::InterceptionInfo(nsIPrincipal* aTriggeringPrincipal,
                                   nsContentPolicyType aContentPolicyType,
                                   const RedirectHistoryArray& aRedirectChain,
                                   bool aFromThirdParty)
    : mTriggeringPrincipal(aTriggeringPrincipal),
      mContentPolicyType(aContentPolicyType),
      mFromThirdParty(aFromThirdParty) {
  SetRedirectChain(aRedirectChain);
}

nsIPrincipal* InterceptionInfo::TriggeringPrincipal() {
  return mTriggeringPrincipal;
}

void InterceptionInfo::SetTriggeringPrincipal(nsIPrincipal* aPrincipal) {
  mTriggeringPrincipal = aPrincipal;
}

nsContentPolicyType InterceptionInfo::ContentPolicyType() {
  return mContentPolicyType;
}

nsContentPolicyType InterceptionInfo::ExternalContentPolicyType() {
  return static_cast<nsContentPolicyType>(
      nsContentUtils::InternalContentPolicyTypeToExternal(mContentPolicyType));
}

void InterceptionInfo::SetContentPolicyType(
    const nsContentPolicyType aContentPolicyType) {
  mContentPolicyType = aContentPolicyType;
}

const RedirectHistoryArray& InterceptionInfo::RedirectChain() {
  return mRedirectChain;
}

void InterceptionInfo::SetRedirectChain(
    const RedirectHistoryArray& aRedirectChain) {
  for (auto entry : aRedirectChain) {
    mRedirectChain.AppendElement(entry);
  }
}

bool InterceptionInfo::FromThirdParty() { return mFromThirdParty; }

void InterceptionInfo::SetFromThirdParty(bool aFromThirdParty) {
  mFromThirdParty = aFromThirdParty;
}

}  // namespace mozilla::net
