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
/* su_folderspec.h 
 * public stuff for su_folderspec.c
 */

/* su_DirSpecID
 * enums for different folder types
 */

#include "xp_mcom.h"
#include "prtypes.h"
#include "ntypes.h"

/* su_PickDirTimer
 * keeps track of all SU specific data needed for the stream
 */ 
typedef struct su_PickDirTimer_struct {
    MWContext * context;
    char * fileName;
    char * prompt;
    XP_Bool done;
} su_PickDirTimer;


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

struct su_DirectoryTable
{
	char * directoryName;			/* The formal directory name */
	su_DirSpecID folderEnum;		/* Directory ID */
	XP_Bool bJavaDir;               /* TRUE is a Java-capable directory */
};

extern struct su_DirectoryTable DirectoryTable[];

XP_BEGIN_PROTOS

/* FE_GetDirectoryPath
 * given a directory id, returns full path in native format
 * and ends with the platform specific directory separator ('/',':','\\')
 */
char * FE_GetDirectoryPath( su_DirSpecID folderID );
int    FE_ReplaceExistingFile(char *, XP_FileType, char *, XP_FileType, XP_Bool);
void PR_CALLBACK pickDirectoryCallback(void * a);


#ifdef WIN32
BOOL WFE_IsMoveFileExBroken();
#endif

/* Makes sure that the path ends with a slash (or other platform end character)
 * @return  alloc'd new path that ends with a slash
 */
char * AppendSlashToDirPath(char * dirPath);

/* MapNameToEnum
 * maps name from the directory table to its enum */
su_DirSpecID MapNameToEnum(const char * name);

XP_END_PROTOS
