/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsWinReg.h"
#include "nsWinRegItem.h"
#include "nsNativeCharsetUtils.h"
#include <windows.h>


/* Public Methods */

MOZ_DECL_CTOR_COUNTER(nsWinReg)

nsWinReg::nsWinReg(nsInstall* suObj)
{
    MOZ_COUNT_CTOR(nsWinReg);

    mInstallObject      = suObj;
	mRootKey = (PRInt32)HKEY_CLASSES_ROOT;
}

nsWinReg::~nsWinReg()
{
    MOZ_COUNT_DTOR(nsWinReg);
}

PRInt32
nsWinReg::SetRootKey(PRInt32 key)
{
	mRootKey = key;
    return NS_OK;
}
  
PRInt32
nsWinReg::CreateKey(const nsAString& subkey, const nsAString& classname, PRInt32* aReturn)
{
	nsWinRegItem* wi = new nsWinRegItem(this, mRootKey, NS_WIN_REG_CREATE, subkey, classname, NS_LITERAL_STRING("null"), aReturn);

  if(wi == nsnull)
  {
    *aReturn = nsInstall::OUT_OF_MEMORY;
    return NS_OK;
  }

  if(*aReturn != nsInstall::SUCCESS)
  {
    if(wi)
    {
      delete wi;
      return NS_OK;
    }
  }

  if(mInstallObject)
  	*aReturn = mInstallObject->ScheduleForInstall(wi);
	
  return 0;
}
  
PRInt32
nsWinReg::DeleteKey(const nsAString& subkey, PRInt32* aReturn)
{
	nsWinRegItem* wi = new nsWinRegItem(this, mRootKey, NS_WIN_REG_DELETE, subkey, NS_LITERAL_STRING("null"), NS_LITERAL_STRING("null"), aReturn);

  if(wi == nsnull)
  {
    *aReturn = nsInstall::OUT_OF_MEMORY;
    return NS_OK;
  }

  if(*aReturn != nsInstall::SUCCESS)
  {
    if(wi)
    {
      delete wi;
      return NS_OK;
    }
  }

  if(mInstallObject)
    *aReturn = mInstallObject->ScheduleForInstall(wi);

  return 0;
}

PRInt32
nsWinReg::DeleteValue(const nsAString& subkey, const nsAString& valname, PRInt32* aReturn)
{
	nsWinRegItem* wi = new nsWinRegItem(this, mRootKey, NS_WIN_REG_DELETE_VAL, subkey, valname, NS_LITERAL_STRING("null"), aReturn);

  if(wi == nsnull)
  {
    *aReturn = nsInstall::OUT_OF_MEMORY;
    return NS_OK;
  }

  if(*aReturn != nsInstall::SUCCESS)
  {
    if(wi)
    {
      delete wi;
      return NS_OK;
    }
  }

  if(mInstallObject)
    *aReturn = mInstallObject->ScheduleForInstall(wi);

  return 0;
}

PRInt32
nsWinReg::SetValueString(const nsAString& subkey, const nsAString& valname, const nsAString& value, PRInt32* aReturn)
{
	nsWinRegItem* wi = new nsWinRegItem(this, mRootKey, NS_WIN_REG_SET_VAL_STRING, subkey, valname, value, aReturn);

  if(wi == nsnull)
  {
    *aReturn = nsInstall::OUT_OF_MEMORY;
    return NS_OK;
  }

  if(*aReturn != nsInstall::SUCCESS)
  {
    if(wi)
    {
      delete wi;
      return NS_OK;
    }
  }

  if(mInstallObject)
    *aReturn = mInstallObject->ScheduleForInstall(wi);
	
  return 0;
}

PRInt32
nsWinReg::GetValueString(const nsString& subkey, const nsString& valname, nsString* aReturn)
{
   return NativeGetValueString(subkey, valname, aReturn);
}
 
