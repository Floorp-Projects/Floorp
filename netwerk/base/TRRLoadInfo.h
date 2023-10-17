/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TRRLoadInfo_h
#define mozilla_TRRLoadInfo_h

#include "nsILoadInfo.h"
#include "nsIURI.h"
#include "nsTArray.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/OriginAttributes.h"

namespace mozilla {
namespace net {

// TRRLoadInfo is designed to be used by TRRServiceChannel only. Most of
// nsILoadInfo functions are not implemented since TRRLoadInfo needs to
// support off main thread.
class TRRLoadInfo final : public nsILoadInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILOADINFO

  TRRLoadInfo(nsIURI* aResultPrincipalURI,
              nsContentPolicyType aContentPolicyType);

  already_AddRefed<nsILoadInfo> Clone() const;

 private:
  virtual ~TRRLoadInfo() = default;

  nsCOMPtr<nsIURI> mResultPrincipalURI;
  nsContentPolicyType mInternalContentPolicyType;
  OriginAttributes mOriginAttributes;
  nsTArray<nsCOMPtr<nsIRedirectHistoryEntry>> mEmptyRedirectChain;
  nsTArray<nsCOMPtr<nsIPrincipal>> mEmptyPrincipals;
  nsTArray<uint64_t> mEmptyBrowsingContextIDs;
  nsTArray<nsCString> mCorsUnsafeHeaders;
  nsID mSandboxedNullPrincipalID;
  Maybe<mozilla::dom::ClientInfo> mClientInfo;
  Maybe<mozilla::dom::ClientInfo> mReservedClientInfo;
  Maybe<mozilla::dom::ClientInfo> mInitialClientInfo;
  Maybe<mozilla::dom::ServiceWorkerDescriptor> mController;
  Maybe<RFPTarget> mOverriddenFingerprintingSettings;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_TRRLoadInfo_h
