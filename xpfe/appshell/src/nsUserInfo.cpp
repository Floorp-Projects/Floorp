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

#ifdef XP_UNIX
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif /* XP_UNIX */

nsUserInfo::nsUserInfo()
{
  NS_INIT_REFCNT();
}

nsUserInfo::~nsUserInfo()
{
}

NS_IMPL_ISUPPORTS1(nsUserInfo,nsIUserInfo);

NS_IMETHODIMP
nsUserInfo::GetUsername(char **aUsername)
{
#ifdef XP_UNIX
    struct passwd *pw = nsnull;

    pw = getpwuid (geteuid ());

    if (!pw || !pw->pw_gecos) return NS_ERROR_FAILURE;

    printf("name = %s\n", pw->pw_gecos);

    *aUsername = nsCRT::strdup(pw->pw_gecos);
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED
#endif /* XP_UNIX */
}
