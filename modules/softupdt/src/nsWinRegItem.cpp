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

#include "nsWinRegItem.h"
#include "xp.h"
#include "xp_str.h"
#include <windows.h>

PR_BEGIN_EXTERN_C

/* Public Methods */

nsWinRegItem::nsWinRegItem(nsWinReg* regObj, PRInt32 root, PRInt32 action, char* sub, char* valname, void* val)
: nsInstallObject(regObj->softUpdate())
{
	reg = regObj;
	command = action;
	rootkey = root;

	/* I'm assuming we need to copy these */
	subkey = XP_STRDUP(sub);
	name = XP_STRDUP(valname);
	value = XP_STRDUP((char*)val);

}

char* nsWinRegItem::Complete()
{
	switch (command) {
		case NS_WIN_REG_CREATE:
			reg->finalCreateKey(rootkey, subkey, name);
            break;
        case NS_WIN_REG_DELETE:
            reg->finalDeleteKey(rootkey, subkey);
            break;
        case NS_WIN_REG_DELETE_VAL:
            reg->finalDeleteValue(rootkey, subkey, name);
            break;
        case NS_WIN_REG_SET_VAL_STRING:
            reg->finalSetValueString(rootkey, subkey, name, (char*)value);
            break;
        case NS_WIN_REG_SET_VAL:
            reg->finalSetValue(rootkey, subkey, name, (nsWinRegValue*)value);
            break;
        }
	return NULL;
}
  
float nsWinRegItem::GetInstallOrder()
{
	return 3;
}

#define kCRK "Create Registry Key "
#define kDRK "Delete Registry key "
#define kDRV "Delete Registry value "
#define kSRV "Store Registry value "
#define kUNK "Unknown "

char* nsWinRegItem::toString()
{
	char* keyString;
	char* result;
	PRInt32 len;

	switch(command)
	{
	case NS_WIN_REG_CREATE:
		keyString = keystr(rootkey, subkey, NULL);
		len = XP_STRLEN(kCRK) + XP_STRLEN(keyString);
		result = (char*)XP_ALLOC((len+1)*sizeof(char));
		XP_STRCAT(result, kCRK);
		XP_STRCAT(result, keyString);
		XP_FREE(keyString);
		return result;
	case NS_WIN_REG_DELETE:
		keyString = keystr(rootkey, subkey, NULL);
		len = XP_STRLEN(kDRK) + XP_STRLEN(keyString);
		result = (char*)XP_ALLOC((len+1)*sizeof(char));
		XP_STRCAT(result, kDRK);
		XP_STRCAT(result, keyString);
		XP_FREE(keyString);
		return result;
	case NS_WIN_REG_DELETE_VAL:
		keyString = keystr(rootkey, subkey, name);
		len = XP_STRLEN(kDRV) + XP_STRLEN(keyString);
		result = (char*)XP_ALLOC((len+1)*sizeof(char));
		XP_STRCAT(result, kDRV);
		XP_STRCAT(result, keyString);
		XP_FREE(keyString);
		return result;
	case NS_WIN_REG_SET_VAL_STRING:
		keyString = keystr(rootkey, subkey, name);
		len = XP_STRLEN(kSRV) + XP_STRLEN(keyString);
		result = (char*)XP_ALLOC((len+1)*sizeof(char));
		XP_STRCAT(result, kSRV);
		XP_STRCAT(result, keyString);
		XP_FREE(keyString);
		return result;
	case NS_WIN_REG_SET_VAL:
		keyString = keystr(rootkey, subkey, name);
		len = XP_STRLEN(kSRV) + XP_STRLEN(keyString);
		result = (char*)XP_ALLOC((len+1)*sizeof(char));
		XP_STRCAT(result, kSRV);
		XP_STRCAT(result, keyString);
		XP_FREE(keyString);
		return result;
	default:
		keyString = keystr(rootkey, subkey, name);
		len = XP_STRLEN(kUNK) + XP_STRLEN(keyString);
		result = (char*)XP_ALLOC((len+1)*sizeof(char));
		XP_STRCAT(result, kUNK);
		XP_STRCAT(result, keyString);
		XP_FREE(keyString);
		return result;
	}
}

