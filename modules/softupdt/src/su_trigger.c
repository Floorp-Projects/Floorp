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
/* su_trigger.c
 * netscape.softupdate.Trigger.java
 * native implementation
 */

#define IMPLEMENT_netscape_softupdate_Trigger

#ifndef XP_MAC
#include "_jri/netscape_softupdate_Trigger.c"
#else
#include "netscape_softupdate_Trigger.c"
#endif

#define IMPLEMENT_netscape_softupdate_SoftUpdateException

#ifndef XP_MAC
#include "_jri/netscape_softupdate_SoftUpdateException.c"
#else
#include "n_s_SoftUpdateException.c"
#endif

#define IMPLEMENT_netscape_softupdate_SoftwareUpdate

#ifndef XP_MAC
#include "_jri/netscape_softupdate_SoftwareUpdate.c"
#else
#include "n_softupdate_SoftwareUpdate.c"
#endif

#define IMPLEMENT_netscape_javascript_JSObject	/* Only needed because we are calling privates */
#ifndef XP_MAC
#include "netscape_javascript_JSObject.h"
#else
#include "netscape_javascript_JSObject.h"
#endif

#ifndef XP_MAC
#include "_jri/netscape_softupdate_Strings.c"
#else
#include "netscape_softupdate_Strings.c"
#endif

#include "softupdt.h"
#include "zig.h"
#include "prefapi.h"
#include "su_aplsn.h"
#include "proto.h"
#include "jsapi.h"
#include "xp_error.h"
#include "jsjava.h"

extern void FolderSpecInitialize(JRIEnv * env);
extern void InstallFileInitialize(JRIEnv * env);
extern void SUWinSpecificInit( JRIEnv *env );
extern int  DiskSpaceAvailable( char *path, int nBytes );

#ifdef XP_MAC
#pragma export on
#endif

void SU_Initialize(JRIEnv * env)
{
	use_netscape_softupdate_Trigger( env );
	use_netscape_softupdate_SoftwareUpdate( env );
	use_netscape_softupdate_Strings( env );
	use_netscape_softupdate_SoftUpdateException( env );
	FolderSpecInitialize(env);
	InstallFileInitialize(env);
#if defined(XP_PC)
    SUWinSpecificInit( env );
#endif
}

#ifdef XP_MAC
#pragma export reset
#endif

JRI_PUBLIC_API(jbool)
native_netscape_softupdate_Trigger_UpdateEnabled(
    JRIEnv* env,
    struct java_lang_Class* clazz)
{
	XP_Bool enabled;
	PREF_GetBoolPref( AUTOUPDATE_ENABLE_PREF, &enabled);

	if (enabled)
        return JRITrue;
    else
        return JRIFalse;
}

JRI_PUBLIC_API(jbool)
native_netscape_softupdate_Trigger_StartSoftwareUpdate(JRIEnv* env,
                          struct java_lang_Class* clazz, 
                          struct java_lang_String *jurl, 
                          jint flags)

{
	char * url = NULL;

	url = (char*)JRI_GetStringPlatformChars( env, jurl, "", 0 );

	if (url != NULL)
	{
		/* This is a potential problem */
		/* We are grabbing any context, but we really need ours */
		/* The problem is that Java does not have access to MWContext */
		MWContext * cx;
		cx = XP_FindSomeContext();
		if (cx)
			return SU_StartSoftwareUpdate(cx, url, NULL, NULL, NULL, flags);
	    else
            return FALSE;
    }
    return FALSE;
}

extern int MK_OUT_OF_MEMORY;
extern JSClass lm_softup_class;

