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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Seth Spitzer <sspitzer@netscape.com>
 */

#include "nsUserInfo.h"
#include "nsCRT.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "nsString.h"
#include "nsXPIDLString.h"

nsUserInfo::nsUserInfo()
{
  NS_INIT_REFCNT();
}

nsUserInfo::~nsUserInfo()
{
}

NS_IMPL_ISUPPORTS1(nsUserInfo,nsIUserInfo);

NS_IMETHODIMP
nsUserInfo::GetFullname(PRUnichar **aFullname)
{
    struct passwd *pw = nsnull;

    pw = getpwuid (geteuid());

    // do I need to free pw? 

    if (!pw || !pw->pw_gecos) return NS_ERROR_FAILURE;

#ifdef DEBUG_sspitzer
    printf("fullname = %s\n", pw->pw_gecos);
#endif

    nsAutoString fullname(pw->pw_gecos);

    *aFullname = fullname.ToNewUnicode();

    if (*aFullname)
        return NS_OK;

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GetUsername(char * *aUsername)
{
    struct passwd *pw = nsnull;

    pw = getpwuid(geteuid());

    // do I need to free pw? 

    if (!pw || !pw->pw_name) return NS_ERROR_FAILURE;

#ifdef DEBUG_sspitzer
    printf("username = %s\n", pw->pw_name);
#endif

    *aUsername = nsCRT::strdup(pw->pw_name);

    return NS_OK;
}

NS_IMETHODIMP GetDomain(char * *aDomain)
{
    struct utsname buf;
    
    if (uname(&buf)) { 
        return NS_ERROR_FAILURE; 
    }
    
    *aDomain = nsCRT::strdup(buf.__domainname);

    return NS_OK;
}

NS_IMETHODIMP GetEmailAddress(char * *aEmailAddress)
{
    // use username + "@" + domain for the email address

    nsresult rv;

    nsCAutoString emailAddress;
    nsXPIDLCString username;
    nsXPIDLCString domain;

    rv = GetUsername(getter_Copies(username));
    if (NS_FAILED(rv)) return rv;

    rv = GetDomain(getter_Copies(domain));
    if (NS_FAILED(rv)) return rv;

    if ((const char *)username && (const char*)domain && nsCRT::strlen((const char *)username) && nsCRT::strlen((const char *)domain)) {
        emailAddress = (const char *)username;
        emailAddress += "@";
        emailAddress += (const char *)domain;
    }
    else {
        return NS_ERROR_FAILURE;
    }

    *aEmailAddress = nsCRT::strdup((const char *)emailAddress);
    
    return NS_OK;
}

