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

nsWinProfile::nsWinProfile( nsInstall* suObj, const nsString& folder, const nsString& file )
{
  filename = new nsString(folder);
  if(filename->Last() != '\\')
  {
    filename->Append("\\");
  }
  filename->Append(file);

	su = suObj;

//	principal = suObj->GetPrincipal();
//	privMgr = nsPrivilegeManager::getPrivilegeManager();
//	impersonation = nsTarget::findTarget(IMPERSONATOR);
//	target = (nsUserTarget*)nsTarget::findTarget(INSTALL_PRIV);
}

nsWinProfile::~nsWinProfile()
{
  delete filename;
}

PRInt32
nsWinProfile::getString(nsString section, nsString key, nsString* aReturn)
{
  return nativeGetString(section, key, aReturn);
}

PRInt32
nsWinProfile::writeString(nsString section, nsString key, nsString value, PRInt32* aReturn)
{
  nsWinProfileItem* wi = new nsWinProfileItem(this, section, key, value);

  *aReturn = NS_OK;

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
nsWinProfile::finalWriteString( nsString section, nsString key, nsString value )
{
	/* do we need another security check here? */
	return nativeWriteString(section, key, value);
}

/* Private Methods */

#define STRBUFLEN 255
  
PRInt32
nsWinProfile::nativeGetString(nsString section, nsString key, nsString* aReturn )
{
	int       numChars;
  char      valbuf[STRBUFLEN];
  char*     sectionCString;
  char*     keyCString;
  char*     filenameCString;

  /* make sure conversions worked */
  if(section.First() != '\0' && key.First() != '\0' && filename->First() != '\0')
  {
    sectionCString  = section.ToNewCString();
    keyCString      = key.ToNewCString();
    filenameCString = filename->ToNewCString();

    numChars        = GetPrivateProfileString(sectionCString, keyCString, "", valbuf, STRBUFLEN, filenameCString);

    *aReturn        = valbuf;

    delete [] sectionCString;
    delete [] keyCString;
    delete [] filenameCString;
  }

  return numChars;
}

PRInt32
nsWinProfile::nativeWriteString( nsString section, nsString key, nsString value )
{
  char* sectionCString;
  char* keyCString;
  char* valueCString;
  char* filenameCString;
  int   success = 0;

	/* make sure conversions worked */
  if(section.First() != '\0' && key.First() != '\0' && filename->First() != '\0')
  {
    sectionCString  = section.ToNewCString();
    keyCString      = key.ToNewCString();
    valueCString    = value.ToNewCString();
    filenameCString = filename->ToNewCString();

    success = WritePrivateProfileString( sectionCString, keyCString, valueCString, filenameCString );

    delete [] sectionCString;
    delete [] keyCString;
    delete [] valueCString;
    delete [] filenameCString;
  }

  return success;
}

