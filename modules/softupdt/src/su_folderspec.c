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
 * netscape.softupdate.FolderSpec.java
 * native methods
 * created by atotic, 1/6/97
 */
#include "xp_mcom.h"
#include "jri.h"
#include "su_folderspec.h"

/* #include "prthread.h" */

#include "fe_proto.h"
#include "xp_str.h"
#include "prefapi.h"
#include "proto.h"
#include "prthread.h"
#include "prprf.h"
#include "fe_proto.h"
#include "xpgetstr.h"

#define IMPLEMENT_netscape_softupdate_FolderSpec
#define IMPLEMENT_netscape_softupdate_SoftwareUpdate

#ifndef XP_MAC
#include "_jri/netscape_softupdate_FolderSpec.c"
#include "_jri/netscape_softupdate_SoftwareUpdate.h"
#else
#include "n_softupdate_FolderSpec.c"
#include "n_softupdate_SoftwareUpdate.h"
#endif

#ifndef MAX_PATH
#if defined(XP_WIN) || defined(XP_OS2)
#define MAX_PATH _MAX_PATH
#endif
#ifdef XP_UNIX
#ifdef NSPR20
#include "md/prosdep.h"
#else
#include "prosdep.h"
#endif
#if defined(HPUX) || defined(SCO)
/*
** HPUX: PATH_MAX is defined in <limits.h> to be 1023, but they
**       recommend that it not be used, and that pathconf() be
**       used to determine the maximum at runtime.
** SCO:  This is what MAXPATHLEN is set to in <arpa/ftp.h> and
**       NL_MAXPATHLEN in <nl_types.h>.  PATH_MAX is defined in
**       <limits.h> to be 256, but the comments in that file
**       claim the setting is wrong.
*/
#define MAX_PATH 1024
#else
#define MAX_PATH PATH_MAX
#endif
#endif
#endif

extern int SU_INSTALL_ASK_FOR_DIRECTORY;

/* Standard Java initialization for native classes */
void 
FolderSpecInitialize(JRIEnv * env)
{
	use_netscape_softupdate_FolderSpec( env );
}

/* Makes sure that the path ends with a slash (or other platform end character)
 * @return  alloc'd new path that ends with a slash
 */
static char *
AppendSlashToDirPath(char * dirPath)
{
    char pathSeparator; /* Gross, but harmless */
    int32 newLen;
    char * newPath;

#ifdef XP_WIN
    pathSeparator = '\\';
#elif defined(XP_MAC)
    pathSeparator = ':';
#else /* XP_UNIX */
    pathSeparator = '/';
#endif
    newLen = XP_STRLEN(dirPath) + 2;
    newPath = (char*)XP_ALLOC(newLen);
    if (newPath)
    {
        newPath[0] = 0;
        XP_STRCAT(newPath, dirPath);
         /* Append the slash if missing */
        if (newPath[newLen - 3] != pathSeparator)
        {
            newPath[newLen - 2] = pathSeparator;
            newPath[newLen - 1] = 0;
        }
    }
    return newPath;
}

/* su_PickDirTimer
 * keeps track of all SU specific data needed for the stream
 */ 
typedef struct su_PickDirTimer_struct {
    MWContext * context;
    char * fileName;
    char * prompt;
    XP_Bool done;
} su_PickDirTimer;


/* Callback for FE_PromptForFileName */
PR_STATIC_CALLBACK(void) 
GetDirectoryPathCallbackFunction(MWContext *context,
									char *file_name,
									void *closure)
{
    su_PickDirTimer *t = (su_PickDirTimer*)closure;
    
    if (file_name)
    {
    	char * newName = WH_FileName(file_name, xpURL);
    	if ( newName )
        	t->fileName = AppendSlashToDirPath(newName);
        XP_FREEIF(newName);
        XP_FREE(file_name);
    }
    t->done = TRUE;
}

