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
 * Contributor(s): 
 *   Gagan Saksena <gagan@netscape.com> (original author)
 *   Darin Fisher <darin@netscape.com>
 */

#include "nscore.h"
#include "nsAuth.h"
#include "nsCRT.h"

nsAuth::nsAuth(nsIURI *aURI, 
               const char *aEncString, 
               const char *aUsername,
               const char *aPassword,
               const char *aRealm)
    : encodedString(0)
    , password(0)
    , realm(0)
    , username(0)
    , uri(dont_QueryInterface(aURI))
{
    if (aEncString)
        encodedString = nsCRT::strdup(aEncString);
    if (aUsername)
        username = nsCRT::strdup(aUsername);
    if (aPassword)
        password = nsCRT::strdup(aPassword);
    if (aRealm)
        realm = nsCRT::strdup(aRealm);
}

nsAuth::~nsAuth()
{
    CRTFREEIF(encodedString);
    CRTFREEIF(username);
    CRTFREEIF(password);
    CRTFREEIF(realm);
}
