/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpBasicAuth.h"
#include "plstr.h"
#include "nsString.h"
#include "mozilla/Base64.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// nsHttpBasicAuth <public>
//-----------------------------------------------------------------------------

nsHttpBasicAuth::nsHttpBasicAuth()
{
}

nsHttpBasicAuth::~nsHttpBasicAuth()
{
}

//-----------------------------------------------------------------------------
// nsHttpBasicAuth::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsHttpBasicAuth, nsIHttpAuthenticator)

//-----------------------------------------------------------------------------
// nsHttpBasicAuth::nsIHttpAuthenticator
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpBasicAuth::ChallengeReceived(nsIHttpAuthenticableChannel *authChannel,
                                   const char *challenge,
                                   bool isProxyAuth,
                                   nsISupports **sessionState,
                                   nsISupports **continuationState,
                                   bool *identityInvalid)
{
    // if challenged, then the username:password that was sent must
    // have been wrong.
    *identityInvalid = true;
    return NS_OK;
}
NS_IMETHODIMP
nsHttpBasicAuth::GenerateCredentialsAsync(nsIHttpAuthenticableChannel *authChannel,
                                          nsIHttpAuthenticatorCallback* aCallback,
                                          const char *challenge,
                                          bool isProxyAuth,
                                          const char16_t *domain,
                                          const char16_t *username,
                                          const char16_t *password,
                                          nsISupports *sessionState,
                                          nsISupports *continuationState,
                                          nsICancelable **aCancellable)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpBasicAuth::GenerateCredentials(nsIHttpAuthenticableChannel *authChannel,
                                     const char *challenge,
                                     bool isProxyAuth,
                                     const char16_t *domain,
                                     const char16_t *user,
                                     const char16_t *password,
                                     nsISupports **sessionState,
                                     nsISupports **continuationState,
                                     uint32_t *aFlags,
                                     char **creds)

{
    LOG(("nsHttpBasicAuth::GenerateCredentials [challenge=%s]\n", challenge));

    NS_ENSURE_ARG_POINTER(creds);

    *aFlags = 0;

    // we only know how to deal with Basic auth for http.
    bool isBasicAuth = !PL_strncasecmp(challenge, "basic", 5);
    NS_ENSURE_TRUE(isBasicAuth, NS_ERROR_UNEXPECTED);

    // we work with UTF-8 around here
    nsAutoCString userpass;
    CopyUTF16toUTF8(user, userpass);
    userpass.Append(':'); // always send a ':' (see bug 129565)
    if (password) {
        AppendUTF16toUTF8(password, userpass);
    }

    nsAutoCString authString;
    nsresult rv = Base64Encode(userpass, authString);
    NS_ENSURE_SUCCESS(rv, rv);

    authString.InsertLiteral("Basic ", 0);

    *creds = ToNewCString(authString);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpBasicAuth::GetAuthFlags(uint32_t *flags)
{
    *flags = REQUEST_BASED | REUSABLE_CREDENTIALS | REUSABLE_CHALLENGE;
    return NS_OK;
}

} // namespace net
} // namespace mozilla
