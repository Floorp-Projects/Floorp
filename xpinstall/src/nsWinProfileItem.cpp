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
                                   nsString val) : nsInstallObject(profileObj->installObject())
{
  profile = profileObj;
  section = new nsString(sectionName);
  key     = new nsString(keyName);
  value   = new nsString(val);
}

nsWinProfileItem::~nsWinProfileItem()
{
  delete profile;
  delete section;
  delete key;
  delete value;
}

PRInt32 nsWinProfileItem::Complete()
{
	profile->finalWriteString(*section, *key, *value);
	return NS_OK;
}
  
float nsWinProfileItem::GetInstallOrder()
{
	return 4;
}

char* nsWinProfileItem::toString()
{
  char*     resultCString;
  nsString* result;
  nsString* filename = new nsString(*profile->getFilename());

  result = new nsString("Write ");
  result->Append(*filename);
  result->Append(": [");
  result->Append(*section);
  result->Append("] ");
  result->Append(*key);
  result->Append("=");
  result->Append(*value);

  resultCString = result->ToNewCString();
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
    return FALSE;
}

/* RegisterPackageNode
* WinProfileItem() installs files which need to be registered,
* hence this function returns true.
*/
PRBool
nsWinProfileItem::RegisterPackageNode()
{
    return TRUE;
}

