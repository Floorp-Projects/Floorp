/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
    NS_INIT_ISUPPORTS();
}

nsHttpBasicAuth::~nsHttpBasicAuth()
{
}

//-----------------------------------------------------------------------------
// nsHttpBasicAuth::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsHttpBasicAuth, nsIHttpAuthenticator);

//-----------------------------------------------------------------------------
// nsHttpBasicAuth::nsIHttpAuthenticator
//-----------------------------------------------------------------------------

nsresult
nsHttpBasicAuth::GenerateCredentials(nsIHttpChannel *httpChannel,
                                     const char *challenge,
                                     const PRUnichar *username,
                                     const PRUnichar *password,
                                     char **creds)

{
    LOG(("nsHttpBasicAuth::GenerateCredentials [challenge=%s]\n", challenge));

    // we only know how to deal with Basic auth for http.
    PRBool isBasicAuth = !PL_strncasecmp(challenge, "basic", 5);
    NS_ENSURE_TRUE(isBasicAuth, NS_ERROR_UNEXPECTED);

    NS_ENSURE_ARG_POINTER(creds);

    // we work with ASCII around here
    nsCAutoString userpass;
    userpass.AssignWithConversion(username);
    if (password) {
        userpass.Append(':');
        userpass.AppendWithConversion(password);
    }

    char *b64userpass = PL_Base64Encode(userpass.get(),
                                        userpass.Length(),
                                        nsnull);
    if (!b64userpass)
        return NS_ERROR_OUT_OF_MEMORY;

    // allocate a buffer sizeof("Basic" + " " + b64userpass + "\0")
    *creds = (char *) malloc(6 + nsCRT::strlen(b64userpass) + 1);
    if (!*creds)
        return NS_ERROR_OUT_OF_MEMORY;

    PL_strcpy(*creds, "Basic ");
    PL_strcpy(*creds + 6, b64userpass);

    PR_Free(b64userpass);
    return NS_OK;
}
