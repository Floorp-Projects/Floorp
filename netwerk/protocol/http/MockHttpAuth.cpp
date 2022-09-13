/* vim:set ts=4 sw=2 sts=2 et ci: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "MockHttpAuth.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(MockHttpAuth, nsIHttpAuthenticator)

NS_IMETHODIMP MockHttpAuth::ChallengeReceived(
    nsIHttpAuthenticableChannel* aChannel, const nsACString& aChallenge,
    bool aProxyAuth, nsISupports** aSessionState,
    nsISupports** aContinuationState, bool* aInvalidatesIdentity) {
  *aInvalidatesIdentity = false;
  return NS_OK;
}

NS_IMETHODIMP MockHttpAuth::GenerateCredentialsAsync(
    nsIHttpAuthenticableChannel* aChannel,
    nsIHttpAuthenticatorCallback* aCallback, const nsACString& aChallenge,
    bool aProxyAuth, const nsAString& aDomain, const nsAString& aUser,
    const nsAString& aPassword, nsISupports* aSessionState,
    nsISupports* aContinuationState, nsICancelable** aCancel) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP MockHttpAuth::GenerateCredentials(
    nsIHttpAuthenticableChannel* aChannel, const nsACString& aChallenge,
    bool aProxyAuth, const nsAString& aDomain, const nsAString& aUser,
    const nsAString& aPassword, nsISupports** aSessionState,
    nsISupports** aContinuationState, uint32_t* aFlags, nsACString& _retval) {
  _retval.Assign("moz_test_credentials");
  return NS_OK;
}

NS_IMETHODIMP MockHttpAuth::GetAuthFlags(uint32_t* aAuthFlags) {
  *aAuthFlags |= nsIHttpAuthenticator::CONNECTION_BASED;
  return NS_OK;
}

}  // namespace mozilla::net
