/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifdef XP_PC

#include <windows.h>

#define  IMPLEMENT_netscape_softupdate_WinProfile
#define  IMPLEMENT_netscape_softupdate_WinReg
#define  IMPLEMENT_netscape_softupdate_WinRegValue

#include "_jri/netscape_softupdate_WinProfile.c"
#include "_jri/netscape_softupdate_WinReg.c"
#include "_jri/netscape_softupdate_WinRegValue.c"

#ifdef XP_OS2
#include "registry.h" //from cmd/os2fe/nfc/include/registry.h
#define REG_SZ  0
#endif

#ifndef WIN32
#include "shellapi.h"
#endif

#define STRBUFLEN 1024

/* ------------------------------------------------------------------
 * WinProfile native methods
 * ------------------------------------------------------------------
 */

void SUWinSpecificInit( JRIEnv *env )
{
   use_netscape_softupdate_WinProfile((JRIEnv*)env);
   use_netscape_softupdate_WinReg((JRIEnv*)env);
   use_netscape_softupdate_WinRegValue((JRIEnv*)env);
}


/*** public native nativeWriteString
    (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_WinProfile_nativeWriteString(
    JRIEnv* env,
    struct netscape_softupdate_WinProfile* self,
    struct java_lang_String *section,
    struct java_lang_String *key,
    struct java_lang_String *value)
{
    int     success = 0;
    char *  pApp;
    char *  pKey;
    char *  pValue;
    char *  pFile;
    struct java_lang_String *file;

    file = get_netscape_softupdate_WinProfile_filename( env, self );
    pFile = (char*)JRI_GetStringPlatformChars( env, file, "", 0 );

    pApp   = (char*)JRI_GetStringPlatformChars( env, section, "", 0 );
    pKey   = (char*)JRI_GetStringPlatformChars( env, key, "", 0 );
    pValue = (char*)JRI_GetStringPlatformChars( env, value, "", 0 );

    /* make sure conversions worked */
    if ( pApp != NULL && pKey != NULL && pFile != NULL ) {
        /* it's OK for pValue to be null only if original param was also NULL */
        if ( pValue != NULL || value == NULL ) {
            success = WritePrivateProfileString( pApp, pKey, pValue, pFile );
        }
    }

    return success;
}


/*** public native nativeGetString
    (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String; ***/
JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_WinProfile_nativeGetString(
    JRIEnv* env,
    struct netscape_softupdate_WinProfile* self,
    struct java_lang_String *section,
    struct java_lang_String *key)
{
    int     numChars;
    char *  pApp;
    char *  pKey;
    char *  pFile;
    char    valbuf[STRBUFLEN];
    struct java_lang_String *file;
    struct java_lang_String *value = NULL;

    file = get_netscape_softupdate_WinProfile_filename( env, self );
    pFile = (char*)JRI_GetStringPlatformChars( env, file, "", 0 );

    pApp   = (char*)JRI_GetStringPlatformChars( env, section, "", 0 );
    pKey   = (char*)JRI_GetStringPlatformChars( env, key, "", 0 );

    /* make sure conversions worked */
    if ( pApp != NULL && pKey != NULL && pFile != NULL ) {
        numChars = GetPrivateProfileString( pApp, pKey, "",
                    valbuf, STRBUFLEN, pFile );

        /* if the value fit in the buffer */
        if ( numChars < STRBUFLEN ) {
            value = JRI_NewStringPlatform( env, valbuf, numChars, "", 0 );
        }
    }

    return value;
}



/* ------------------------------------------------------------------
 * WinReg native methods
 * ------------------------------------------------------------------
 */