char* nsWinRegItem::Prepare()
{
	return NULL;
}

void nsWinRegItem::Abort()
{
}

/* Private Methods */

char* nsWinRegItem::keystr(PRInt32 root, char* subkey, char* name)
{
	char* rootstr;
	char* rootnum;
	char* finalstr;
	PRInt32 rootlen, finallen;

	switch(root)
	{
	case (int)(HKEY_CLASSES_ROOT) :
		rootstr = XP_STRDUP("\\HKEY_CLASSES_ROOT\\");
		break;
	case (int)(HKEY_CURRENT_USER) :
		rootstr = XP_STRDUP("\\HKEY_CURRENT_USER\\");
		break;
	case (int)(HKEY_LOCAL_MACHINE) :
		rootstr = XP_STRDUP("\\HKEY_LOCAL_MACHINE\\");
		break;
	case (int)(HKEY_USERS) :
		rootstr = XP_STRDUP("\\HKEY_USERS\\");
		break;
	default:
		rootnum = itoa(root);
		rootlen = XP_STRLEN(rootnum) + XP_STRLEN("\\#\\");
		rootstr = (char*)XP_ALLOC((rootlen+1)*sizeof(char));
		XP_STRCAT(rootstr, "\\#");
		XP_STRCAT(rootstr, rootnum);
		XP_STRCAT(rootstr, "\\");
		break;
	}
	if(name == NULL)
	{
		finallen = XP_STRLEN(rootstr) + XP_STRLEN(subkey);
		finalstr = (char*)XP_ALLOC((finallen+1)*sizeof(char));
		XP_STRCAT(finalstr, rootstr);
		XP_STRCAT(finalstr, subkey);
		XP_FREE(rootstr);
		return finalstr;
	}
	else
	{
		finallen = XP_STRLEN(rootstr) + XP_STRLEN(subkey) + XP_STRLEN(name) + 3;
		finalstr = (char*)XP_ALLOC((finallen+1)*sizeof(char));
		XP_STRCAT(finalstr, rootstr);
		XP_STRCAT(finalstr, subkey);
		XP_STRCAT(finalstr, " [");
		XP_STRCAT(finalstr, name);
		XP_STRCAT(finalstr, "]");
		XP_FREE(rootstr);
		return finalstr;
	}
}


char* nsWinRegItem::itoa(PRInt32 n)
{
	char* s;
	int i, sign;
	if((sign = n) < 0)
		n = -n;
	i = 0;
	
	s = (char*)XP_ALLOC(sizeof(char));

	do
		{
		s = (char*)XP_REALLOC(s, (i+1)*sizeof(char));
		s[i++] = n%10 + '0';
		s[i] = '\0';
		} while ((n/=10) > 0);
		
	if(sign < 0)
	{
		s = (char*)XP_REALLOC(s, (i+1)*sizeof(char));
		s[i++] = '-';
	}
	s[i]  = '\0';
	reverseString(s);
	return s;
}

void nsWinRegItem::reverseString(char* s)
{
	int c, i, j;
	
	for(i=0, j=strlen(s)-1; i<j; i++, j--)
		{
		c = s[i];
		s[i] = s[j];
		s[j] = c;
		}
}

/* CanUninstall
* WinRegItem() does not install any files which can be uninstalled,
* hence this function returns false. 
*/
PRBool
nsWinRegItem:: CanUninstall()
{
    return FALSE;
}

/* RegisterPackageNode
* WinRegItem() installs files which need to be registered,
* hence this function returns true.
*/
PRBool
nsWinRegItem:: RegisterPackageNode()
{
    return TRUE;
}


PR_END_EXTERN_C
