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

#include "nsWinReg.h"
#include "nsWinRegItem.h"
#ifdef WIN32
#include <windows.h> /* is this needed? */
#endif
#include "xp.h"
#include "xp_str.h"

PR_BEGIN_EXTERN_C

/* Public Methods */

nsWinReg::nsWinReg(nsSoftwareUpdate* suObj)
{
	su = suObj;
	principal = suObj->GetPrincipal();
	privMgr = nsPrivilegeManager::getPrivilegeManager();
	impersonation = nsTarget::findTarget(IMPERSONATOR);
	target = (nsUserTarget*)nsTarget::findTarget(INSTALL_PRIV);
}

void nsWinReg::setRootKey(PRInt32 key)
{
	rootkey = key;
}
  
PRInt32 nsWinReg::createKey(char* subkey, char* classname)
{
	resolvePrivileges();

	nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_CREATE, subkey, classname, NULL);
	if(wi == NULL)
		return (-1);
	su->ScheduleForInstall(wi);
	return 0;
}
  
PRInt32 nsWinReg::deleteKey(char* subkey)
{
	resolvePrivileges();

	nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_DELETE, subkey, NULL, NULL);
	if(wi == NULL)
		return (-1);
	su->ScheduleForInstall(wi);
	return 0;
}

PRInt32 nsWinReg::deleteValue(char* subkey, char* valname)
{
	resolvePrivileges();

	nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_DELETE_VAL, subkey, valname, NULL);
	if(wi == NULL)
		return (-1);
	su->ScheduleForInstall(wi);
	return 0;
}

PRInt32 nsWinReg::setValueString(char* subkey, char* valname, char* value)
{
	resolvePrivileges();

	nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_SET_VAL_STRING, subkey, valname, (char*) value);
	if(wi == NULL)
		return (-1);
	su->ScheduleForInstall(wi);
	return 0;
}

char* nsWinReg::getValueString(char* subkey, char* valname)
{
	resolvePrivileges();

	return nativeGetValueString(subkey, valname);
}
  
PRInt32 nsWinReg::setValue(char* subkey, char* valname, nsWinRegValue* value)
{
	resolvePrivileges();

	nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_SET_VAL, subkey, valname, (nsWinRegValue*)value);
	if(wi == NULL)
		return (-1);
	su->ScheduleForInstall(wi);
	return 0;
}
  
nsWinRegValue* nsWinReg::getValue(char* subkey, char* valname)
{
	resolvePrivileges();

	return nativeGetValue(subkey, valname);
}
  
nsSoftwareUpdate* nsWinReg::softUpdate()
{
	return su;
}
  
PRInt32 nsWinReg::finalCreateKey(PRInt32 root, char* subkey, char* classname)
{
	setRootKey(root);
	return nativeCreateKey(subkey, classname);
}
  
PRInt32 nsWinReg::finalDeleteKey(PRInt32 root, char* subkey)
{
	setRootKey(root);
	return nativeDeleteKey(subkey);
}
  
PRInt32 nsWinReg::finalDeleteValue(PRInt32 root, char* subkey, char* valname)
{
	setRootKey(root);
	return nativeDeleteValue(subkey, valname);
}
  
PRInt32 nsWinReg::finalSetValueString(PRInt32 root, char* subkey, char* valname, char* value)
{
	setRootKey(root);
	return nativeSetValueString(subkey, valname, value);
}
 
PRInt32 nsWinReg::finalSetValue(PRInt32 root, char* subkey, char* valname, nsWinRegValue* value)
{
	setRootKey(root);
	return nativeSetValue(subkey, valname, value);
}


/* Private Methods */

PRInt32 nsWinReg::nativeCreateKey(char* subkey, char* classname)
{
    HKEY    root, newkey;
    LONG    result;
    ULONG    disposition;

#ifdef WIN32
    root  = (HKEY) rootkey;

    result = RegCreateKeyEx( root, subkey, 0, classname, REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS, NULL, &newkey, &disposition );
#else
    result = RegCreateKey( HKEY_CLASSES_ROOT, subkey, &newkey );
#endif

    if (ERROR_SUCCESS == result ) {
        RegCloseKey( newkey );
    }

    return result;
}