PRInt32
nsWinReg::EnumValueNames(const nsString& aSubkey, PRInt32 aIndex, nsString &aReturn)
{
    char           namebuf[MAX_BUF];
    HKEY           root;
    HKEY           newkey;
    LONG           result;
    DWORD          namesize         = sizeof(namebuf);
    PRInt32        rv = nsInstall::UNEXPECTED_ERROR;

    nsCAutoString subkey;
    if ( NS_FAILED ( NS_CopyUnicodeToNative(aSubkey, subkey) ) )
        return nsInstall::UNEXPECTED_ERROR;

    root   = (HKEY) mRootKey;
    result = RegOpenKeyEx( root, subkey.get(), 0, KEY_READ, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegEnumValue( newkey, aIndex, namebuf, &namesize, nsnull, 0, 0, 0 );
        RegCloseKey( newkey );

        if ( ERROR_SUCCESS == result ) {
            nsCAutoString cstrName(namebuf);
            nsAutoString name;
            if(NS_SUCCEEDED(NS_CopyNativeToUnicode(cstrName, name)))
            {
                aReturn.Assign(name);
                rv = nsInstall::SUCCESS;
            }
        } 
    }

    return rv;
}

PRInt32
nsWinReg::EnumKeys(const nsString& aSubkey, PRInt32 aIndex, nsString &aReturn)
{
    char            keybuf[MAX_BUF];
    HKEY            root;
    HKEY            newkey;
    LONG            result;
    DWORD           type            = REG_SZ;
    PRInt32         rv = nsInstall::UNEXPECTED_ERROR;

    nsCAutoString subkey;
    if ( NS_FAILED(NS_CopyUnicodeToNative(aSubkey, subkey) ) )
        return nsInstall::UNEXPECTED_ERROR;

    root   = (HKEY) mRootKey;
    result = RegOpenKeyEx( root, subkey.get(), 0, KEY_READ, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegEnumKey( newkey, aIndex, keybuf, sizeof keybuf );
        RegCloseKey( newkey );
        
        if ( ERROR_SUCCESS == result ) {
            nsCAutoString cstrKey(keybuf);
            nsAutoString key;
            if (NS_SUCCEEDED(NS_CopyNativeToUnicode(cstrKey, key) ) )
            {
                aReturn.Assign(key);
                rv = nsInstall::SUCCESS;
            }
        }
    }

    return rv;
}

PRInt32
nsWinReg::SetValueNumber(const nsString& subkey, const nsString& valname, PRInt32 value, PRInt32* aReturn)
{
	nsWinRegItem* wi = new nsWinRegItem(this, mRootKey, NS_WIN_REG_SET_VAL_NUMBER, subkey, valname, value, aReturn);

  if(wi == nsnull)
  {
    *aReturn = nsInstall::OUT_OF_MEMORY;
    return NS_OK;
  }

  if(*aReturn != nsInstall::SUCCESS)
  {
    if(wi)
    {
      delete wi;
      return NS_OK;
    }
  }

  if(mInstallObject)
    *aReturn = mInstallObject->ScheduleForInstall(wi);

  return 0;
}

PRInt32
nsWinReg::GetValueNumber(const nsString& subkey, const nsString& valname, PRInt32* aReturn)
{
    return NativeGetValueNumber(subkey, valname, aReturn);
}
 
PRInt32
nsWinReg::SetValue(const nsString& subkey, const nsString& valname, nsWinRegValue* value, PRInt32* aReturn)
{
  // fix: need to figure out what to do with nsWinRegValue class.
  //
	// nsWinRegItem* wi = new nsWinRegItem(this, mRootKey, NS_WIN_REG_SET_VAL, subkey, valname, (nsWinRegValue*)value, aReturn);
  //
	// if(wi == nsnull)
  // {
  //   return NS_OK;
  // }
	// mInstallObject->ScheduleForInstall(wi);
	return 0;
}
  
PRInt32
nsWinReg::GetValue(const nsString& subkey, const nsString& valname, nsWinRegValue** aReturn)
{
  // fix:
  return NS_OK;
}
  
nsInstall* nsWinReg::InstallObject()
{
	return mInstallObject;
}
  
PRInt32
nsWinReg::KeyExists(const nsString& subkey,
                    PRBool* aReturn)
{
  *aReturn = NativeKeyExists(subkey);
  return NS_OK;
}

PRInt32
nsWinReg::ValueExists(const nsString& subkey,
                      const nsString& valname,
                      PRBool* aReturn)
{
  *aReturn = NativeValueExists(subkey, valname);
  return NS_OK;
}
 
PRInt32
nsWinReg::IsKeyWritable(const nsString& subkey,
                        PRBool* aReturn)
{
  *aReturn = NativeIsKeyWritable(subkey);
  return NS_OK;
}

PRInt32
nsWinReg::PrepareCreateKey(PRInt32 root,
                           const nsString& subkey,
                           PRInt32* aReturn)
{
  SetRootKey(root);
  if(NativeIsKeyWritable(subkey))
    *aReturn = nsInstall::SUCCESS;
  else
    *aReturn = nsInstall::KEY_ACCESS_DENIED;

  return NS_OK;
}

