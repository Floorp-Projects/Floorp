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
 *   Doug Turner <dougt@netscape.com>
 */

#include "nsUserInfo.h"
#include "nsString.h"
#include "windows.h"

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
    *aFullname = nsnull;

    TCHAR username[256];
    DWORD size = 256;

    if (!GetUserName(username, &size))
        return NS_ERROR_FAILURE;
    
    nsAutoString fullname(username);
    *aFullname = fullname.ToNewUnicode();
    
    if (*aFullname)
        return NS_OK;

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP GetEmailAddress(char * *aEmailAddress)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GetUsername(char * *aUsername)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP GetDomain(char * *aDomain)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
