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
/* su_instl.c
 * netscape.softupdate.InstallFile.java
 * native implementation
 */

/* The following two includes are unnecessary, but prevent
 * IS_LITTLE_ENDIAN warnings */
#include "xp_mcom.h"
#include "jri.h"

#define IMPLEMENT_netscape_softupdate_InstallFile

#ifndef XP_MAC
#include "_jri/netscape_softupdate_InstallFile.c"
#else
#include "n_softupdate_InstallFile.c"
#endif

#define IMPLEMENT_netscape_softupdate_InstallExecute

#ifndef XP_MAC
#include "_jri/netscape_softupdate_InstallExecute.c"
#else
#include "n_softupdate_InstallExecute.c"
#endif

#define IMPLEMENT_netscape_softupdate_InstallPatch

#ifndef XP_MAC
#include "_jri/netscape_softupdate_InstallPatch.c"
#else
#include "n_softupdate_InstallPatch.c"
#endif

#define IMPLEMENT_netscape_softupdate_SoftUpdateException

#ifndef XP_MAC
#include "_jri/netscape_softupdate_SoftUpdateException.h"
#else
#include "n_s_SoftUpdateException.h"
#endif

#define IMPLEMENT_netscape_softupdate_Strings

#ifndef XP_MAC
#include "_jri/netscape_softupdate_Strings.h"
#else
#include "netscape_softupdate_Strings.h"
#endif

#define IMPLEMENT_netscape_softupdate_InstallDelete

#ifndef XP_MAC
#include "_jri/netscape_softupdate_InstallDelete.c"
#else
#include "n_softupdate_InstallDelete.c"
#endif

#define IMPLEMENT_netscape_softupdate_SoftwareUpdate

#ifndef XP_MAC
#include "_jri/netscape_softupdate_SoftwareUpdate.h"
#else
#include "n_softupdate_SoftwareUpdate.h"
#endif



#include "fe_proto.h"
#include "zig.h"
#include "xp_file.h"
#include "su_folderspec.h"
#include "su_instl.h"
#include "softupdt.h"
#include "NSReg.h"
#include "gdiff.h"


extern int MK_OUT_OF_MEMORY;

static 	XP_Bool	rebootShown = FALSE;
#ifdef XP_WIN16
static 	XP_Bool	utilityScheduled = FALSE;
#endif

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

extern REGERR  fe_DeleteOldFileLater(char * filename);


/* Standard Java initialization for native methods classes
 */
void InstallFileInitialize(JRIEnv * env)
{
	use_netscape_softupdate_InstallFile( env );
	use_netscape_softupdate_InstallExecute( env );
	use_netscape_softupdate_InstallDelete( env );
    use_netscape_softupdate_InstallPatch( env );
}

/* NativeComplete 
 * copies the file to its final location
 * Tricky, we need to create the directories 
 */
JRI_PUBLIC_API(jint)
native_netscape_softupdate_InstallFile_NativeComplete(JRIEnv* env, 
					struct netscape_softupdate_InstallFile* self)
{
	char * currentName;
	char * finalName = NULL;
	char * finalNamePlatform;
	struct java_lang_String * currentNameJava;
	struct java_lang_String * finalNameJava;
	int result;

	/* Get the names */
	currentNameJava = get_netscape_softupdate_InstallFile_tempFile( env, self);
	currentName = (char*)JRI_GetStringPlatformChars( env, currentNameJava, "", 0);

	finalNameJava = get_netscape_softupdate_InstallFile_finalFile( env, self);
	finalNamePlatform = (char*)JRI_GetStringPlatformChars( env, finalNameJava, "", 0);
	finalName = XP_PlatformFileToURL(finalNamePlatform);
	
    if ( finalName == NULL || currentName == NULL ) {
        /* memory or JRI problems */
        XP_FREEIF(finalName);
        return -1;
    }
    else {
        /* convert finalName name to xpURL form by stripping "file://" */
        char *temp = XP_STRDUP(&finalName[7]);
        XP_FREE(finalName);
        finalName = temp;
    }