PRInt32
nsWinReg::PrepareDeleteKey(PRInt32 root,
                           const nsString& subkey,
                           PRInt32* aReturn)
{
  SetRootKey(root);
  if(NativeKeyExists(subkey))
  {
    if(NativeIsKeyWritable(subkey))
      *aReturn = nsInstall::SUCCESS;
    else
      *aReturn = nsInstall::KEY_ACCESS_DENIED;
  }
  else
    *aReturn = nsInstall::KEY_DOES_NOT_EXIST;

  return NS_OK;
}
  
PRInt32
nsWinReg::PrepareDeleteValue(PRInt32 root,
                             const nsString& subkey,
                             const nsString& valname,
                             PRInt32* aReturn)
{
  SetRootKey(root);
  if(NativeValueExists(subkey, valname))
  {
    if(NativeIsKeyWritable(subkey))
      *aReturn = nsInstall::SUCCESS;
    else
      *aReturn = nsInstall::KEY_ACCESS_DENIED;
  }
  else
    *aReturn = nsInstall::VALUE_DOES_NOT_EXIST;

  return NS_OK;
}

PRInt32
nsWinReg::PrepareSetValueString(PRInt32 root,
                                const nsString& subkey,
                                PRInt32* aReturn)
{
  SetRootKey(root);
  if(NativeIsKeyWritable(subkey))
    *aReturn = nsInstall::SUCCESS;
  else
    *aReturn = nsInstall::KEY_ACCESS_DENIED;

  return NS_OK;
}
 
PRInt32
nsWinReg::PrepareSetValueNumber(PRInt32 root,
                                const nsString& subkey,
                                PRInt32* aReturn)
{
  SetRootKey(root);
  if(NativeIsKeyWritable(subkey))
    *aReturn = nsInstall::SUCCESS;
  else
    *aReturn = nsInstall::KEY_ACCESS_DENIED;

  return NS_OK;
}
 
PRInt32
nsWinReg::PrepareSetValue(PRInt32 root,
                          const nsString& subkey,
                          PRInt32* aReturn)
{
  SetRootKey(root);
  if(NativeIsKeyWritable(subkey))
    *aReturn = nsInstall::SUCCESS;
  else
    *aReturn = nsInstall::KEY_ACCESS_DENIED;

  return NS_OK;
}

PRInt32
nsWinReg::FinalCreateKey(PRInt32 root, const nsString& subkey, const nsString& classname, PRInt32* aReturn)
{
	SetRootKey(root);
	*aReturn = NativeCreateKey(subkey, classname);
  return NS_OK;
}
  
PRInt32
nsWinReg::FinalDeleteKey(PRInt32 root, const nsString& subkey, PRInt32* aReturn)
{
	SetRootKey(root);
  if ( PR_TRUE == NativeKeyExists(subkey) )
    *aReturn = NativeDeleteKey(subkey);
  else {
    NS_WARNING("Trying to delete a key that doesn't exist anymore. Possible causes include calling deleteKey for the same key multiple times, or key+subkey are not unique");
    *aReturn = ERROR_SUCCESS;
  }
  return NS_OK;
}
  
PRInt32
nsWinReg::FinalDeleteValue(PRInt32 root, const nsString& subkey, const nsString& valname, PRInt32* aReturn)
{
	SetRootKey(root);
	*aReturn = NativeDeleteValue(subkey, valname);
  return NS_OK;
}

PRInt32
nsWinReg::FinalSetValueString(PRInt32 root, const nsString& subkey, const nsString& valname, const nsString& value, PRInt32* aReturn)
{
	SetRootKey(root);
	*aReturn = NativeSetValueString(subkey, valname, value);
  return NS_OK;
}
 
PRInt32
nsWinReg::FinalSetValueNumber(PRInt32 root, const nsString& subkey, const nsString& valname, PRInt32 value, PRInt32* aReturn)
{
	SetRootKey(root);
	*aReturn = NativeSetValueNumber(subkey, valname, value);
  return NS_OK;
}
 
PRInt32
nsWinReg::FinalSetValue(PRInt32 root, const nsString& subkey, const nsString& valname, nsWinRegValue* value, PRInt32* aReturn)
{
	SetRootKey(root);
	*aReturn = NativeSetValue(subkey, valname, value);
  return NS_OK;
}