/*** private native VerifyJSObject ()V ***/
/* Check out Java declaration for docs */
JRI_PUBLIC_API(void)
native_netscape_softupdate_SoftwareUpdate_VerifyJSObject(JRIEnv* env,
														 struct netscape_softupdate_SoftwareUpdate* self,
														 struct netscape_javascript_JSObject *a)
{
   /*
    Get the object's class, and verify that it is of class SoftUpdate
	*/
	JSObject * jsobj;
	JSClass * jsclass;
	jsobj = JSJ_ExtractInternalJSObject(env, (HObject*)a);
	jsclass = JS_GetClass(jsobj);
	if ( jsclass != &lm_softup_class )
	{
		struct netscape_softupdate_SoftUpdateException* e;
		struct java_lang_String * errStr;
		errStr = netscape_softupdate_Strings_error_0005fBadJSArgument(env,
				class_netscape_softupdate_Strings(env));
		e = netscape_softupdate_SoftUpdateException_new( env,
								class_netscape_softupdate_SoftUpdateException(env), 
								errStr,
								-1);
		JRI_Throw(env, (struct java_lang_Throwable *)e);
		return;
	}
}

/*** private native OpenJARFile ()V ***/
JRI_PUBLIC_API(void)
native_netscape_softupdate_SoftwareUpdate_OpenJARFile(JRIEnv* env, struct netscape_softupdate_SoftwareUpdate* self)
{
	ZIG * jarData;
	char * jarFile;
	struct java_lang_String * jjarFile;
	int err;
	struct java_lang_String * errStr;
	struct netscape_softupdate_SoftUpdateException* e;

	XP_Bool enabled;
	PREF_GetBoolPref( AUTOUPDATE_ENABLE_PREF, &enabled);

	if (!enabled)
	{
		errStr = netscape_softupdate_Strings_error_0005fSmartUpdateDisabled(env,
				class_netscape_softupdate_Strings(env));
		e = netscape_softupdate_SoftUpdateException_new( env,
								class_netscape_softupdate_SoftUpdateException(env), 
								errStr,
								-1);
		JRI_Throw(env, (struct java_lang_Throwable *)e);
		return;
	}

	jjarFile = get_netscape_softupdate_SoftwareUpdate_jarName( env, self);
	jarFile = (char*)JRI_GetStringPlatformChars( env, jjarFile, "", 0 );
	if (jarFile == NULL) return;	/* error already signaled */

/* Open and initialize the JAR archive */
	jarData = SOB_new();
	if ( jarData == NULL )
	{
		errStr = netscape_softupdate_Strings_error_0005fUnexpected(env,
				class_netscape_softupdate_Strings(env));
		e = netscape_softupdate_SoftUpdateException_new( env,
								class_netscape_softupdate_SoftUpdateException(env), 
								errStr,
								-1);
		JRI_Throw(env, (struct java_lang_Throwable *)e);
		return;
	}
	err = SOB_pass_archive( ZIG_F_GUESS, 
						jarFile, 
						NULL, /* realStream->fURL->address,  */
						jarData);
	if ( err != 0 )
	{
		errStr = netscape_softupdate_Strings_error_0005fVerificationFailed(env,
				class_netscape_softupdate_Strings(env));
		e = netscape_softupdate_SoftUpdateException_new( env,
								class_netscape_softupdate_SoftUpdateException(env), 
								errStr,
								err);
		JRI_Throw(env, (struct java_lang_Throwable *)e);
		return;
	}
	

	/* Get the installer file name */
	{
		char * installerJarName = NULL;
		unsigned long fileNameLength;
		err = SOB_get_metainfo( jarData, NULL, INSTALLER_HEADER, (void**)&installerJarName, &fileNameLength);
		if (err != 0)
		{
			errStr = netscape_softupdate_Strings_error_0005fMissingInstaller(env,
				class_netscape_softupdate_Strings(env));
			e = netscape_softupdate_SoftUpdateException_new( env,
								class_netscape_softupdate_SoftUpdateException(env), 
								errStr,
								err);
			JRI_Throw(env, (struct java_lang_Throwable *)e);
			return;
		}
		set_netscape_softupdate_SoftwareUpdate_installerJarName(env, self, 
			JRI_NewStringPlatform(env, installerJarName, fileNameLength, "", 0));
	}
	set_netscape_softupdate_SoftwareUpdate_zigPtr(env, self, (long)jarData);
}

