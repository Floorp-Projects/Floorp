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

#include "nsWinProfile.h"
#include "nsWinProfileItem.h"
#include "nspr.h"
#include <windows.h>

/* Public Methods */

nsWinProfile::nsWinProfile( nsSoftwareUpdate* suObj, nsFolderSpec* folder, char* file )
{
	filename = folder->MakeFullPath(file, NULL); /* can I pass NULL here? */
	su = suObj;

//	principal = suObj->GetPrincipal();
//	privMgr = nsPrivilegeManager::getPrivilegeManager();
//	impersonation = nsTarget::findTarget(IMPERSONATOR);
//	target = (nsUserTarget*)nsTarget::findTarget(INSTALL_PRIV);
}

PRInt32
nsWinProfile::getString(nsString section, nsString key, nsString* aReturn )
{
  *aReturn = nativeGetString(section, key);
  return NS_OK;
}

PRInt32
nsWinProfile::writeString( char* section, char* key, char* value, PRInt32* aReturn )
{
  nsWinProfileItem* wi = new nsWinProfileItem(this, section, key, value);

  if(wi == NULL)
    return PR_FALSE;

  su->ScheduleForInstall(wi);
  return NS_OK;
}

nsString* nsWinProfile::getFilename()
{
	return filename;
}

nsInstall* nsWinProfile::installObject()
{
	return su;
}

PRInt32
nsWinProfile::finalWriteString( char* section, char* key, char* value )
{
	/* do we need another security check here? */
	return nativeWriteString(section, key, value);
}

/* Private Methods */

PRInt32
nsWinProfile::nativeWriteString( char* section, char* key, char* value )
{
  int success = 0;

	/* make sure conversions worked */
  if(section != NULL && key != NULL && filename != NULL)
    success = WritePrivateProfileString( section, key, value, filename );

  return success;
}

#define STRBUFLEN 255
  
char* nsWinProfile::nativeGetString(nsString section, nsString key )
{
	int   numChars;
  char  valbuf[STRBUFLEN];
  char* value = NULL;

  /* make sure conversions worked */
  if(section != "nsnull" && key != "nsnull" && filename != NULL)
  {
    numChars = GetPrivateProfileString( section, key, "", valbuf, STRBUFLEN, filename );

    /* if the value fit in the buffer */
    if(numChars < STRBUFLEN)
    {
      value = XP_STRDUP(valbuf);
    }
  }

  return value;
}