/* Private Methods */

PRBool
nsWinReg::NativeKeyExists(const nsString& aSubkey)
{
    HKEY    root, newkey;
    LONG    result;
    PRBool  keyExists     = PR_FALSE;

#ifdef WIN32
    nsCAutoString subkey;

    if ( NS_FAILED( NS_CopyUnicodeToNative(aSubkey, subkey) ) )
        return PR_FALSE;

    root   = (HKEY)mRootKey;
    result = RegOpenKeyEx(root, subkey.get(), 0, KEY_READ, &newkey);
    switch(result)
    {
        case ERROR_SUCCESS:
            RegCloseKey(newkey);
            keyExists = PR_TRUE;
            break;

        case ERROR_FILE_NOT_FOUND:
        case ERROR_ACCESS_DENIED:
        default:
            break;
    }
#endif

    return keyExists;
}

PRBool
nsWinReg::NativeValueExists(const nsString& aSubkey, const nsString& aValname)
{
    HKEY          root;
    HKEY          newkey;
    LONG          result;
    PRBool        valueExists = PR_FALSE;
    DWORD         length      = _MAXKEYVALUE_;
    unsigned char valbuf[_MAXKEYVALUE_];
    
#ifdef WIN32
    nsCAutoString subkey;
    nsCAutoString valname;

    if ( NS_FAILED( NS_CopyUnicodeToNative(aSubkey, subkey) ) ||
         NS_FAILED( NS_CopyUnicodeToNative(aValname, valname) ) )
        return PR_FALSE;

    root   = (HKEY) mRootKey;
    result = RegOpenKeyEx(root, subkey.get(), 0, KEY_READ, &newkey);
    switch(result)
    {
        case ERROR_SUCCESS:
            result = RegQueryValueEx(newkey,
                                     valname.get(),
                                     0,
                                     NULL,
                                     valbuf,
                                     &length);
            switch(result)
            {
                case ERROR_SUCCESS:
                    valueExists = PR_TRUE;
                    break;

                case ERROR_FILE_NOT_FOUND:
                case ERROR_ACCESS_DENIED:
                default:
                    break;
            }
            RegCloseKey(newkey);
            break;

        case ERROR_FILE_NOT_FOUND:
        case ERROR_ACCESS_DENIED:
        default:
            break;
    }
#endif

    return valueExists;
}
 
PRBool
nsWinReg::NativeIsKeyWritable(const nsString& aSubkey)
{
    HKEY     root;
    HKEY     newkey;
    LONG     result;
    nsString subkeyParent = aSubkey;
    PRInt32  index;
    PRInt32  rv = PR_FALSE;

#ifdef WIN32
    /* In order to check to see if a key is writable (user has write access
     * to the key), we need to open the key with KEY_WRITE access.  In order
     * to open a key, the key must first exist.  If the key passed does not
     * exist, we need to search for one of its parent key that _does_ exist.
     * This do{} loop will search for an existing parent key. */
    do
    {
        rv = NativeKeyExists(subkeyParent);
        if(!rv)
        {
            index = subkeyParent.RFindChar('\\', -1, -1);
            if(index > 0)
                /* delete everything from the '\\' found to the end of the string */
                subkeyParent.SetLength(index);
            else
                /* key does not exist and no parent key found */
                break;
        }
    }while(!rv);

    if(rv)
    {
        nsCAutoString subkey;

        if ( NS_FAILED( NS_CopyUnicodeToNative(subkeyParent, subkey) ) )
            result = nsInstall::UNEXPECTED_ERROR;
        else
        {
            rv     = PR_FALSE;
            root   = (HKEY)mRootKey;
            result = RegOpenKeyEx(root, subkey.get(), 0, KEY_WRITE, &newkey);
            switch(result)
            {
                case ERROR_SUCCESS:
                    RegCloseKey(newkey);
                    rv = PR_TRUE;
                    break;

                case ERROR_FILE_NOT_FOUND:
                case ERROR_ACCESS_DENIED:
                default:
                    break;
            }
        }
    }
#endif
    return rv;
}

