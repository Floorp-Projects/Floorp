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

#include "nsUserInfoMac.h"

#include "nsString.h"

#include <Processes.h>
#include <CodeFragments.h>

//-----------------------------------------------------------
nsUserInfo::nsUserInfo()
//-----------------------------------------------------------
: mInstance(0)
, mInitted(PR_FALSE)
{
  NS_INIT_REFCNT();
}

//-----------------------------------------------------------
nsUserInfo::~nsUserInfo()
//-----------------------------------------------------------
{
  if (mInstance)
    ::ICStop(mInstance);
  mInstance = 0;
}

NS_IMPL_ISUPPORTS1(nsUserInfo,nsIUserInfo);

NS_IMETHODIMP
nsUserInfo::GetFullname(PRUnichar **aFullname)
{
  *aFullname = nsnull;
  
  if (NS_FAILED(EnsureInitted()))
    return NS_ERROR_FAILURE;
  
  ICAttr  dummyAttr;
  Str255  resultString;
  long    stringLen;
  OSErr err = ::ICGetPref(mInstance, kICRealName, &dummyAttr, (Ptr)resultString, &stringLen);
  if (err != noErr) return NS_ERROR_FAILURE;
  
  *aFullname = PStringToNewUCS2(resultString);
  return NS_OK;
}

NS_IMETHODIMP 
nsUserInfo::GetEmailAddress(char * *aEmailAddress)
{
  *aEmailAddress = nsnull;
  
  if (NS_FAILED(EnsureInitted()))
    return NS_ERROR_FAILURE;
  
  ICAttr  dummyAttr;
  Str255  resultString;
  long    stringLen;
  OSErr err = ::ICGetPref(mInstance, kICEmail, &dummyAttr, (Ptr)resultString, &stringLen);
  if (err != noErr) return NS_ERROR_FAILURE;
  
  nsCAutoString   tempString((const char*)&resultString[1], resultString[0]);
  *aEmailAddress = tempString.ToNewCString();
  return NS_OK;
}

NS_IMETHODIMP 
nsUserInfo::GetUsername(char * *aUsername)
{
  *aUsername = nsnull;

  if (NS_FAILED(EnsureInitted()))
    return NS_ERROR_FAILURE;
  
  ICAttr  dummyAttr;
  Str255  resultString;
  long    stringLen;
  OSErr err = ::ICGetPref(mInstance, kICMailAccount, &dummyAttr, (Ptr)resultString, &stringLen);
  if (err != noErr) return NS_ERROR_FAILURE;
  
  nsCAutoString   tempString((const char*)&resultString[1], resultString[0]);
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

  if (NS_FAILED(EnsureInitted()))
    return NS_ERROR_FAILURE;
  
  ICAttr  dummyAttr;
  Str255  resultString;
  long    stringLen;
  OSErr err = ::ICGetPref(mInstance, kICMailAccount, &dummyAttr, (Ptr)resultString, &stringLen);
  if (err != noErr) return NS_ERROR_FAILURE;
  
  nsCAutoString   tempString((const char*)&resultString[1], resultString[0]);
  const char*     atString = "@";
  PRInt32         atOffset = tempString.Find(atString);
  if (atOffset != kNotFound)
  {
    nsCAutoString domainString;
    tempString.Right(domainString, atOffset + 1);
    *aDomain = domainString.ToNewCString();
    return NS_OK;
  }

  // no domain in the pref
  return NS_ERROR_FAILURE;
}

#pragma mark -


//-----------------------------------------------------------
PRUnichar* nsUserInfo::PStringToNewUCS2(ConstStr255Param str)
//-----------------------------------------------------------
{
  NS_ConvertASCIItoUCS2   tempString((const char*)&str[1], str[0]);
  return tempString.ToNewUnicode();
}


//-----------------------------------------------------------
OSType nsUserInfo::GetAppCreatorCode()
//-----------------------------------------------------------
{
  ProcessSerialNumber psn = { 0, kCurrentProcess } ;
  ProcessInfoRec      procInfo;
  
  procInfo.processInfoLength = sizeof(ProcessInfoRec);
  procInfo.processName = nsnull;
  procInfo.processAppSpec = nsnull;
  
  GetProcessInformation(&psn, &procInfo);
  return procInfo.processSignature;  
}


//-----------------------------------------------------------
nsresult nsUserInfo::EnsureInitted()
//-----------------------------------------------------------
{
  if (mInitted)
    return (mInstance) ? NS_OK : NS_ERROR_NOT_INITIALIZED;
  
  mInitted = PR_TRUE;     // shows that we've tried to init

  if ((long)ICStart == kUnresolvedCFragSymbolAddress)
    return NS_ERROR_FAILURE;
  
  OSType    creator = GetAppCreatorCode();
  ICError   err = ::ICStart(&mInstance, creator);
  if (err != noErr) return NS_ERROR_FAILURE;
  
  err = ::ICFindConfigFile(mInstance, 0, nsnull);
  if (err != noErr) {
    ::ICStop(mInstance);
    mInstance = 0;
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