    if (finalName != NULL)
	{
        if ( XP_STRCMP(finalName, currentName) == 0 ) {
            /* No need to rename, they are the same */
            result = 0;
        }
        else {
            XP_StatStruct s;
            if ( XP_Stat( finalName, &s, xpURL ) != 0 ) {
                /* Target file doesn't exist, try to rename file */
                result = XP_FileRename(currentName, xpURL, finalName, xpURL);
            }
            else {
                /* Target exists, can't trust XP_FileRename--do platform
                 * specific stuff in FE_ReplaceExistingFile()
				 */
                result = -1;
            }
        }
	}
	else {
        /* memory problem */
		return -1;
    }

	if (result != 0)
	{
		XP_StatStruct s;
		if ( XP_Stat( finalName, &s, xpURL ) == 0 )
		/* File already exists, need to remove the original */
		{
            XP_Bool force = get_netscape_softupdate_InstallFile_force(env, self);
            result = FE_ReplaceExistingFile(currentName, xpURL, finalName, xpURL, force);

            if ( result == REBOOT_NEEDED ) {
#ifdef XP_WIN16
                if (!utilityScheduled) {
                    utilityScheduled = TRUE;
                    FE_ScheduleRenameUtility();
                }
#endif
            }
		}
		else
		/* Directory might not exist, check and create if necessary */
		{
			char separator;
			char * end;
			separator = '/';
			end = XP_STRRCHR(finalName, separator);
			if (end)
			{
				end[0] = 0;
				result = XP_MakeDirectoryR( finalName, xpURL);
				end[0] = separator;
				if ( 0 == result )
					result = XP_FileRename(currentName, xpURL, finalName, xpURL);
			}
		}
#ifdef XP_UNIX
		/* Last try, can't rename() across file systems on UNIX */
		if ( -1 == result )
		{
			result = FE_CopyFile(currentName, finalName);
		}
#endif
	}
	XP_FREEIF(finalName);
	return result;
}

/* Removes the temporary file */
JRI_PUBLIC_API(void)
native_netscape_softupdate_InstallFile_NativeAbort(JRIEnv* env, 
                              struct netscape_softupdate_InstallFile* self)
{
	char * currentName;
	struct java_lang_String * currentNameJava;
	int result;

    /* Get the names */
	currentNameJava = get_netscape_softupdate_InstallFile_tempFile( env, self);
	currentName = (char*)JRI_GetStringPlatformChars( env, currentNameJava, "", 0);

    result = XP_FileRemove(currentName, xpURL);
    XP_ASSERT(result != 0); /* need to fe_deletefilelater() or something */
}

/* Finds out if the file exists
 */
JRI_PUBLIC_API(jbool)
native_netscape_softupdate_InstallFile_NativeDoesFileExist(JRIEnv* env, 
						struct netscape_softupdate_InstallFile* self)
{
	char * fileName;
	char * fileNamePlatform;
	struct java_lang_String * fileNameJava;
	int32 err;
	XP_StatStruct statinfo;
    XP_Bool replace = FALSE;

	fileNameJava = get_netscape_softupdate_InstallFile_finalFile( env, self);
	fileNamePlatform = (char*)JRI_GetStringPlatformChars( env, fileNameJava, "", 0);
	fileName = XP_PlatformFileToURL(fileNamePlatform);
	if (fileName != NULL)
	{
		char * temp = XP_STRDUP(&fileName[7]);
		XP_FREEIF(fileName);
		fileName = temp;

		if (fileName)
		{
    		err = XP_Stat(fileName, &statinfo, xpURL);
			if (err != -1)
			{
				replace = TRUE;
			}
        }
	}
	XP_FREEIF(fileName);
	return replace;

}


/*---------------------------------------
 *
 *   InstallExecute native methods
 *
 *---------------------------------------*/

