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
#include "xp.h"
#include "xp_str.h"

PR_BEGIN_EXTERN_C

/* Public Methods */

nsWinProfileItem::nsWinProfileItem(nsWinProfile* profileObj, 
								   char* sectionName, char* keyName,
								   char* val) : nsInstallObject(profileObj->softUpdate())
{
	profile = profileObj;
	section = XP_STRDUP(sectionName);
	key = XP_STRDUP(keyName);
	value = XP_STRDUP(val);
}

char* nsWinProfileItem::Complete()
{
	profile->finalWriteString(section, key, value);
	return NULL;
}
  
float nsWinProfileItem::GetInstallOrder()
{
	return 4;
}

char* nsWinProfileItem::toString()
{
	PRInt32 len;
	char* result;
	char* filename = profile->getFilename();

	len = XP_STRLEN("Write ") + XP_STRLEN(filename) +
		  XP_STRLEN(": [") + XP_STRLEN(section) + XP_STRLEN("] ") +
		  XP_STRLEN(key) + XP_STRLEN("=") + XP_STRLEN(value);

	result = (char*)XP_ALLOC((len+1)*sizeof(char));
	XP_STRCAT(result, "Write ");
	XP_STRCAT(result, filename);
	XP_STRCAT(result, ": [");
	XP_STRCAT(result, section);
	XP_STRCAT(result, "] ");
	XP_STRCAT(result, key);
	XP_STRCAT(result, "=");
	XP_STRCAT(result, value);

	return result;
}

void nsWinProfileItem::Abort()
{
}

char* nsWinProfileItem::Prepare()
{
	return NULL;
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


PR_END_EXTERN_C