PRInt32
nsWinReg::NativeCreateKey(const nsString& aSubkey, const nsString& aClassname)
{
    HKEY    root, newkey;
    LONG    result;
    ULONG   disposition;

#ifdef WIN32
    nsCAutoString subkey;
    nsCAutoString classname;

    if (NS_FAILED(NS_CopyUnicodeToNative(aSubkey, subkey) ) ||
        NS_FAILED(NS_CopyUnicodeToNative(aClassname, classname) ) )
        return nsInstall::UNEXPECTED_ERROR;

    root   = (HKEY)mRootKey;
    result = RegCreateKeyEx(root, subkey.get(), 0, NS_CONST_CAST(char*, classname.get()),
                REG_OPTION_NON_VOLATILE, KEY_WRITE, nsnull, &newkey, &disposition);

    if(ERROR_SUCCESS == result)
    {
        RegCloseKey( newkey );
    }
#endif

    return result;
}

PRInt32
nsWinReg::NativeDeleteKey(const nsString& aSubkey)
{
    HKEY  root;
    LONG  result;

#ifdef WIN32
    nsCAutoString subkey;

    if (NS_FAILED(NS_CopyUnicodeToNative(aSubkey, subkey) ) )
      return nsInstall::UNEXPECTED_ERROR;

    root   = (HKEY) mRootKey;
    result = RegDeleteKey( root, subkey.get() );
#endif

    return result;
}
  
PRInt32
nsWinReg::NativeDeleteValue(const nsString& aSubkey, const nsString& aValname)
{
#if defined (WIN32) || defined (XP_OS2)
    HKEY    root, newkey;
    LONG    result;
    nsCAutoString subkey;
    nsCAutoString valname;

    if ( NS_FAILED( NS_CopyUnicodeToNative(aSubkey, subkey) ) ||
         NS_FAILED( NS_CopyUnicodeToNative(aValname, valname) ) )
        return nsInstall::UNEXPECTED_ERROR;
    root   = (HKEY) mRootKey;
    result = RegOpenKeyEx( root, subkey.get(), 0, KEY_WRITE, &newkey);

    if ( ERROR_SUCCESS == result )
    {
        result = RegDeleteValue( newkey, valname.get() );
        RegCloseKey( newkey );
    }

    return result;
#else
    return ERROR_INVALID_PARAMETER;
#endif
}

PRInt32
nsWinReg::NativeSetValueString(const nsString& aSubkey, const nsString& aValname, const nsString& aValue)
{
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length;

    nsCAutoString subkey;
    nsCAutoString valname;
    nsCAutoString value;

    if ( NS_FAILED( NS_CopyUnicodeToNative(aSubkey, subkey) ) ||
         NS_FAILED( NS_CopyUnicodeToNative(aValname, valname) ) ||
         NS_FAILED( NS_CopyUnicodeToNative(aValue, value) ) )
        return nsInstall::UNEXPECTED_ERROR;

    length = value.Length();

    root   = (HKEY) mRootKey;
    result = RegOpenKeyEx( root, subkey.get(), 0, KEY_WRITE, &newkey);

    if(ERROR_SUCCESS == result)
    {
        result = RegSetValueEx( newkey, valname.get(), 0, REG_SZ, (unsigned char*)value.get(), length );
        RegCloseKey( newkey );
    }

    return result;
}
 
#define STRBUFLEN 255
 
PRInt32
nsWinReg::NativeGetValueString(const nsString& aSubkey, const nsString& aValname, nsString* aReturn)
{
    unsigned char     valbuf[_MAXKEYVALUE_];
    HKEY              root;
    HKEY              newkey;
    LONG              result;
    DWORD             type            = REG_SZ;
    DWORD             length          = sizeof(valbuf);
    nsCAutoString     subkey;
    nsCAutoString     valname;

    if ( NS_FAILED( NS_CopyUnicodeToNative(aSubkey, subkey) ) ||
         NS_FAILED( NS_CopyUnicodeToNative(aValname, valname) ) )
        return nsInstall::UNEXPECTED_ERROR;

    root   = (HKEY) mRootKey;
    result = RegOpenKeyEx( root, subkey.get(), 0, KEY_READ, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegQueryValueEx( newkey, valname.get(), nsnull, &type, valbuf, &length );

        RegCloseKey( newkey );

        if( result == ERROR_SUCCESS && type == REG_SZ)
        {
            // this cast is ok until mozilla is compiled in UNICODE, then this
            // will break.
            nsAutoString value;
            if (NS_SUCCEEDED( NS_CopyNativeToUnicode(nsDependentCString(NS_REINTERPRET_CAST(const char *, valbuf)), value) ) )
              aReturn->Assign(value);
            else
              result = nsInstall::UNEXPECTED_ERROR;
        }
    }

    if(ERROR_ACCESS_DENIED == result)
        result = nsInstall::ACCESS_DENIED;

    return result;
}

