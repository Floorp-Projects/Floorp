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
 */

#include "nsUserInfo.h"
#include "nsInternetConfig.h"
#include "nsString.h"

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
	 nsInternetConfig ic;
	 char* cName;
	 nsresult result = ic.GetString( kICRealName, &cName );
	 if ( NS_SUCCEEDED ( result ) )
	 {
		 nsString fullName;
		 fullName.AssignWithConversion( cName );
		 nsMemory::Free( cName );
     *aFullname = fullName.ToNewUnicode();
   }
   return result;
}

NS_IMETHODIMP 
nsUserInfo::GetEmailAddress(char * *aEmailAddress)
{
    nsInternetConfig ic;
		return ic.GetString( kICEmail, aEmailAddress );
}

NS_IMETHODIMP 
nsUserInfo::GetUsername(char * *aUsername)
{
  *aUsername = nsnull;
  nsInternetConfig ic;
  
  char* cString;
	nsresult rv = ic.GetString( kICEmail, &cString );
	if ( NS_FAILED( rv ) ) return rv;
	
  nsCAutoString   tempString(cString);
  nsMemory::Free( cString );
  const char*     atString = "@";
  PRInt32         atOffset = tempString.Find(atString);
  if (atOffset != kNotFound)
    tempString.Truncate(atOffset);
      
  *aUsername = tempString.ToNewCString();  
  return NS_OK;
}

NS_IMETHODIMP 
nsUserInfo::GetDomain(char * *aDomain)
{
  *aDomain = nsnull;
  char* cString;
  nsInternetConfig ic;
	nsresult rv = ic.GetString( kICEmail, &cString );
	if ( NS_FAILED( rv ) ) return rv;
  nsCAutoString   tempString( cString);
  nsMemory::Free( cString );
  const char*     atString = "@";
  PRInt32         atOffset = tempString.Find(atString);
  if (atOffset != kNotFound)
  {
    nsCAutoString domainString;
    tempString.Right(domainString, tempString.Length() - (atOffset + 1));
    *aDomain = domainString.ToNewCString();
    return NS_OK;
  }

  // no domain in the pref
  return NS_ERROR_FAILURE;
}

#pragma mark -






