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
nsWinReg::createKey(nsString subkey, nsString classname, PRInt32* aReturn)
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
nsWinReg::deleteKey(nsString subkey, PRInt32* aReturn)
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
nsWinReg::deleteValue(nsString subkey, nsString valname, PRInt32* aReturn)
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
nsWinReg::setValueString(nsString subkey, nsString valname, nsString value, PRInt32* aReturn)
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
nsWinReg::getValueString(nsString subkey, nsString valname, nsString** aReturn)
{
  // fix:
  return NS_OK;
}
  
PRInt32
nsWinReg::setValue(nsString subkey, nsString valname, nsWinRegValue* value, PRInt32* aReturn)
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
nsWinReg::getValue(nsString subkey, nsString valname, nsWinRegValue** aReturn)
{
  // fix:
  return NS_OK;
}
  
nsInstall* nsWinReg::installObject()
{
	return su;
}
  
PRInt32
nsWinReg::finalCreateKey(PRInt32 root, nsString subkey, nsString classname, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeCreateKey(subkey, classname);
  return NS_OK;
}
  
PRInt32
nsWinReg::finalDeleteKey(PRInt32 root, nsString subkey, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeDeleteKey(subkey);
  return NS_OK;
}
  
PRInt32
nsWinReg::finalDeleteValue(PRInt32 root, nsString subkey, nsString valname, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeDeleteValue(subkey, valname);
  return NS_OK;
}
  
PRInt32 
nsWinReg::finalSetValueString(PRInt32 root, nsString subkey, nsString valname, nsString value, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeSetValueString(subkey, valname, value);
  return NS_OK;
}
 
PRInt32
nsWinReg::finalSetValue(PRInt32 root, nsString subkey, nsString valname, nsWinRegValue* value, PRInt32* aReturn)
{
	setRootKey(root);
	*aReturn = nativeSetValue(subkey, valname, value);
  return NS_OK;
}


/* Private Methods */

PRInt32
nsWinReg::nativeCreateKey(nsString subkey, nsString classname)
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
nsWinReg::nativeDeleteKey(nsString subkey)
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
nsWinReg::nativeDeleteValue(nsString subkey, nsString valname)
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
nsWinReg::nativeSetValueString(nsString subkey, nsString valname, nsString value)
{
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length;

    char*   subkeyCString   = subkey.ToNewCString();
    char*   valnameCString  = valname.ToNewCString();
    char*   valueCString    = value.ToNewCString();
    
    length = subkey.Length();

#ifdef WIN32
    root   = (HKEY) rootkey;
    result = RegOpenKeyEx( root, subkeyCString, 0, KEY_ALL_ACCESS, &newkey);

    if(ERROR_SUCCESS == result)
    {
        result = RegSetValueEx( newkey, valnameCString, 0, REG_SZ, (unsigned char*)valueCString, length );
        RegCloseKey( newkey );
    }
#endif

    delete [] subkeyCString;
    delete [] valnameCString;
    delete [] valueCString;

    return result;
}
 
#define STRBUFLEN 255
 
nsString*
nsWinReg::nativeGetValueString(nsString subkey, nsString valname)
{
    unsigned char    valbuf[STRBUFLEN];
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   type = REG_SZ;
    DWORD   length = STRBUFLEN;
	  nsString* value;
    char*   subkeyCString   = subkey.ToNewCString();
    char*   valnameCString  = valname.ToNewCString();


#ifdef WIN32
    root   = (HKEY) rootkey;
    result = RegOpenKeyEx( root, subkeyCString, 0, KEY_ALL_ACCESS, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegQueryValueEx( newkey, valnameCString, nsnull, &type, valbuf, &length );

        RegCloseKey( newkey );
    }

    if(ERROR_SUCCESS == result && type == REG_SZ)
    {
        value = new nsString((char*)valbuf);
    }
#endif

    delete [] subkeyCString;
    delete [] valnameCString;

    return value;
}

  
PRInt32
nsWinReg::nativeSetValue(nsString subkey, nsString valname, nsWinRegValue* value)
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
nsWinReg::nativeGetValue(nsString subkey, nsString valname)
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

