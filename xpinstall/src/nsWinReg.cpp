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

#include "nsWinReg.h"
#include "nsWinRegItem.h"
#include <windows.h> /* is this needed? */

/* Public Methods */

nsWinReg::nsWinReg(nsInstall* suObj)
{
	su      = suObj;
	rootkey = (PRInt32)HKEY_CLASSES_ROOT;
}

PRInt32
nsWinReg::setRootKey(PRInt32 key)
{
	rootkey = key;
  return NS_OK;
}
  
PRInt32
nsWinReg::createKey(const nsString& subkey, const nsString& classname, PRInt32* aReturn)
{
	nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_CREATE, subkey, classname, "null");

	if(wi == nsnull)
  {
    return NS_OK;
  }
	su->ScheduleForInstall(wi);
	return 0;
}
  
PRInt32
nsWinReg::deleteKey(const nsString& subkey, PRInt32* aReturn)
{
	nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_DELETE, subkey, "null", "null");

	if(wi == nsnull)
  {
    return NS_OK;
  }
	su->ScheduleForInstall(wi);
	return 0;
}

PRInt32
nsWinReg::deleteValue(const nsString& subkey, const nsString& valname, PRInt32* aReturn)
{
	nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_DELETE_VAL, subkey, valname, "null");

	if(wi == nsnull)
  {
    return NS_OK;
  }
	su->ScheduleForInstall(wi);
	return 0;
}

PRInt32
nsWinReg::setValueString(const nsString& subkey, const nsString& valname, const nsString& value, PRInt32* aReturn)
{
	nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_SET_VAL_STRING, subkey, valname, value);

	if(wi == nsnull)
  {
    return NS_OK;
  }
	su->ScheduleForInstall(wi);
	return 0;
}

PRInt32
nsWinReg::getValueString(const nsString& subkey, const nsString& valname, nsString* aReturn)
{
  nativeGetValueString(subkey, valname, aReturn);

  return NS_OK;
}
 
PRInt32
nsWinReg::setValueNumber(const nsString& subkey, const nsString& valname, PRInt32 value, PRInt32* aReturn)
{
	nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_SET_VAL_NUMBER, subkey, valname, value);

	if(wi == nsnull)
  {
    return NS_OK;
  }
	su->ScheduleForInstall(wi);
	return 0;
}

PRInt32
nsWinReg::getValueNumber(const nsString& subkey, const nsString& valname, PRInt32* aReturn)
{
  nativeGetValueNumber(subkey, valname, aReturn);

  return NS_OK;
}
 
PRInt32
nsWinReg::setValue(const nsString& subkey, const nsString& valname, nsWinRegValue* value, PRInt32* aReturn)
{
  // fix: need to figure out what to do with nsWinRegValue class.
  //
	// nsWinRegItem* wi = new nsWinRegItem(this, rootkey, NS_WIN_REG_SET_VAL, subkey, valname, (nsWinRegValue*)value);
  //
	// if(wi == nsnull)
  // {
  //   return NS_OK;
  // }
	// su->ScheduleForInstall(wi);
	return 0;
}
  
PRInt32
nsWinReg::getValue(const nsString& subkey, const nsString& valname, nsWinRegValue** aReturn)
{
  // fix:
  return NS_OK;
}
  
nsInstall* nsWinReg::installObject()
{
	return su;
}
  
PRInt32
nsWinReg::finalCreateKey(PRInt32 root, const nsString& subkey, const nsString& classname, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeCreateKey(subkey, classname);
  return NS_OK;
}
  
PRInt32
nsWinReg::finalDeleteKey(PRInt32 root, const nsString& subkey, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeDeleteKey(subkey);
  return NS_OK;
}
  
PRInt32
nsWinReg::finalDeleteValue(PRInt32 root, const nsString& subkey, const nsString& valname, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeDeleteValue(subkey, valname);
  return NS_OK;
}

PRInt32
nsWinReg::finalSetValueString(PRInt32 root, const nsString& subkey, const nsString& valname, const nsString& value, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeSetValueString(subkey, valname, value);
  return NS_OK;
}
 
