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
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 05/11/2000       IBM Corp.      Created for OS/2 VisualAge build.
 */

#include "nsUserInfo.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsXPIDLString.h"

nsUserInfo::nsUserInfo()
{
}

nsUserInfo::~nsUserInfo()
{
}

NS_IMPL_ISUPPORTS1(nsUserInfo,nsIUserInfo)

NS_IMETHODIMP
nsUserInfo::GetUsername(char **aUsername)
{
    *aUsername = nsnull;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsUserInfo::GetFullname(PRUnichar **aFullname)
{
    *aFullname = nsnull;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsUserInfo::GetDomain(char * *aDomain)
{ 
    *aDomain = nsnull;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsUserInfo::GetEmailAddress(char * *aEmailAddress)
{
    *aEmailAddress = nsnull;
    return NS_ERROR_NOT_IMPLEMENTED;
}
