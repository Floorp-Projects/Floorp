/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 */


#include "nsWinProfileItem.h"
#include "nspr.h"
#include <windows.h>

/* Public Methods */

MOZ_DECL_CTOR_COUNTER(nsWinProfileItem);

nsWinProfileItem::nsWinProfileItem(nsWinProfile* profileObj, 
                                   nsString sectionName,
                                   nsString keyName,
                                   nsString val,
                                   PRInt32 *aReturn) : nsInstallObject(profileObj->InstallObject())
{
  MOZ_COUNT_CTOR(nsWinProfileItem);

  mProfile = profileObj;
  mSection = new nsString(sectionName);
  mKey     = new nsString(keyName);
  mValue   = new nsString(val);

  *aReturn = nsInstall::SUCCESS;

  if((mSection == nsnull) ||
     (mKey     == nsnull) ||
     (mValue   == nsnull))
  {
    *aReturn = nsInstall::OUT_OF_MEMORY;
  }
}

nsWinProfileItem::~nsWinProfileItem()
{
  if (mSection) delete mSection;
  if (mKey)     delete mKey;
  if (mValue)   delete mValue;

  MOZ_COUNT_DTOR(nsWinProfileItem);
}

PRInt32 nsWinProfileItem::Complete()
{
	if (mProfile) 
        mProfile->FinalWriteString(*mSection, *mKey, *mValue);
	
    return NS_OK;
}
  
char* nsWinProfileItem::toString()
{
  char*     resultCString;
  
  nsString* filename = new nsString(*mProfile->GetFilename());
  nsString* result = new nsString;
  result->AssignWithConversion("Write ");

  if (filename == nsnull || result == nsnull)
      return nsnull;

  result->Append(*filename);
  result->AppendWithConversion(": [");
  result->Append(*mSection);
  result->AppendWithConversion("] ");
  result->Append(*mKey);
  result->AppendWithConversion("=");
  result->Append(*mValue);

  resultCString = new char[result->Length() + 1];
  if(resultCString != nsnull)
      result->ToCString(resultCString, result->Length() + 1);
  
  if (result)   delete result;
  if (filename) delete filename;

  return resultCString;
}

void nsWinProfileItem::Abort()
{
}

PRInt32 nsWinProfileItem::Prepare()
{
	return nsnull;
}


/* CanUninstall
* WinProfileItem() does not install any files which can be uninstalled,
* hence this function returns false. 
*/
PRBool 
nsWinProfileItem::CanUninstall()
{
    return PR_FALSE;
}

/* RegisterPackageNode
* WinProfileItem() installs files which need to be registered,
* hence this function returns true.
*/
PRBool
nsWinProfileItem::RegisterPackageNode()
{
    return PR_TRUE;
}

