/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "nsWinProfileItem.h"
#include "nspr.h"
#include <windows.h>

/* Public Methods */

nsWinProfileItem::nsWinProfileItem(nsWinProfile* profileObj, 
                                   nsString sectionName,
                                   nsString keyName,
                                   nsString val) : nsInstallObject(profileObj->InstallObject())
{
  mProfile = profileObj;
  mSection = new nsString(sectionName);
  mKey     = new nsString(keyName);
  mValue   = new nsString(val);
}

nsWinProfileItem::~nsWinProfileItem()
{
  if (mSection) delete mSection;
  if (mKey)     delete mKey;
  if (mValue)   delete mValue;
}

PRInt32 nsWinProfileItem::Complete()
{
	if (mProfile) 
        mProfile->FinalWriteString(*mSection, *mKey, *mValue);
	
    return NS_OK;
}
  
PRUnichar* nsWinProfileItem::toString()
{
  PRUnichar*     resultCString;
  
  nsString* filename = new nsString(*mProfile->GetFilename());
  nsString* result = new nsString("Write ");

  if (filename == nsnull || result == nsnull)
      return nsnull;

  result->Append(*filename);
  result->Append(": [");
  result->Append(*mSection);
  result->Append("] ");
  result->Append(*mKey);
  result->Append("=");
  result->Append(*mValue);

  resultCString = (PRUnichar *)result->ToNewCString();
  
  delete result;
  delete filename;

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