/* Executes the extracted binary
 */
JRI_PUBLIC_API(void)
native_netscape_softupdate_InstallExecute_NativeComplete(JRIEnv* env, 
						struct netscape_softupdate_InstallExecute* self)
{
	char * cmdline;
    char * filename;
	int32 err;

	filename = (char*)JRI_GetStringPlatformChars( 
        env, 
        get_netscape_softupdate_InstallExecute_tempFile( env, self ),
        "",
        0 );

	cmdline = (char*)JRI_GetStringPlatformChars( 
        env, 
        get_netscape_softupdate_InstallExecute_cmdline( env, self ),
        "",
        0 );

    err = FE_ExecuteFile( filename, cmdline );
    XP_ASSERT( err == 0 );
	if ( err != 0 )
	{
		struct netscape_softupdate_SoftUpdateException* e;
		struct java_lang_String * errStr;
		errStr = netscape_softupdate_Strings_error_0005fUnexpected(env,
				class_netscape_softupdate_Strings(env));

		e = netscape_softupdate_SoftUpdateException_new( env,
								class_netscape_softupdate_SoftUpdateException(env),
								errStr,
								err);
		return;
	}
}

/* Removes the temporary file */
JRI_PUBLIC_API(void)
native_netscape_softupdate_InstallExecute_NativeAbort(JRIEnv* env, 
                              struct netscape_softupdate_InstallExecute* self)
{
	char * currentName;
	struct java_lang_String * currentNameJava;
	int result;

    /* Get the names */
	currentNameJava = get_netscape_softupdate_InstallExecute_tempFile(env,self);
	currentName = (char*)JRI_GetStringPlatformChars(env, currentNameJava,"",0);

    result = XP_FileRemove(currentName, xpURL);
    XP_ASSERT(result == 0);
}



/*---------------------------------------
 *
 *   InstallDelete native methods
 *
 *---------------------------------------*/

/* Executes the extracted binary
 */
JRI_PUBLIC_API(jint)
native_netscape_softupdate_InstallDelete_NativeComplete(JRIEnv* env, 
						struct netscape_softupdate_InstallDelete* self)
{
	char * fileName;
	char * fileNamePlatform;
	struct java_lang_String * fileNameJava;
 	int32 err;
	XP_StatStruct statinfo;
    int deleteStatus;

    deleteStatus = get_netscape_softupdate_InstallDelete_deleteStatus( env, self);
    fileNameJava = get_netscape_softupdate_InstallDelete_finalFile( env, self);
	fileNamePlatform = (char*)JRI_GetStringPlatformChars( env, fileNameJava, "", 0);
	fileName = XP_PlatformFileToURL(fileNamePlatform);
    if (fileName != NULL)
	{
		char * temp = XP_STRDUP(&fileName[7]);
		XP_FREEIF(fileName);
		fileName = temp;
		if (fileName)
		{
			err = XP_Stat(fileName, &statinfo, xpURL);
			if (err != -1)
			{
				if ( XP_STAT_READONLY( statinfo ) )
				{
					err = get_netscape_softupdate_InstallDelete_FILE_READ_ONLY( env, self);
				}
				else if (!S_ISDIR(statinfo.st_mode))
				{
					err = XP_FileRemove ( fileName, xpURL );
					if (err != 0)
                    {
#ifdef XP_PC
/* REMIND  need to move function to generic XP file */
						err = REBOOT_NEEDED;
						fe_DeleteOldFileLater( (char*)fileNamePlatform );
#endif
					}

				}
                else
                {
                    err = get_netscape_softupdate_InstallDelete_FILE_IS_DIRECTORY( env, self);;
                }
			}
			else
			{
				err = get_netscape_softupdate_InstallDelete_FILE_DOES_NOT_EXIST( env, self);
			}
    		
        }
		else
		{
			err = -1;
		}
	}   

	
	XP_FREEIF(fileName);
	return err;
}


