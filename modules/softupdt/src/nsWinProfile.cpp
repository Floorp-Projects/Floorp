/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
