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
/* su_folderspec.h 
 * public stuff for su_folderspec.c
 */

/* su_DirSpecID
 * enums for different folder types
 */

#include "xp_mcom.h"

typedef enum su_DirSpecID {

	/* Common folders */
	eBadFolder,
	ePluginFolder,		/* where the plugins are */
	eProgramFolder,		/* Where the netscape program is */
	ePackageFolder,		/* Default application folder */
	eTemporaryFolder,   /* Temporary */
	eCommunicatorFolder,/* Communicator */
	eInstalledFolder,   /* Already installed component */
	eCurrentUserFolder, /* Navigator's current user */
	eNetHelpFolder,
	eOSDriveFolder,
	eFileURLFolder,

	/* Java folders */
	eJavaBinFolder,
	eJavaClassesFolder,
	eJavaDownloadFolder,

	/* Windows folders */
	eWin_WindowsFolder,
	eWin_SystemFolder,
	eWin_System16Folder,

	/* Macintosh folders */
	eMac_SystemFolder,
	eMac_DesktopFolder,
	eMac_TrashFolder,
	eMac_StartupFolder,
	eMac_ShutdownFolder,
	eMac_AppleMenuFolder,
	eMac_ControlPanelFolder,
	eMac_ExtensionFolder,
	eMac_FontsFolder,
	eMac_PreferencesFolder,

	/* Unix folders */
	eUnix_LocalFolder,
	eUnix_LibFolder

} su_DirSpecID;

typedef enum su_SecurityLevel {
	eOneFolderAccess,
	eAllFolderAccess
} su_SecurityLevel;

XP_BEGIN_PROTOS

/* FE_GetDirectoryPath
 * given a directory id, returns full path in native format
 * and ends with the platform specific directory separator ('/',':','\\')
 */
char * FE_GetDirectoryPath( su_DirSpecID folderID );
int    FE_ReplaceExistingFile(char *, XP_FileType, char *, XP_FileType, XP_Bool);

#ifdef WIN32
BOOL WFE_IsMoveFileExBroken();
#endif

XP_END_PROTOS
