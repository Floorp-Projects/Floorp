/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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

#include "xp_mcom.h"
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

#ifndef MAX_PATH
#if defined(XP_WIN) || defined(XP_OS2)
#define MAX_PATH _MAX_PATH
#endif
#ifdef XP_UNIX
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

/* Makes sure that the path ends with a slash (or other platform end character)
 * @return  alloc'd new path that ends with a slash
 */
char *AppendSlashToDirPath(char * dirPath)
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
void PR_CALLBACK pickDirectoryCallback(void * a)
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


/* 
 * Directory manipulation 
 *
 * DirectoryTable holds the info about built-in directories:
 * Text name, security level, enum
 */
struct su_DirectoryTable DirectoryTable[] = 
{
	{"Plugins",         ePluginFolder,              TRUE},
	{"Program",         eProgramFolder,             FALSE},
	{"Communicator",    eCommunicatorFolder,        FALSE},
	{"User Pick",       ePackageFolder,             FALSE},
	{"Temporary",       eTemporaryFolder,           FALSE},
	{"Installed",       eInstalledFolder,           FALSE},
	{"Current User",    eCurrentUserFolder,         FALSE},

	{"NetHelp",         eNetHelpFolder,             FALSE},
	{"OS Drive",        eOSDriveFolder,             FALSE},
	{"File URL",        eFileURLFolder,             FALSE},

	{"Netscape Java Bin",     eJavaBinFolder,       FALSE},
	{"Netscape Java Classes", eJavaClassesFolder,   TRUE},
	{"Java Download",         eJavaDownloadFolder,  TRUE},

	{"Win System",      eWin_SystemFolder,          FALSE},
	{"Win System16",    eWin_System16Folder,        FALSE},
	{"Windows",         eWin_WindowsFolder,         FALSE},

	{"Mac System",      eMac_SystemFolder,          FALSE},
	{"Mac Desktop",     eMac_DesktopFolder,         FALSE},
	{"Mac Trash",       eMac_TrashFolder,           FALSE},
	{"Mac Startup",     eMac_StartupFolder,         FALSE},
	{"Mac Shutdown",    eMac_ShutdownFolder,        FALSE},
	{"Mac Apple Menu",  eMac_AppleMenuFolder,       FALSE},
	{"Mac Control Panel", eMac_ControlPanelFolder,  FALSE},
	{"Mac Extension",   eMac_ExtensionFolder,       FALSE},
	{"Mac Fonts",       eMac_FontsFolder,           FALSE},
	{"Mac Preferences", eMac_PreferencesFolder,     FALSE},

	{"Unix Local",      eUnix_LocalFolder,          FALSE},
	{"Unix Lib",        eUnix_LibFolder,            FALSE},

	{"",                eBadFolder,                 FALSE} /* Termination */
};

/* MapNameToEnum
 * maps name from the directory table to its enum */
su_DirSpecID MapNameToEnum(const char * name)
{
	int i = 0;
	XP_ASSERT( name );
	if ( name == NULL)
		return eBadFolder;

	while ( DirectoryTable[i].directoryName[0] != 0 )
	{
		if ( XP_STRCASECMP(name, DirectoryTable[i].directoryName) == 0 )
			return DirectoryTable[i].folderEnum;
		i++;
	}
	return eBadFolder;
}