PRInt32
nsWinReg::finalSetValueNumber(PRInt32 root, const nsString& subkey, const nsString& valname, PRInt32 value, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeSetValueNumber(subkey, valname, value);
  return NS_OK;
}
 
PRInt32
nsWinReg::finalSetValue(PRInt32 root, const nsString& subkey, const nsString& valname, nsWinRegValue* value, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeSetValue(subkey, valname, value);
  return NS_OK;
}


/* Private Methods */

PRInt32
nsWinReg::nativeCreateKey(const nsString& subkey, const nsString& classname)
{
    HKEY    root, newkey;
    LONG    result;
    ULONG   disposition;
    char*   subkeyCString     = subkey.ToNewCString();
    char*   classnameCString  = classname.ToNewCString();

#ifdef WIN32
    root   = (HKEY)rootkey;
    result = RegCreateKeyEx(root, subkeyCString, 0, classnameCString, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nsnull, &newkey, &disposition);

    if(ERROR_SUCCESS == result)
    {
        RegCloseKey( newkey );
    }
#endif

    delete [] subkeyCString;
    delete [] classnameCString;

    return result;
}

PRInt32
nsWinReg::nativeDeleteKey(const nsString& subkey)
{
    HKEY  root;
    LONG  result;
    char* subkeyCString = subkey.ToNewCString();

#ifdef WIN32
    root   = (HKEY) rootkey;
    result = RegDeleteKey( root, subkeyCString );
#endif

    delete [] subkeyCString;

    return result;
}
  
PRInt32
nsWinReg::nativeDeleteValue(const nsString& subkey, const nsString& valname)
{
#if defined (WIN32) || defined (XP_OS2)
    HKEY    root, newkey;
    LONG    result;
    char*   subkeyCString   = subkey.ToNewCString();
    char*   valnameCString  = valname.ToNewCString();

    root   = (HKEY) rootkey;
    result = RegOpenKeyEx( root, subkeyCString, 0, KEY_WRITE, &newkey);

    if ( ERROR_SUCCESS == result )
    {
        result = RegDeleteValue( newkey, valnameCString );
        RegCloseKey( newkey );
    }

    delete [] subkeyCString;
    delete [] valnameCString;

    return result;
#else
    return ERROR_INVALID_PARAMETER;
#endif
}

PRInt32
nsWinReg::nativeSetValueString(const nsString& subkey, const nsString& valname, const nsString& value)
{
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length;

    char*   subkeyCString   = subkey.ToNewCString();
    char*   valnameCString  = valname.ToNewCString();
    char*   valueCString    = value.ToNewCString();
    
    length = subkey.Length();

    root   = (HKEY) rootkey;
    result = RegOpenKeyEx( root, subkeyCString, 0, KEY_ALL_ACCESS, &newkey);

    if(ERROR_SUCCESS == result)
    {
        result = RegSetValueEx( newkey, valnameCString, 0, REG_SZ, (unsigned char*)valueCString, length );
        RegCloseKey( newkey );
    }

    delete [] subkeyCString;
    delete [] valnameCString;
    delete [] valueCString;

    return result;
}
 
#define STRBUFLEN 255
 
void
nsWinReg::nativeGetValueString(const nsString& subkey, const nsString& valname, nsString* aReturn)
{
    unsigned char     valbuf[_MAXKEYVALUE_];
    HKEY              root;
    HKEY              newkey;
    LONG              result;
    DWORD             type            = REG_SZ;
    DWORD             length          = _MAXKEYVALUE_;
    char*             subkeyCString   = subkey.ToNewCString();
    char*             valnameCString  = valname.ToNewCString();

    root   = (HKEY) rootkey;
    result = RegOpenKeyEx( root, subkeyCString, 0, KEY_READ, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegQueryValueEx( newkey, valnameCString, nsnull, &type, valbuf, &length );

        RegCloseKey( newkey );
    }

    if(ERROR_SUCCESS == result && type == REG_SZ)
    {
        *aReturn = (char*)valbuf;
    }

    delete [] subkeyCString;
    delete [] valnameCString;
}