/*** private native CloseJARFile ()V ***/
extern JRI_PUBLIC_API(void)
native_netscape_softupdate_SoftwareUpdate_CloseJARFile(JRIEnv* env, struct netscape_softupdate_SoftwareUpdate* self)
{

	ZIG * jarData;
	jarData = (ZIG*)get_netscape_softupdate_SoftwareUpdate_zigPtr(env, self);
	if (jarData != NULL)
		SOB_destroy( jarData);
	set_netscape_softupdate_SoftwareUpdate_zigPtr(env, self, 0);
}

#define APPLESINGLE_MAGIC_HACK 1		/* Hack to automatically detect applesingle files until we get tdell to do the right thing */

/* Extracts a JAR target into the directory */
/* Always decodes AppleSingle files */
JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_SoftwareUpdate_NativeExtractJARFile(JRIEnv* env, 
															   struct netscape_softupdate_SoftwareUpdate* self, 
															   struct java_lang_String *jJarSource,
															   struct java_lang_String *finalFile)
{
	char * tempName = NULL;
	char * target = NULL;
	struct java_lang_String * tempNameJava = NULL;
	int result;
	

	target = (char*)JRI_GetStringPlatformChars( env, finalFile, "", 0 );

	if (target)
	{
		
		char *fn;
		char *end;
		char *URLfile = XP_PlatformFileToURL(target);

        fn = URLfile+7;     /* skip "file://" part */

		if ( end = XP_STRRCHR(fn, '/') )
			end[1] = 0;

		/* Create a temporary location */
		tempName = WH_TempName( xpURL, fn );
		XP_FREEIF(URLfile);
	}
	else
		tempName = WH_TempName( xpURL, NULL );
	
	if (tempName == NULL)
	{
		result = MK_OUT_OF_MEMORY;
		goto done;
	}

	
	{
		ZIG * jar;
		char * jarPath;

		/* Extract the file */
		jar = (ZIG *)get_netscape_softupdate_SoftwareUpdate_zigPtr(env, self);
		jarPath = (char*)JRI_GetStringPlatformChars( env, jJarSource, "", 0 );
		if (jarPath == NULL) {
		    /* out-of-memory error already signaled */
		    free(tempName);
		    return NULL;
		}
		result = SOB_verified_extract( jar, jarPath, tempName);
		
		if ( result == 0 )
		{
		/* If we have an applesingle file
		   decode it to a new file
		 */
			char * encodingName;
			unsigned long encodingNameLength;
			XP_Bool isApplesingle = FALSE;
			result = SOB_get_metainfo( jar, NULL, CONTENT_ENCODING_HEADER, (void**)&encodingName, &encodingNameLength);

#ifdef APPLESINGLE_MAGIC_HACK
			if (result != 0)
			{
				XP_File f;
				uint32 magic;
				f = XP_FileOpen(tempName, xpURL, XP_FILE_READ_BIN);
				XP_FileRead( &magic, sizeof(magic), f);
				XP_FileClose(f);
				isApplesingle = (magic == 0x00051600 );
				result = 0;
			}
#else
				isApplesingle = (( result == 0 ) &&
								(XP_STRNCMP(APPLESINGLE_MIME_TYPE, encodingName, XP_STRLEN( APPLESINGLE_MIME_TYPE ) == 0)));
#endif
			if ( isApplesingle )
			/* We have an AppleSingle file */
			/* Extract it to the new AppleFile, and get the URL back */
			{
				char * newTempName = NULL;
#ifdef XP_MAC
				result = SU_DecodeAppleSingle(tempName, &newTempName);
				if ( result == 0 )
				{
					XP_FileRemove( tempName, xpURL );
					XP_FREE(tempName);
					tempName = newTempName;
				}
				else
					XP_FileRemove( tempName, xpURL );
#else
				result = 0;
#endif
			}
		}
        else 
            result = XP_GetError();
	}

done:
	/* Create the return Java string if everything went OK */
	if (tempName && ( result == 0) )
	{
		tempNameJava = JRI_NewStringPlatform(env, tempName, XP_STRLEN( tempName), "", 0);
		if (tempNameJava == NULL)
			result = MK_OUT_OF_MEMORY;
		XP_FREE(tempName);
	}