/*
 * Checks the status of the file
 */
extern JRI_PUBLIC_API(jint)
native_netscape_softupdate_InstallDelete_NativeCheckFileStatus(JRIEnv* env,
                        struct netscape_softupdate_InstallDelete* self)
{
	char * fileName;
	char * fileNamePlatform;
	struct java_lang_String * fileNameJava;
	int32 err;
	XP_StatStruct statinfo;

	fileNameJava = get_netscape_softupdate_InstallDelete_finalFile( env, self);
	fileNamePlatform = (char*)JRI_GetStringPlatformChars( env, fileNameJava, "", 0);
	fileName = XP_PlatformFileToURL(fileNamePlatform);
	if (fileName != NULL)
	{
		char * temp = XP_STRDUP(&fileName[7]);
		XP_FREEIF(fileName);
		fileName = temp;

		if (fileName)
		{
    		err = XP_Stat(fileName, &statinfo, xpURL);
			if (err != -1)
			{
				if ( XP_STAT_READONLY( statinfo ) )
				{
					err = get_netscape_softupdate_InstallDelete_FILE_READ_ONLY( env, self);
				}
                else if (!S_ISDIR(statinfo.st_mode))
                {
                    ;
                }
                else
                {
                    err = get_netscape_softupdate_InstallDelete_FILE_IS_DIRECTORY( env, self);
                }
			}
			else
			{
				err = get_netscape_softupdate_InstallDelete_FILE_DOES_NOT_EXIST( env, self);
			}
        }
		else
		{
			err = -1;
		}
	
	}
	else
	{
		err = -1;
	}
	XP_FREEIF(fileName);
	return err;

}

/*---------------------------------------
 *
 *   InstallPatch native methods
 *
 *---------------------------------------*/

/*** private native NativePatch (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String; ***/
JRI_PUBLIC_API(struct java_lang_String*)
native_netscape_softupdate_InstallPatch_NativePatch(JRIEnv* env,
                struct netscape_softupdate_InstallPatch* self,
                struct java_lang_String *jSrcFile,
                struct java_lang_String *jDiffURL)
{
    char * srcfile  = NULL;
    char * diffURL  = NULL;
    char * fullSrcURL   = NULL;
    char * srcURL = NULL;
    char * newfileURL = NULL;
    char * newfile = NULL;
    int32 err = GDIFF_OK;
    struct java_lang_String * jNewFile = NULL;

    /* get all the required filenames in the correct URL format */

    srcfile = (char*)JRI_GetStringPlatformChars( env, jSrcFile, "", 0 );
    diffURL = (char*)JRI_GetStringPlatformChars( env, jDiffURL, "", 0 );

    if ( srcfile != NULL && diffURL != NULL )
    {
        fullSrcURL = XP_PlatformFileToURL( srcfile );
        if ( fullSrcURL != NULL ) 
        {
            char ch;
            char *p;

            srcURL = fullSrcURL+7; /* skip "file://" part */
            p = XP_STRRCHR( srcURL, '/' );
            if ( p != NULL ) {
                ch = p[1];
                p[1] = 0;
                newfileURL = WH_TempName( xpURL, srcURL );
                p[1] = ch;
            }
        }
    }


    /* apply the patch if conversions worked */

    if ( newfileURL != NULL ) {
        err = SU_PatchFile( srcURL, xpURL, diffURL, xpURL, newfileURL, xpURL );
    }
    else {
        /* String conversions failed -- probably out of memory */
        err = netscape_softupdate_SoftwareUpdate_UNEXPECTED_ERROR;
    }