PRInt32
nsWinReg::nativeSetValueNumber(const nsString& subkey, const nsString& valname, PRInt32 value)
{
    HKEY    root;
    HKEY    newkey;
    LONG    result;

    char*   subkeyCString   = subkey.ToNewCString();
    char*   valnameCString  = valname.ToNewCString();
    
    root   = (HKEY) rootkey;
    result = RegOpenKeyEx( root, subkeyCString, 0, KEY_ALL_ACCESS, &newkey);

    if(ERROR_SUCCESS == result)
    {
        result = RegSetValueEx( newkey, valnameCString, 0, REG_DWORD, (LPBYTE)&value, sizeof(PRInt32));
        RegCloseKey( newkey );
    }

    delete [] subkeyCString;
    delete [] valnameCString;

    return result;
}
 
void
nsWinReg::nativeGetValueNumber(const nsString& subkey, const nsString& valname, PRInt32* aReturn)
{
    PRInt32 valbuf;
    PRInt32 valbuflen;
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   type            = REG_DWORD;
    DWORD   length          = _MAXKEYVALUE_;
    char*   subkeyCString   = subkey.ToNewCString();
    char*   valnameCString  = valname.ToNewCString();

    valbuflen = sizeof(PRInt32);
    root      = (HKEY) rootkey;
    result    = RegOpenKeyEx( root, subkeyCString, 0, KEY_READ, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegQueryValueEx( newkey, valnameCString, nsnull, &type, (LPBYTE)&valbuf, (LPDWORD)&valbuflen);

        RegCloseKey( newkey );
    }

    if(ERROR_SUCCESS == result && type == REG_DWORD)
    {
        *aReturn = valbuf;
    }

    delete [] subkeyCString;
    delete [] valnameCString;
}

PRInt32
nsWinReg::nativeSetValue(const nsString& subkey, const nsString& valname, nsWinRegValue* value)
{
#if defined (WIN32) || defined (XP_OS2)
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length;
    DWORD   type;
    unsigned char*    data;
    char*   subkeyCString   = subkey.ToNewCString();
    char*   valnameCString  = valname.ToNewCString();


    root   = (HKEY) rootkey;
    result = RegOpenKeyEx( root, subkeyCString, 0, KEY_ALL_ACCESS, &newkey );

    if(ERROR_SUCCESS == result)
    {
        type = (DWORD)value->type;
        data = (unsigned char*)value->data;
        length = (DWORD)value->data_length;

        result = RegSetValueEx( newkey, valnameCString, 0, type, data, length);
        RegCloseKey( newkey );
    }

    delete [] subkeyCString;
    delete [] valnameCString;

    return result;
#else
    return ERROR_INVALID_PARAMETER;
#endif
}
  
nsWinRegValue*
nsWinReg::nativeGetValue(const nsString& subkey, const nsString& valname)
{
#if defined (WIN32) || defined (XP_OS2)
    unsigned char    valbuf[STRBUFLEN];
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length=STRBUFLEN;
    DWORD   type;
    nsString* data;
    nsWinRegValue* value = nsnull;
    char*   subkeyCString   = subkey.ToNewCString();
    char*   valnameCString  = valname.ToNewCString();

    root   = (HKEY) rootkey;
    result = RegOpenKeyEx( root, subkeyCString, 0, KEY_ALL_ACCESS, &newkey );

    if(ERROR_SUCCESS == result)
    {
        result = RegQueryValueEx( newkey, valnameCString, nsnull, &type, valbuf, &length );

        if ( ERROR_SUCCESS == result ) {
            data = new nsString((char*)valbuf);
			      length = data->Length();
            value = new nsWinRegValue(type, (void*)data, length);
        }

        RegCloseKey( newkey );
    }

    delete [] subkeyCString;
    delete [] valnameCString;

    return value;
#else
    return nsnull;
#endif
}

