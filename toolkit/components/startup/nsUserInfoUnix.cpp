/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUserInfo.h"
#include "nsCRT.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/utsname.h>

#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsNativeCharsetUtils.h"

/* Some UNIXy platforms don't have pw_gecos. In this case we use pw_name */
#if defined(NO_PW_GECOS)
#define PW_GECOS pw_name
#else
#define PW_GECOS pw_gecos
#endif

nsUserInfo::nsUserInfo()
{
}

nsUserInfo::~nsUserInfo()
{
}

NS_IMPL_ISUPPORTS1(nsUserInfo,nsIUserInfo)

NS_IMETHODIMP
nsUserInfo::GetFullname(PRUnichar **aFullname)
{
    struct passwd *pw = nullptr;

    pw = getpwuid (geteuid());

    if (!pw || !pw->PW_GECOS) return NS_ERROR_FAILURE;

#ifdef DEBUG_sspitzer
    printf("fullname = %s\n", pw->PW_GECOS);
#endif

    nsCAutoString fullname(pw->PW_GECOS);

    // now try to parse the GECOS information, which will be in the form
    // Full Name, <other stuff> - eliminate the ", <other stuff>
    // also, sometimes GECOS uses "&" to mean "the user name" so do
    // the appropriate substitution
    
    // truncate at first comma (field delimiter)
    PRInt32 index;
    if ((index = fullname.Find(",")) != kNotFound)
        fullname.Truncate(index);

    // replace ampersand with username
    if (pw->pw_name) {
        nsCAutoString username(pw->pw_name);
        if (!username.IsEmpty() && nsCRT::IsLower(username.CharAt(0)))
            username.SetCharAt(nsCRT::ToUpper(username.CharAt(0)), 0);
            
        fullname.ReplaceSubstring("&", username.get());
    }

    nsAutoString unicodeFullname;
    NS_CopyNativeToUnicode(fullname, unicodeFullname);

    *aFullname = ToNewUnicode(unicodeFullname);

    if (*aFullname)
        return NS_OK;

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsUserInfo::GetUsername(char * *aUsername)
{
    struct passwd *pw = nullptr;

    // is this portable?  those are POSIX compliant calls, but I need to check
    pw = getpwuid(geteuid());

    if (!pw || !pw->pw_name) return NS_ERROR_FAILURE;

#ifdef DEBUG_sspitzer
    printf("username = %s\n", pw->pw_name);
#endif

    *aUsername = nsCRT::strdup(pw->pw_name);

    return NS_OK;
}

NS_IMETHODIMP 
nsUserInfo::GetDomain(char * *aDomain)
{
    nsresult rv = NS_ERROR_FAILURE;

    struct utsname buf;
    char *domainname = nullptr;

    // is this portable?  that is a POSIX compliant call, but I need to check
    if (uname(&buf)) { 
        return rv;
    }

#if defined(HAVE_UNAME_DOMAINNAME_FIELD)
    domainname = buf.domainname;
#elif defined(HAVE_UNAME_US_DOMAINNAME_FIELD)
    domainname = buf.__domainname;
#endif

    if (domainname && domainname[0]) {   
        *aDomain = nsCRT::strdup(domainname);
        rv = NS_OK;
    }
    else {
        // try to get the hostname from the nodename
        // on machines that use DHCP, domainname may not be set
        // but the nodename might.
        if (buf.nodename && buf.nodename[0]) {
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

    if (!username.IsEmpty() && !domain.IsEmpty()) {
        emailAddress = (const char *)username;
        emailAddress += "@";
        emailAddress += (const char *)domain;
    }
    else {
        return NS_ERROR_FAILURE;
    }

    *aEmailAddress = ToNewCString(emailAddress);
    
    return NS_OK;
}

