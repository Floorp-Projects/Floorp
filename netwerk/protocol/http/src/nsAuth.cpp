/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: Gagan Saksena <gagan@netscape.com>
 *
 * Contributor(s): 
 */

#include "nscore.h"
#include "nsAuth.h"
#include "nsCRT.h"

nsAuth::nsAuth(nsIURI* i_URI, 
        const char* i_encString, 
        const char* i_username,
        const char* i_password,
        const char* i_realm):
    encodedString(0),
    password(0),
    realm(0),
    username(0),
    uri(dont_QueryInterface(i_URI))
{
    if (i_encString)
        encodedString = nsCRT::strdup(i_encString);
    if (i_username)
        username = nsCRT::strdup(i_username);
    if (i_password)
        password = nsCRT::strdup(i_password);
    if (i_realm)
        realm = nsCRT::strdup(i_realm);
}

nsAuth::~nsAuth()
{
    CRTFREEIF(encodedString);
    CRTFREEIF(username);
    CRTFREEIF(password);
    CRTFREEIF(realm);
}
