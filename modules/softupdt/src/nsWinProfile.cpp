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
#include "xp.h"
#include "xp_str.h"

PR_BEGIN_EXTERN_C

/* Public Methods */

nsWinProfile::nsWinProfile( nsSoftwareUpdate* suObj, nsFolderSpec* folder, char* file )
{
	filename = folder->MakeFullPath(file, NULL); /* can I pass NULL here? */
	su = suObj;
	principal = suObj->GetPrincipal();
	privMgr = nsPrivilegeManager::getPrivilegeManager();
	impersonation = nsTarget::findTarget(IMPERSONATOR);
	target = (nsUserTarget*)nsTarget::findTarget(INSTALL_PRIV);
}

PRBool nsWinProfile::writeString( char* section, char* key, char* value )
{
	if(resolvePrivileges())
	{
		nsWinProfileItem* wi = new nsWinProfileItem(this, section, key, value);
		if(wi == NULL)
			return FALSE;
		su->ScheduleForInstall(wi);
		return TRUE;
		}
	else
		return FALSE;

}

char* nsWinProfile::getString( char* section, char* key )
{
	if(resolvePrivileges())
	{
		return nativeGetString(section, key);
	}
	else
		return NULL;
}

char* nsWinProfile::getFilename()
{
	return filename;
}

nsSoftwareUpdate* nsWinProfile::softUpdate()
{
	return su;
}

int nsWinProfile::finalWriteString( char* section, char* key, char* value )
{
	/* do we need another security check here? */
	return nativeWriteString(section, key, value);
}

/* Private Methods */

int nsWinProfile::nativeWriteString( char* section, char* key, char* value )
{
    int     success = 0;

	/* make sure conversions worked */
    if ( section != NULL && key != NULL && filename != NULL )
		success = WritePrivateProfileString( section, key, value, filename );

    return success;
}

#define STRBUFLEN 255
  
char* nsWinProfile::nativeGetString( char* section, char* key )
{
	int     numChars;
    char    valbuf[STRBUFLEN];
    char* value = NULL;

    /* make sure conversions worked */
    if ( section != NULL && key != NULL && filename != NULL ) {
        numChars = GetPrivateProfileString( section, key, "",
                    valbuf, STRBUFLEN, filename );

        /* if the value fit in the buffer */
        if ( numChars < STRBUFLEN ) {
            value = XP_STRDUP(valbuf);
        }
    }

    return value;
}

PRBool nsWinProfile::resolvePrivileges()
{
	if(privMgr->enablePrivilege(impersonation, 1) && 
		privMgr->enablePrivilege(target, principal, 1))
		return TRUE;
	else
		return FALSE;
}

PR_END_EXTERN_C