/* Callback for the timer set by FE_SetTimer */
PR_STATIC_CALLBACK(void) 
pickDirectoryCallback(void * a)
{
    su_PickDirTimer *t = (su_PickDirTimer *)a;
    int err;
#ifdef XP_UNIX
	err = FE_PromptForFileName (XP_FindSomeContext(), t->prompt, NULL, FALSE, TRUE,
								 GetDirectoryPathCallbackFunction, a );
#else
	err = FE_PromptForFileName (t->context, t->prompt, NULL, FALSE, TRUE,
								 GetDirectoryPathCallbackFunction, a );
#endif
    if ( err != 0)  /* callback will not run */
        t->done = TRUE;
}

/* NativePickDefaultDirectory
 * Asks the user where he'd like to install the package
 */

JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_FolderSpec_NativePickDefaultDirectory(
    JRIEnv* env, 
	struct netscape_softupdate_FolderSpec* self)
{
	struct java_lang_String * jPackageName, * newName= NULL;
    su_PickDirTimer callback;
    char * packageName;
    char prompt[200];

 
    callback.context = XP_FindSomeContext();
    callback.fileName = NULL;
    callback.done = FALSE;
 	/* Get the name of the package to prompt for */
	jPackageName = get_netscape_softupdate_FolderSpec_userPackageName( env, self);
	packageName = (char*)JRI_GetStringPlatformChars( env, jPackageName, "", 0 );

	if (packageName)
	{
		/* In Java thread now, and need to call FE_PromptForFileName
         * from the main thread
         * post an event on a timer, and busy-wait until it completes.
         */
       PR_snprintf(prompt, 200, XP_GetString(SU_INSTALL_ASK_FOR_DIRECTORY), packageName); 
       callback.prompt = prompt;
       FE_SetTimeout( pickDirectoryCallback, &callback, 1 );
        while (!callback.done)  /* Busy loop for now */
             PR_Sleep(PR_INTERVAL_NO_WAIT); /* java_lang_Thread_yield(WHAT?); */
	}

    if (callback.fileName != NULL)
	{
		newName = JRI_NewStringPlatform(env, callback.fileName, XP_STRLEN( callback.fileName), "", 0);
		XP_FREE( callback.fileName);
	}
	return newName;
}

/* 
 *Directory manipulation 
 */

/* Entry for the DirectoryTable[] */
struct su_DirectoryTable
{
	char * directoryName;			/* The formal directory name */
	su_SecurityLevel securityLevel;	/* Security level */
	su_DirSpecID folderEnum;		/* Directory ID */
};

/* DirectoryTable holds the info about built-in directories:
 * Text name, security level, enum
 */
static struct su_DirectoryTable DirectoryTable[] = 
{
	{"Plugins", eAllFolderAccess, ePluginFolder},
	{"Program", eAllFolderAccess, eProgramFolder},
	{"Communicator", eAllFolderAccess, eCommunicatorFolder},
	{"User Pick", eAllFolderAccess, ePackageFolder},
	{"Temporary", eAllFolderAccess, eTemporaryFolder},
	{"Installed", eAllFolderAccess, eInstalledFolder},
	{"Current User", eAllFolderAccess, eCurrentUserFolder},

	{"NetHelp", eAllFolderAccess, eNetHelpFolder},
	{"OS Drive", eAllFolderAccess, eOSDriveFolder},
	{"File URL", eAllFolderAccess, eFileURLFolder},

	{"Netscape Java Bin", eAllFolderAccess, eJavaBinFolder},
	{"Netscape Java Classes", eAllFolderAccess, eJavaClassesFolder},
	{"Java Download", eOneFolderAccess, eJavaDownloadFolder},

	{"Win System", eAllFolderAccess, eWin_SystemFolder},
	{"Win System16", eAllFolderAccess, eWin_System16Folder},
	{"Windows", eAllFolderAccess, eWin_WindowsFolder},

	{"Mac System", eAllFolderAccess, eMac_SystemFolder},
	{"Mac Desktop", eAllFolderAccess, eMac_DesktopFolder},
	{"Mac Trash", eAllFolderAccess, eMac_TrashFolder},
	{"Mac Startup", eAllFolderAccess, eMac_StartupFolder},
	{"Mac Shutdown", eAllFolderAccess, eMac_ShutdownFolder},
	{"Mac Apple Menu", eAllFolderAccess, eMac_AppleMenuFolder},
	{"Mac Control Panel", eAllFolderAccess, eMac_ControlPanelFolder},
	{"Mac Extension", eAllFolderAccess, eMac_ExtensionFolder},
	{"Mac Fonts", eAllFolderAccess, eMac_FontsFolder},
	{"Mac Preferences", eAllFolderAccess, eMac_PreferencesFolder},