PRInt32
nsWinReg::NativeSetValueNumber(const nsString& aSubkey, const nsString& aValname, PRInt32 aValue)
{
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    nsCAutoString subkey;
    nsCAutoString valname;

    if ( NS_FAILED( NS_CopyUnicodeToNative(aSubkey, subkey) ) ||
         NS_FAILED( NS_CopyUnicodeToNative(aValname, valname) ) )
        return nsInstall::UNEXPECTED_ERROR;

    root   = (HKEY) mRootKey;
    result = RegOpenKeyEx( root, subkey.get(), 0, KEY_WRITE, &newkey);

    if(ERROR_SUCCESS == result)
    {
        result = RegSetValueEx( newkey, valname.get(), 0, REG_DWORD, (LPBYTE)&aValue, sizeof(PRInt32));
        RegCloseKey( newkey );
    }

    return result;
}
 
PRInt32
nsWinReg::NativeGetValueNumber(const nsString& aSubkey, const nsString& aValname, PRInt32* aReturn)

{
    PRInt32 valbuf;
    PRInt32 valbuflen;
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   type            = REG_DWORD;
    DWORD   length          = _MAXKEYVALUE_;
    nsCAutoString subkey;
    nsCAutoString valname;

    if ( NS_FAILED( NS_CopyUnicodeToNative(aSubkey, subkey) ) ||
         NS_FAILED( NS_CopyUnicodeToNative(aValname, valname) ) )
        return nsInstall::UNEXPECTED_ERROR;

    valbuflen = sizeof(PRInt32);
    root      = (HKEY) mRootKey;
    result    = RegOpenKeyEx( root, subkey.get(), 0, KEY_READ, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegQueryValueEx( newkey, valname.get(), nsnull, &type, (LPBYTE)&valbuf, (LPDWORD)&valbuflen);

        RegCloseKey( newkey );

        if(type == REG_DWORD)
            *aReturn = valbuf;
    }

    if(ERROR_ACCESS_DENIED == result)
        return nsInstall::ACCESS_DENIED;
    else if (result != ERROR_SUCCESS)
        return nsInstall::UNEXPECTED_ERROR;

    return nsInstall::SUCCESS;
}

PRInt32
nsWinReg::NativeSetValue(const nsString& aSubkey, const nsString& aValname, nsWinRegValue* aValue)
{
#if defined (WIN32) || defined (XP_OS2)
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length;
    DWORD   type;
    unsigned char* data;
    nsCAutoString subkey;
    nsCAutoString valname;

    if ( NS_FAILED( NS_CopyUnicodeToNative(aSubkey, subkey) ) ||
         NS_FAILED( NS_CopyUnicodeToNative(aValname, valname) ) )
        return nsnull;

    root   = (HKEY) mRootKey;
    result = RegOpenKeyEx( root, subkey.get(), 0, KEY_WRITE, &newkey );

    if(ERROR_SUCCESS == result)
    {
        type = (DWORD)aValue->type;
        data = (unsigned char*)aValue->data;
        length = (DWORD)aValue->data_length;

        result = RegSetValueEx( newkey, valname.get(), 0, type, data, length);
        RegCloseKey( newkey );
    }

    return result;
#else
    return ERROR_INVALID_PARAMETER;
#endif
}
  
nsWinRegValue*
nsWinReg::NativeGetValue(const nsString& aSubkey, const nsString& aValname)
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
    nsCAutoString subkey;
    nsCAutoString valname;

    if ( NS_FAILED( NS_CopyUnicodeToNative(aSubkey, subkey) ) ||
         NS_FAILED( NS_CopyUnicodeToNative(aValname, valname) ) )
        return nsnull;

    root   = (HKEY) mRootKey;
    result = RegOpenKeyEx( root, subkey.get(), 0, KEY_READ, &newkey );

    if(ERROR_SUCCESS == result)
    {
        result = RegQueryValueEx( newkey, valname.get(), nsnull, &type, valbuf, &length );

        if ( ERROR_SUCCESS == result ) {
            data = new nsString;
            data->AssignWithConversion((char *)valbuf);
			      length = data->Length();
            value = new nsWinRegValue(type, (void*)data, length);
        }

        RegCloseKey( newkey );
    }

    return value;
#else
    return nsnull;
#endif
}