	if ( (result != 0) || (tempNameJava == NULL) )
	{
		struct netscape_softupdate_SoftUpdateException* e;
		struct java_lang_String * errStr;
		errStr = netscape_softupdate_Strings_error_0005fExtractFailed(env,
				class_netscape_softupdate_Strings(env));
		e = netscape_softupdate_SoftUpdateException_new( env,
						class_netscape_softupdate_SoftUpdateException(env), 
						errStr,
						result);
		JRI_Throw(env, (struct java_lang_Throwable *)e);
		return NULL;
	}
	
	return tempNameJava;
}

/* getCertificates
 * native encapsulation that calls AppletClassLoader.getCertificates
 * we cannot call this method from Java because it is private.
 * The method cannot be made public because it is a security risk
 */
#ifndef XP_MAC
#include "netscape_applet_AppletClassLoader.h"
#else
#include "n_applet_AppletClassLoader.h"
#endif

/* From java.h (keeping the old hack, until include path is setup correctly on Mac) */
PR_PUBLIC_API(jref)
LJ_GetCertificates(JRIEnv* env, void *zigPtr, char *pathname);


JRI_PUBLIC_API(jref)
native_netscape_softupdate_SoftwareUpdate_getCertificates(JRIEnv* env, 
					struct netscape_softupdate_SoftwareUpdate* self, 
					jint zigPtr, 
					struct java_lang_String *name)
{
	char* pathname;
	if (!name)
		return NULL;

	pathname = (char *)JRI_GetStringUTFChars(env, name);
	if (!pathname)
		return NULL;

	return LJ_GetCertificates(env, (void *)zigPtr, pathname);
}

#ifdef XP_MAC
#include <Gestalt.h>
#endif

/*** private native NativeGestalt (Ljava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_SoftwareUpdate_NativeGestalt(JRIEnv* env, 
														struct netscape_softupdate_SoftwareUpdate* self, 
														struct java_lang_String *jselector)
{
#ifdef XP_MAC
	OSErr err;
	long response;
	char * selectorStr;
	OSType selector;

	selectorStr = (char*)JRI_GetStringPlatformChars( env, jselector, "", 0 );
	if ((selectorStr == NULL) ||
		(XP_STRLEN(selectorStr) != 4))
	{
		err = -5550;	/* gestaltUnknownErr */
		goto fail;
	}
	XP_MEMCPY(&selector, selectorStr, 4);
	err = Gestalt(selector, &response);
	if (err == noErr)
		return response;
	goto fail;	/* Drop through to failure */
#else
	int32 err;
	err = -1;
	goto fail;
#endif

fail:
	{
		struct netscape_softupdate_SoftUpdateException* e;
		struct java_lang_String * errStr;
		/* Just a random error message, it will never be seen, error code is the only important bit */
		errStr = netscape_softupdate_Strings_error_0005fExtractFailed(env,
				class_netscape_softupdate_Strings(env));
		e = netscape_softupdate_SoftUpdateException_new( env,
						class_netscape_softupdate_SoftUpdateException(env), 
						errStr,
						err);
		JRI_Throw(env, (struct java_lang_Throwable *)e);
	}
	return 0;
}

/*** private native ExtractDirEntries (Ljava/lang/String;)[Ljava/lang/String; ***/
JRI_PUBLIC_API(jref)
native_netscape_softupdate_SoftwareUpdate_ExtractDirEntries(
    JRIEnv* env,
    struct netscape_softupdate_SoftwareUpdate* self,
    struct java_lang_String * dir)
{
    int         size = 0;
    int         len = 0;
    int         dirlen;
    ZIG         *zigPtr;
    char        *Directory;
    char        *pattern = NULL;
    ZIG_Context *context;
    SOBITEM     *item;
    jref        StrArray = NULL;
    char        *buff;