	{"Unix Local", eAllFolderAccess, eUnix_LocalFolder},
	{"Unix Lib", eAllFolderAccess, eUnix_LibFolder},

	{"", eAllFolderAccess, eBadFolder}	/* Termination line */
};

/* MapNameToEnum
 * maps name from the directory table to its enum */
static su_DirSpecID MapNameToEnum(const char * name)
{
	int i = 0;
	XP_ASSERT( name );
	if ( name == NULL)
		return eBadFolder;

	while ( DirectoryTable[i].directoryName[0] != 0 )
	{
		if ( strcmp(name, DirectoryTable[i].directoryName) == 0 )
			return DirectoryTable[i].folderEnum;
		i++;
	}
	return eBadFolder;
}

/**
 *  GetNativePickPath -- return a native path equivalent of a XP path
 */

JRI_PUBLIC_API(struct java_lang_String *)
native_netscape_softupdate_FolderSpec_GetNativePath (JRIEnv* env, 
					struct netscape_softupdate_FolderSpec* self, 
					struct java_lang_String *a) 
{
	struct java_lang_String * nativePath = NULL;
	char *xpPath, *p;
	char pathSeparator;

#define XP_PATH_SEPARATOR  '/'

#ifdef XP_WIN
	pathSeparator = '\\';
#elif defined(XP_MAC)
	pathSeparator = ':';
#else /* XP_UNIX */
	pathSeparator = '/';
#endif

	p = xpPath = (char *) JRI_GetStringUTFChars (env, a); /* UTF OK */

	/*
	 * Traverse XP path and replace XP_PATH_SEPARATOR with
	 * the platform native equivalent
	 */
	if ( p == NULL )
	{
		xpPath = "";
	}
	else
	{
		while ( *p )
		{
			if ( *p == XP_PATH_SEPARATOR )
				*p = pathSeparator;
			++p;
		}
	}

	nativePath = JRI_NewStringUTF(env, xpPath, XP_STRLEN( xpPath)); /* UTF OK */

	return nativePath;
}

/*
 * NativeGetDirectoryPath
 */
JRI_PUBLIC_API(jint)
native_netscape_softupdate_FolderSpec_NativeGetDirectoryPath(JRIEnv* env,
						struct netscape_softupdate_FolderSpec* self)
{
	su_DirSpecID folderID;
	char *		folderName;
	char *		folderPath = NULL;
	struct java_lang_String * jFolderName;

	/* Get the name of the package to prompt for */

	jFolderName = get_netscape_softupdate_FolderSpec_folderID( env, self);
	folderName = (char*)JRI_GetStringUTFChars( env, jFolderName ); /* UTF OK */

	folderID = MapNameToEnum(folderName);
    switch (folderID)
    {
    case eBadFolder:
 		return netscape_softupdate_FolderSpec_INVALID_PATH_ERR;

    case eCurrentUserFolder:
        {
	        char dir[MAX_PATH];
	        int len = MAX_PATH;
	        if ( PREF_GetCharPref("profile.directory", dir, &len) == PREF_NOERROR)
	        {
	        	char * platformDir = WH_FileName(dir, xpURL);
	        	if (platformDir)
		            folderPath = AppendSlashToDirPath(platformDir);
	    		XP_FREEIF(platformDir);
	        }
        }
        break;

    default:
    	/* Get the FE path */
	    folderPath = FE_GetDirectoryPath( folderID);
        break;
   }


	/* Store it in the object */
	if (folderPath != NULL)
	{
		struct java_lang_String * jFolderPath;
		jFolderPath = JRI_NewStringPlatform(env, folderPath, strlen( folderPath), "", 0);
		if ( jFolderPath != NULL)
			set_netscape_softupdate_FolderSpec_urlPath(env, self, jFolderPath);

        XP_FREE(folderPath);
		return 0;
	}

	return netscape_softupdate_FolderSpec_INVALID_PATH_ERR;
}

