/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gagan Saksena <gagan@netscape.com> (original author)
 *   Mike Shaver <shaver@zeroknowledge.com>
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include "nsHttp.h"
#include "nsHttpBasicAuth.h"
#include "plbase64.h"
#include "plstr.h"
#include "prmem.h"
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
    *identityInvalid = PR_TRUE;
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
                                     PRUint32 *aFlags,
                                     char **creds)

{
    LOG(("nsHttpBasicAuth::GenerateCredentials [challenge=%s]\n", challenge));

    NS_ENSURE_ARG_POINTER(creds);

    *aFlags = 0;

    // we only know how to deal with Basic auth for http.
    bool isBasicAuth = !PL_strncasecmp(challenge, "basic", 5);
    NS_ENSURE_TRUE(isBasicAuth, NS_ERROR_UNEXPECTED);

    // we work with ASCII around here
    nsCAutoString userpass;
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
nsHttpBasicAuth::GetAuthFlags(nsresult *flags)
{
    *flags = REQUEST_BASED | REUSABLE_CREDENTIALS | REUSABLE_CHALLENGE;
    return NS_OK;
}
