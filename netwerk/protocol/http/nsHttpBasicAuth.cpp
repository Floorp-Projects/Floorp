/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpBasicAuth.h"
#include "plstr.h"
#include "nsCRT.h"
#include "nsString.h"
#include "mozilla/Base64.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
namespace net {

StaticRefPtr<nsHttpBasicAuth> nsHttpBasicAuth::gSingleton;

already_AddRefed<nsIHttpAuthenticator> nsHttpBasicAuth::GetOrCreate() {
  nsCOMPtr<nsIHttpAuthenticator> authenticator;
  if (gSingleton) {
    authenticator = gSingleton;
  } else {
    gSingleton = new nsHttpBasicAuth();
    ClearOnShutdown(&gSingleton);
    authenticator = gSingleton;
  }

  return authenticator.forget();
}

//-----------------------------------------------------------------------------
// nsHttpBasicAuth::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsHttpBasicAuth, nsIHttpAuthenticator)

//-----------------------------------------------------------------------------
// nsHttpBasicAuth::nsIHttpAuthenticator
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpBasicAuth::ChallengeReceived(nsIHttpAuthenticableChannel* authChannel,
                                   const nsACString& challenge,
                                   bool isProxyAuth, nsISupports** sessionState,
                                   nsISupports** continuationState,
                                   bool* identityInvalid) {
  // if challenged, then the username:password that was sent must
  // have been wrong.
  *identityInvalid = true;
  return NS_OK;
}
NS_IMETHODIMP
nsHttpBasicAuth::GenerateCredentialsAsync(
    nsIHttpAuthenticableChannel* authChannel,
    nsIHttpAuthenticatorCallback* aCallback, const nsACString& aChallenge,
    bool isProxyAuth, const nsAString& domain, const nsAString& username,
    const nsAString& password, nsISupports* sessionState,
    nsISupports* continuationState, nsICancelable** aCancellable) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpBasicAuth::GenerateCredentials(
    nsIHttpAuthenticableChannel* authChannel, const nsACString& aChallenge,
    bool isProxyAuth, const nsAString& domain, const nsAString& user,
    const nsAString& password, nsISupports** sessionState,
    nsISupports** continuationState, uint32_t* aFlags, nsACString& creds) {
  LOG(("nsHttpBasicAuth::GenerateCredentials [challenge=%s]\n",
       aChallenge.BeginReading()));

  *aFlags = 0;

  // we only know how to deal with Basic auth for http.
  bool isBasicAuth = StringBeginsWith(aChallenge, "basic"_ns,
                                      nsCaseInsensitiveCStringComparator);
  NS_ENSURE_TRUE(isBasicAuth, NS_ERROR_UNEXPECTED);

  // we work with UTF-8 around here
  nsAutoCString userpass;
  CopyUTF16toUTF8(user, userpass);
  userpass.Append(':');  // always send a ':' (see bug 129565)
  AppendUTF16toUTF8(password, userpass);

  nsAutoCString authString{"Basic "_ns};
  nsresult rv = Base64EncodeAppend(userpass, authString);
  NS_ENSURE_SUCCESS(rv, rv);

  creds = authString;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpBasicAuth::GetAuthFlags(uint32_t* flags) {
  *flags = REQUEST_BASED | REUSABLE_CREDENTIALS | REUSABLE_CHALLENGE;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