/*** public native createKey (Ljava/lang/String;Ljava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_WinReg_nativeCreateKey(
    JRIEnv* env,
    struct netscape_softupdate_WinReg* self,
    struct java_lang_String *subkey,
    struct java_lang_String *classname)
{
    char *  pSubkey;
    char *  pClass;
    HKEY    root, newkey;
    LONG    result;
    LONG    disposition;

    pSubkey = (char*)JRI_GetStringPlatformChars( env, subkey, "", 0 );
    pClass  = (char*)JRI_GetStringPlatformChars( env, classname, "", 0 );

#ifdef WIN32
    root  = (HKEY)get_netscape_softupdate_WinReg_rootkey( env, self );

    result = RegCreateKeyEx( root, pSubkey, 0, pClass, REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS, NULL, &newkey, &disposition );
#else
    result = RegCreateKey( HKEY_CLASSES_ROOT, pSubkey, &newkey );
#endif

    if (ERROR_SUCCESS == result ) {
        RegCloseKey( newkey );
    }

    return result;
}



/*** public native deleteKey (Ljava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_WinReg_nativeDeleteKey(
    JRIEnv* env,
    struct netscape_softupdate_WinReg* self,
    struct java_lang_String *subkey)
{
    char *  pSubkey;
    HKEY    root;

    pSubkey = (char*)JRI_GetStringPlatformChars( env, subkey, "", 0 );

#ifdef WIN32
    root    = (HKEY)get_netscape_softupdate_WinReg_rootkey( env, self );
#else
    root    = HKEY_CLASSES_ROOT;
#endif

    return RegDeleteKey( root, pSubkey );
}



/*** public native deleteValue (Ljava/lang/String;Ljava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_WinReg_nativeDeleteValue(
    JRIEnv* env,
    struct netscape_softupdate_WinReg* self,
    struct java_lang_String *subkey,
    struct java_lang_String *valname)
{
#if defined (WIN32) || defined (XP_OS2)
    char *  pSubkey;
    char *  pValue;
    HKEY    root, newkey;
    LONG    result;

    pSubkey = (char*)JRI_GetStringPlatformChars( env, subkey, "", 0 );
    pValue  = (char*)JRI_GetStringPlatformChars( env, valname, "", 0 );

    root    = (HKEY)get_netscape_softupdate_WinReg_rootkey( env, self );

    result = RegOpenKeyEx( root, pSubkey, 0, KEY_WRITE, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegDeleteValue( newkey, pValue );

        RegCloseKey( newkey );
    }

    return result;
#else
    return ERROR_INVALID_PARAMETER;
#endif
}



/*** public native setValueString (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_WinReg_nativeSetValueString(
    JRIEnv* env,
    struct netscape_softupdate_WinReg* self,
    struct java_lang_String *subkey,
    struct java_lang_String *valname,
    struct java_lang_String *value)
{
    char *  pSubkey;
    char *  pName;
    char *  pValue;
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length;

    pSubkey = (char*)JRI_GetStringPlatformChars( env, subkey, "", 0 );

    pValue  = (char*)JRI_GetStringPlatformChars( env, value, "", 0 );
    length = (pValue == NULL) ? 0 : strlen(pValue);
    
#ifdef WIN32
    pName   = (char*)JRI_GetStringPlatformChars( env, valname, "", 0 );

    root    = (HKEY)get_netscape_softupdate_WinReg_rootkey( env, self );

    result = RegOpenKeyEx( root, pSubkey, 0, KEY_ALL_ACCESS, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegSetValueEx( newkey, pName, 0, REG_SZ, pValue, length );

        RegCloseKey( newkey );
    }
#else
    result = RegSetValue( HKEY_CLASSES_ROOT, pSubkey, REG_SZ, pValue, length );
#endif

    return result;
}



/*** public native getValueString (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String; ***/
JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_WinReg_nativeGetValueString(
    JRIEnv* env,
    struct netscape_softupdate_WinReg* self,
    struct java_lang_String *subkey,
    struct java_lang_String *valname)
{
    char *  pSubkey;
    char *  pName;
    char    valbuf[STRBUFLEN];
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   type = REG_SZ;
    DWORD   length = STRBUFLEN;
    struct java_lang_String *value = NULL;

    pSubkey = (char*)JRI_GetStringPlatformChars( env, subkey, "", 0 );

#ifdef WIN32
    pName   = (char*)JRI_GetStringPlatformChars( env, valname, "", 0 );