    if (!dir) 
        goto bail;

    if ( !(zigPtr = (ZIG *)get_netscape_softupdate_SoftwareUpdate_zigPtr(env, self)) )
        goto bail;

    if ( !(Directory = (char*)JRI_GetStringPlatformChars( env, dir, "", 0 )) )
        goto bail;
    
    dirlen = XP_STRLEN(Directory);
    if ( (pattern = XP_ALLOC(dirlen + 3)) == NULL)
        goto bail;

    XP_STRCPY(pattern, Directory);
    XP_STRCPY(pattern+dirlen, "/*");


    /* it's either go through the JAR twice (first time just to get a count)
     * or go through once and potentially use lots of memory saving all the
     * strings.  In deference to Win16 and potentially large installs we're
     * going to loop through the JAR twice and take the performance hit;
     * no one installs very often anyway.
     */

    if ((context = SOB_find (zigPtr, pattern, ZIG_MF)) == NULL) 
        goto bail;

    while (SOB_find_next (context, &item) >= 0) 
        size++;

    SOB_find_end (context);

    if ( (StrArray = JRI_NewObjectArray(env, size, JRI_FindClass(env, "java/lang/String"), NULL)) == 0)
        goto bail;

    if ((context = SOB_find (zigPtr, pattern, ZIG_MF)) == NULL) 
        goto bail;

    size = 0;
    while (SOB_find_next (context, &item) >= 0) 
    {
         len = XP_STRLEN(item->pathname);
         /* subtract length of target directory + slash */
         len = len - (dirlen+1);
         if ( buff = XP_ALLOC (len+1) )
         {
             /* Don't copy the search directory part */
             XP_STRCPY (buff, (item->pathname)+dirlen+1);

             /* XXX -- use intl_makeJavaString() instead? */
             JRI_SetObjectArrayElement( env, StrArray, size++, 
                JRI_NewStringPlatform(env, buff, len, "", 0) );
             XP_FREE(buff);
         }
    }
    SOB_find_end (context);

bail:
    XP_FREEIF(pattern);
    return StrArray;
}


/*** private native NativeDeleteFile (Ljava/lang/String;)I ***/
extern JRI_PUBLIC_API(jint)
native_netscape_softupdate_SoftwareUpdate_NativeDeleteFile(
    JRIEnv* env, 
    struct netscape_softupdate_SoftwareUpdate* self, 
    struct java_lang_String *path)
{
    char *fName;

    if ( !(fName = (char*)JRI_GetStringPlatformChars( env, path, "", 0 )) )
        return -1;

    return XP_FileRemove ( fName, xpURL );
}

/*** private native NativeVerifyDiskspace (Ljava/lang/String;I)I ***/
extern JRI_PUBLIC_API(jint)
native_netscape_softupdate_SoftwareUpdate_NativeVerifyDiskspace(
    JRIEnv* env, 
    struct netscape_softupdate_SoftwareUpdate* self, 
    struct java_lang_String *path, 
    jint nBytes)
{
    char *fileSystem;

    if ( !(fileSystem = (char*)JRI_GetStringPlatformChars( env, path, "", 0 )) )
        return 0;

    return DiskSpaceAvailable(fileSystem, (int) nBytes);
}

/*** private native NativeMakeDirectory (Ljava/lang/String;)I ***/
extern JRI_PUBLIC_API(jint)
native_netscape_softupdate_SoftwareUpdate_NativeMakeDirectory(
    JRIEnv* env, 
    struct netscape_softupdate_SoftwareUpdate* self, 
    struct java_lang_String *path)
{
    char *dir;

    if ( !(dir = (char*)JRI_GetStringPlatformChars( env, path, "", 0 )) )
        return -1;

    return XP_MakeDirectoryR ( dir, xpURL );
}

