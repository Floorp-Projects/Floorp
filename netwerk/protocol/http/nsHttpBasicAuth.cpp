/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpBasicAuth.h"
#include "plbase64.h"
#include "nsString.h"

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

NS_IMPL_ISUPPORTS1(nsHttpBasicAuth, nsIHttpAuthenticator)

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
nsHttpBasicAuth::GenerateCredentials(nsIHttpAuthenticableChannel *authChannel,
                                     const char *challenge,
                                     bool isProxyAuth,
                                     const PRUnichar *domain,
                                     const PRUnichar *user,
                                     const PRUnichar *password,
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

    // we work with ASCII around here
    nsAutoCString userpass;
    LossyCopyUTF16toASCII(user, userpass);
    userpass.Append(':'); // always send a ':' (see bug 129565)
    if (password)
        LossyAppendUTF16toASCII(password, userpass);

    // plbase64.h provides this worst-case output buffer size calculation.
    // use calloc, since PL_Base64Encode does not null terminate.
    *creds = (char *) calloc(6 + ((userpass.Length() + 2)/3)*4 + 1, 1);
    if (!*creds)
        return NS_ERROR_OUT_OF_MEMORY;

    memcpy(*creds, "Basic ", 6);
    PL_Base64Encode(userpass.get(), userpass.Length(), *creds + 6);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpBasicAuth::GetAuthFlags(uint32_t *flags)
{
    *flags = REQUEST_BASED | REUSABLE_CREDENTIALS | REUSABLE_CHALLENGE;
    return NS_OK;
}