    root    = (HKEY)get_netscape_softupdate_WinReg_rootkey( env, self );

    result = RegOpenKeyEx( root, pSubkey, 0, KEY_ALL_ACCESS, &newkey );

    if ( ERROR_SUCCESS == result ) {
        result = RegQueryValueEx( newkey, pName, NULL, &type, valbuf, &length );

        RegCloseKey( newkey );
    }
#else
    result = RegQueryValue( HKEY_CLASSES_ROOT, pSubkey, valbuf, &length );
#endif

    if ( ERROR_SUCCESS == result && type == REG_SZ ) {
        value = JRI_NewStringPlatform( env, valbuf, length, "", 0 );
    }

    return value;
}



/*** public native setValue (Ljava/lang/String;Ljava/lang/String;Lnetscape/softupdate/WinRegValue;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_WinReg_nativeSetValue(
    JRIEnv* env,
    struct netscape_softupdate_WinReg* self,
    struct java_lang_String *subkey,
    struct java_lang_String *valname,
    struct netscape_softupdate_WinRegValue *value)
{
#if defined (WIN32) || defined (XP_OS2)
    char *  pSubkey;
    char *  pName;
    char *  pValue;
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length;
    DWORD   type;
    jref    data;

    pSubkey = (char*)JRI_GetStringPlatformChars( env, subkey, "", 0 );

    root    = (HKEY)get_netscape_softupdate_WinReg_rootkey( env, self );

    result = RegOpenKeyEx( root, pSubkey, 0, KEY_ALL_ACCESS, &newkey );

    if ( ERROR_SUCCESS == result ) {
        pName   = (char*)JRI_GetStringPlatformChars( env, valname, "", 0 );

        type = get_netscape_softupdate_WinRegValue_type( env, value );
        data = get_netscape_softupdate_WinRegValue_data( env, value );

        length = JRI_GetByteArrayLength( env, data );
        pValue = JRI_GetByteArrayElements( env, data );

        result = RegSetValueEx( newkey, pName, 0, type, pValue, length );

        RegCloseKey( newkey );
    }

    return result;
#else
    return ERROR_INVALID_PARAMETER;
#endif
}



/*** public native getValue (Ljava/lang/String;Ljava/lang/String;)Lnetscape/softupdate/WinRegValue; ***/
JRI_PUBLIC_API(struct netscape_softupdate_WinRegValue *)
native_netscape_softupdate_WinReg_nativeGetValue(
    JRIEnv* env,
    struct netscape_softupdate_WinReg* self,
    struct java_lang_String *subkey,
    struct java_lang_String *valname)
{
#if defined (WIN32) || defined (XP_OS2)
    char *  pSubkey;
    char *  pName;
    char    valbuf[STRBUFLEN];
    HKEY    root;
    HKEY    newkey;
    LONG    result;
    DWORD   length=STRBUFLEN;
    DWORD   type;
    jref    data;
    struct netscape_softupdate_WinRegValue * value = NULL;

    pSubkey = (char*)JRI_GetStringPlatformChars( env, subkey, "", 0 );

    root    = (HKEY)get_netscape_softupdate_WinReg_rootkey( env, self );

    result = RegOpenKeyEx( root, pSubkey, 0, KEY_ALL_ACCESS, &newkey );

    if ( ERROR_SUCCESS == result ) {
        pName   = (char*)JRI_GetStringPlatformChars( env, valname, "", 0 );

        result = RegQueryValueEx( newkey, pName, NULL, &type, valbuf, &length );

        if ( ERROR_SUCCESS == result ) {
            data = JRI_NewByteArray( env, length, valbuf );

            value = netscape_softupdate_WinRegValue_new(
                env,
                class_netscape_softupdate_WinRegValue(env),
                type,
                data );
        }

        RegCloseKey( newkey );
    }

    return value;
#else
    return NULL;
#endif
}


#endif /* XP_PC */
