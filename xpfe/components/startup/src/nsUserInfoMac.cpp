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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsUserInfo.h"
#include "nsString.h"
#include "nsReadableUtils.h"

#include "nsIServiceManager.h"
#include "nsIInternetConfigService.h"

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
  nsresult result = NS_ERROR_FAILURE;
  nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
  if (icService)
  {
	  nsCAutoString cName;
    result = icService->GetString(nsIInternetConfigService::eICString_RealName, cName);
    if ( NS_SUCCEEDED ( result ) )
    {
  	  nsString fullName;
      *aFullname = ToNewUnicode(cName);
    }
  }
  return result;

}

NS_IMETHODIMP 
nsUserInfo::GetEmailAddress(char * *aEmailAddress)
{
  nsresult result = NS_ERROR_FAILURE;
  nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
  if (icService)
  {
    nsCAutoString tempString;
    result = icService->GetString(nsIInternetConfigService::eICString_Email, tempString);
    if (NS_SUCCEEDED(result))
      *aEmailAddress = ToNewCString(tempString);  

  }
  return result;
}

NS_IMETHODIMP 
nsUserInfo::GetUsername(char * *aUsername)
{
  *aUsername = nsnull;
  
  nsCAutoString   tempString;
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
  if (icService)
    rv = icService->GetString(nsIInternetConfigService::eICString_Email, tempString);

	if ( NS_FAILED( rv ) ) return rv;
	
  const char*     atString = "@";
  PRInt32         atOffset = tempString.Find(atString);
  if (atOffset != kNotFound)
    tempString.Truncate(atOffset);
      
  *aUsername = ToNewCString(tempString);  
  return NS_OK;
}

NS_IMETHODIMP 
nsUserInfo::GetDomain(char * *aDomain)
{
  *aDomain = nsnull;
  nsCAutoString   tempString;
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
  if (icService)
    rv = icService->GetString(nsIInternetConfigService::eICString_Email, tempString);
	if ( NS_FAILED( rv ) ) return rv;
  const char*     atString = "@";
  PRInt32         atOffset = tempString.Find(atString);
  if (atOffset != kNotFound)
  {
    nsCAutoString domainString;
    tempString.Right(domainString, tempString.Length() - (atOffset + 1));
    *aDomain = ToNewCString(domainString);
    return NS_OK;
  }

  // no domain in the pref
  return NS_ERROR_FAILURE;
}

#pragma mark -






