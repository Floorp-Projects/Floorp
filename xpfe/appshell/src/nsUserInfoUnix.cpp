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
#include "nslog.h"

NS_IMPL_LOG(nsUserInfoUnixLog, 0)
#define PRINTF NS_LOG_PRINTF(nsUserInfoUnixLog)
#define FLUSH  NS_LOG_FLUSH(nsUserInfoUnixLog)

/* Some UNIXy platforms don't have pw_gecos. In this case we use pw_name */
#if defined(NO_PW_GECOS)
#define PW_GECOS pw_name
#else
#define PW_GECOS pw_gecos
#endif

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

    if (!pw || !pw->PW_GECOS) return NS_ERROR_FAILURE;

#ifdef DEBUG_sspitzer
    PRINTF("fullname = %s\n", pw->PW_GECOS);
#endif

    nsAutoString fullname(NS_ConvertASCIItoUCS2(pw->PW_GECOS));

    *aFullname = fullname.ToNewUnicode();

    if (*aFullname)
        return NS_OK;

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsUserInfo::GetUsername(char * *aUsername)
{
    struct passwd *pw = nsnull;

    // is this portable?  those are POSIX compliant calls, but I need to check
    pw = getpwuid(geteuid());

    if (!pw || !pw->pw_name) return NS_ERROR_FAILURE;

#ifdef DEBUG_sspitzer
    PRINTF("username = %s\n", pw->pw_name);
#endif

    *aUsername = nsCRT::strdup(pw->pw_name);

    return NS_OK;
}

NS_IMETHODIMP 
nsUserInfo::GetDomain(char * *aDomain)
{
    nsresult rv = NS_ERROR_FAILURE;

    struct utsname buf;
    char *domainname = nsnull;

    // is this portable?  that is a POSIX compliant call, but I need to check
    if (uname(&buf)) { 
        return rv;
    }

#if defined(HAVE_UNAME_DOMAINNAME_FIELD)
    domainname = buf.domainname;
#elif defined(HAVE_UNAME_US_DOMAINNAME_FIELD)
    domainname = buf.__domainname;
#endif

    if (domainname && nsCRT::strlen(domainname)) {   
        *aDomain = nsCRT::strdup(domainname);
        rv = NS_OK;
    }
    else {
        // try to get the hostname from the nodename
        // on machines that use DHCP, domainname may not be set
        // but the nodename might.
        if (buf.nodename && nsCRT::strlen(buf.nodename)) {
            // if the nodename is foo.bar.org, use bar.org as the domain
            char *pos = strchr(buf.nodename,'.');
            if (pos) {
                *aDomain = nsCRT::strdup(pos+1);
                rv = NS_OK;
            }
        }
    }
    
    return rv;
}

NS_IMETHODIMP 
nsUserInfo::GetEmailAddress(char * *aEmailAddress)
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