    if ( err == GDIFF_OK ) 
    {
        /* everything's fine, convert to java string and return */
        newfile = WH_FileName( newfileURL, xpURL );
        jNewFile = JRI_NewStringPlatform(env,newfile,XP_STRLEN(newfile),"",0);
    }
    else
    {
        /* convert GDIFF_ERR to SoftwareUpdate error and throw it */
        /* XXX: replace with correct error message! */
		struct netscape_softupdate_SoftUpdateException* e;
		struct java_lang_String * errStr;
		errStr = netscape_softupdate_Strings_error_0005fUnexpected(env,
				class_netscape_softupdate_Strings(env));
		e = netscape_softupdate_SoftUpdateException_new( env,
								class_netscape_softupdate_SoftUpdateException(env), 
								errStr,
								err);
		JRI_Throw(env, (struct java_lang_Throwable *)e);
    }
    

    /* cleanup */

    if ( diffURL != NULL )
        XP_FileRemove( diffURL, xpURL );

    XP_FREEIF( newfile );
    XP_FREEIF( newfileURL );
    XP_FREEIF( fullSrcURL );

    return (jNewFile);

}



/*** private native NativeReplace (Ljava/lang/String;Ljava/lang/String;)I ***/
JRI_PUBLIC_API(jint)
native_netscape_softupdate_InstallPatch_NativeReplace(JRIEnv* env,
                struct netscape_softupdate_InstallPatch* self,
                struct java_lang_String *jTargetfile,
                struct java_lang_String *jNewfile)
{
    char * targetfile   = NULL;
    char * newfile      = NULL;
    char * targetURL    = NULL;
    char * newURL       = NULL;
    char * pTarget;
    char * pNew;
    int    err = SU_SUCCESS;

    targetfile = (char*)JRI_GetStringPlatformChars( env, jTargetfile, "", 0 );
    newfile    = (char*)JRI_GetStringPlatformChars( env, jNewfile, "", 0 );

    if ( targetfile != NULL && newfile != NULL )
    {
        targetURL = XP_PlatformFileToURL( targetfile );
        newURL    = XP_PlatformFileToURL( newfile );

        if ( targetURL != NULL && newURL != NULL )
        {
            XP_StatStruct st;

            pTarget = targetURL+7;
            pNew    = newURL+7;

            if ( XP_Stat( pTarget, &st, xpURL ) == SU_SUCCESS ) {
                /* file still exists */
                err = FE_ReplaceExistingFile( pNew, xpURL, pTarget, xpURL, 0 );
#ifdef XP_WIN16
                if ( err == REBOOT_NEEDED && !utilityScheduled) {
                    utilityScheduled = TRUE;
                    FE_ScheduleRenameUtility();
                }
#endif
            }
            else {
                /* someone got rid of the target file? */
                /* can do simple rename, but assert */
                err = XP_FileRename( pNew, xpURL, pTarget, xpURL );
                XP_ASSERT( err == SU_SUCCESS );
                XP_ASSERT(0);
            }
        }
        else {
            err = -1;
        }
    }
    else {
        err = -1;
    }

    XP_FREEIF( targetURL );
    XP_FREEIF( newURL );

    return (err);
}


/*** private native NativeDeleteFile (Ljava/lang/String;)V ***/
JRI_PUBLIC_API(void)
native_netscape_softupdate_InstallPatch_NativeDeleteFile(JRIEnv* env,
                struct netscape_softupdate_InstallPatch* self,
                struct java_lang_String *jFilename)
{
    char * filename = NULL;
    char * fnameURL = NULL;
    char * fname = NULL;
    int result;
    XP_StatStruct s;

    filename = (char*)JRI_GetStringPlatformChars( env, jFilename, "", 0 );
    XP_ASSERT( filename );

    if ( filename != NULL ) {
        fnameURL = XP_PlatformFileToURL( filename );
        XP_ASSERT( fnameURL );

        if ( fnameURL != NULL ) {
            fname = fnameURL+7;

		    if ( XP_Stat( fname, &s, xpURL ) == 0 ) {
                /* file is still here */
                result = XP_FileRemove( fname, xpURL );
                if ( result != SU_SUCCESS ) {
                    /* schedule for later */
                }
            }
        }
    }


    XP_FREEIF( fnameURL );
}