PRInt32 nsWinReg::nativeDeleteKey(char* subkey)
{
    HKEY    root;

#ifdef WIN32
    root    = (HKEY) rootkey;
#else
    root    = HKEY_CLASSES_ROOT;
#endif

    return RegDeleteKey( root, subkey );
}
  
PRInt32 nsWinReg::nativeDeleteValue(char* subkey, char* valname)
{
#if defined (WIN32) || defined (XP_OS2)
    HKEY    root, newkey;
    LONG    result;

    root    = (HKEY) rootkey;

    result = RegOpenKeyEx( root, subkey, 0, KEY_WRITE, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegDeleteValue( newkey, valname );

        RegCloseKey( newkey );
    }

    return result;
#else
    return ERROR_INVALID_PARAMETER;
#endif
}

PRInt32 nsWinReg::nativeSetValueString(char* subkey, char* valname, char* value)
{
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length;
    
#ifdef WIN32

    root    = (HKEY) rootkey;

    result = RegOpenKeyEx( root, subkey, 0, KEY_ALL_ACCESS, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegSetValueEx( newkey, valname, 0, REG_SZ, (unsigned char*)value, length );

        RegCloseKey( newkey );
    }
#else
    result = RegSetValue( HKEY_CLASSES_ROOT, subkey, REG_SZ, value, length );
#endif

    return result;
}
 
#define STRBUFLEN 255
 
char* nsWinReg::nativeGetValueString(char* subkey, char* valname)
{
    unsigned char    valbuf[STRBUFLEN];
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   type = REG_SZ;
    DWORD   length = STRBUFLEN;
	char*	value;


#ifdef WIN32

    root    = (HKEY) rootkey;

    result = RegOpenKeyEx( root, subkey, 0, KEY_ALL_ACCESS, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegQueryValueEx( newkey, valname, NULL, &type, valbuf, &length );

        RegCloseKey( newkey );
    }
#else
    result = RegQueryValue( HKEY_CLASSES_ROOT, subkey, valbuf, &length );
#endif

    if ( ERROR_SUCCESS == result && type == REG_SZ ) {
        value = XP_STRDUP((char*)valbuf);
    }

    return value;
}

  
PRInt32 nsWinReg::nativeSetValue(char* subkey, char* valname, nsWinRegValue* value)
{
#if defined (WIN32) || defined (XP_OS2)
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length;
    DWORD   type;
    unsigned char*    data;


    root    = (HKEY) rootkey;

    result = RegOpenKeyEx( root, subkey, 0, KEY_ALL_ACCESS, &newkey );

    if ( ERROR_SUCCESS == result ) {

        type = (DWORD)value->type;
        data = (unsigned char*)value->data;
        length = (DWORD)value->data_length;


        result = RegSetValueEx( newkey, valname, 0, type, data, length);

        RegCloseKey( newkey );
    }

    return result;
#else
    return ERROR_INVALID_PARAMETER;
#endif
}
  
nsWinRegValue* nsWinReg::nativeGetValue(char* subkey, char* valname)
{
#if defined (WIN32) || defined (XP_OS2)
    unsigned char    valbuf[STRBUFLEN];
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length=STRBUFLEN;
    DWORD   type;
    char*    data;
    nsWinRegValue* value = NULL;

    root    = (HKEY) rootkey;

    result = RegOpenKeyEx( root, subkey, 0, KEY_ALL_ACCESS, &newkey );

    if ( ERROR_SUCCESS == result ) {

        result = RegQueryValueEx( newkey, valname, NULL, &type, valbuf, &length );

        if ( ERROR_SUCCESS == result ) {
            data = XP_STRDUP((char*)valbuf);
			length = XP_STRLEN(data);

            value = new nsWinRegValue(type, (void*)data, length);
        }

        RegCloseKey( newkey );
    }

    return value;
#else
    return NULL;
#endif
}

PRBool nsWinReg::resolvePrivileges()
{
	if(privMgr->enablePrivilege(impersonation, 1) && 
		privMgr->enablePrivilege(target, principal, 1))
		return TRUE;
	else
		return FALSE;
}

PR_END_EXTERN_C
