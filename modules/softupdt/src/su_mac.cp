/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*-
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
/* su_mac.c
 * Mac specific softupdate routines
 */

#include "xp_mcom.h"
#include "su_instl.h"
#include "su_folderspec.h"
#include "fe_proto.h"
#include "xp_str.h"
#include "net.h"
#include <Folders.h>

// MacFE
#include "ufilemgr.h"
#include "uprefd.h"
#include "macutil.h"
#include "macfeprefs.h"

/* Given a system folder enum, returns a full path, in the URL form */
char * GetDirectoryPathFromSystemEnum(OSType folder)
{
	OSErr err;
	short vRefNum;
	long dirID;
	FSSpec folderSpec;
	
	err = FindFolder(kOnSystemDisk,folder,true,&vRefNum,&dirID);
	if (err != noErr)
		return NULL;

	err = CFileMgr::FolderSpecFromFolderID( vRefNum, dirID, folderSpec);
	if (err != noErr)
		return NULL;

	return CFileMgr::PathNameFromFSSpec(folderSpec, true);
}
extern OSErr FindNetscapeFolder(FSSpec* outSpec);
extern OSErr FindPluginFolder(FSSpec * spec, Boolean create);
extern "C" OSErr ConvertUnixPathToMacPath(const char *, char **);
/* Returns the URL format folder path */

PR_PUBLIC_API(char *) FE_GetDirectoryPath( su_DirSpecID folderID)
{
	char * path = NULL;
	OSErr err;
	switch( folderID)
	{
	case ePluginFolder:
		{
			FSSpec spec;
			if ( FindPluginFolder(&spec, true) != noErr )
				return NULL;
			path = CFileMgr::PathNameFromFSSpec(spec, true);
		}
		break;
	case eProgramFolder:
	case eCommunicatorFolder:
		{
			FSSpec spec= CPrefs::GetFolderSpec(CPrefs::NetscapeFolder);
			path = CFileMgr::PathNameFromFSSpec(spec, true);
		}
		break;
	case ePackageFolder:
		path = NULL;
	case eTemporaryFolder:
		path = GetDirectoryPathFromSystemEnum(kTemporaryFolderType);
		break;

	case eMac_SystemFolder:
		path = GetDirectoryPathFromSystemEnum(kSystemFolderType);
		break;
	case eMac_DesktopFolder:
		path = GetDirectoryPathFromSystemEnum(kDesktopFolderType);
		break;
	case eMac_TrashFolder:
		path = GetDirectoryPathFromSystemEnum(kTrashFolderType);
		break;
	case eMac_StartupFolder:
		path = GetDirectoryPathFromSystemEnum(kStartupFolderType);
		break;
	case eMac_ShutdownFolder:
		path = GetDirectoryPathFromSystemEnum(kShutdownFolderType);
		break;
	case eMac_AppleMenuFolder:
		path = GetDirectoryPathFromSystemEnum(kAppleMenuFolderType);
		break;
	case eMac_ControlPanelFolder:
		path = GetDirectoryPathFromSystemEnum(kControlPanelFolderType);
		break;
	case eMac_ExtensionFolder:
		path = GetDirectoryPathFromSystemEnum(kExtensionFolderType);
		break;
	case eMac_FontsFolder:
		path = GetDirectoryPathFromSystemEnum(kFontsFolderType);
		break;
	case eMac_PreferencesFolder:
		path = GetDirectoryPathFromSystemEnum(kPreferencesFolderType);
		break;

	case eJavaBinFolder:
		err = ConvertUnixPathToMacPath("/usr/local/netscape/RequiredGuts/Java/Bin", &path);
		if (err != noErr)
			path = NULL;
		break;

	case eJavaClassesFolder:
		err = ConvertUnixPathToMacPath("/usr/local/netscape/RequiredGuts/Java/Lib", &path);
		if (err != noErr)
			path = NULL;
		break;

	case eJavaDownloadFolder:
		{
			FSSpec spec;
			if ( FindJavaDownloadsFolder(&spec) != noErr )
				return NULL;
			path = CFileMgr::PathNameFromFSSpec(spec, true);
		}
		break;


	// Directories that do not make sense on the Mac
    case eWin_SystemFolder:
    case eWin_WindowsFolder:
    case eUnix_LocalFolder:
    case eUnix_LibFolder:
		path = NULL;
		break;

    case eNetHelpFolder:
        {
            char* tmpdir = FE_GetNetHelpDir();
            path = WH_FileName(tmpdir+7, xpURL);   
            XP_FREEIF(tmpdir);
        }
        break;

    case eOSDriveFolder:
        {
            char *p;
            path = GetDirectoryPathFromSystemEnum(kSystemFolderType);
            /* Drive on mac is :Drive:, so look for second ':' */
            if ( (p = XP_STRCHR( path+1, ':' )) )
            {
                if (p[1])
                    p[1] =  '\0';
            }
        }
        break;

    case eFileURLFolder: // should never get past outer routine
    default:
        XP_ASSERT(false);
        path = NULL;
	}

	if (path)	// Unescape the path, because we'll be passing it to XP_FileOpen as xpURL
	{
		NET_UnEscape(path);
		if (path[XP_STRLEN(path) - 1] != ':')	// Append the ending slash if it is not there
		{
			char * newPath = (char*)XP_ALLOC(XP_STRLEN(path) + 2);
			if (newPath)
			{
				newPath[0] = 0;
				XP_STRCAT(newPath, path);
				XP_STRCAT(newPath, ":");
			}
			XP_FREE(path);
			path = newPath;
		}
	}
	return path;
}


int FE_ExecuteFile( const char * fileName, const char * cmdline )
{
	OSErr err;
	FSSpec appSpec;

    if ( fileName == NULL )
        return -1;

	try
	{
		err = CFileMgr::FSSpecFromLocalUnixPath(fileName, &appSpec, true);
		ThrowIfOSErr_(err);
		LaunchParamBlockRec launchThis;
		
		launchThis.launchAppSpec = (FSSpecPtr)&appSpec;
		launchThis.launchAppParameters = NULL;
		/* launch the thing */
		launchThis.launchBlockID = extendedBlock;
		launchThis.launchEPBLength = extendedBlockLen;
		launchThis.launchFileFlags = NULL;
		launchThis.launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;
		if (!IsFrontApplication())
			launchThis.launchControlFlags += launchDontSwitch;
		err = LaunchApplication(&launchThis);
		ThrowIfOSErr_(err);
	}
	catch (OSErr err)
	{
		return -1;
	}
	return 0;
}

/* Mac technique for replacing the file 'in use'
 * Move the file in use to Trash, and copy the new one
 */
int FE_ReplaceExistingFile(char *from, XP_FileType ftype, 
		char *to, XP_FileType totype,  
		XP_Bool /* force */)	/* We ignore force because we do not check versions */
{
    int result;
	OSErr err;
	FSSpec fileSpec;
	short vRefNum;
	long dirID;

	result = XP_FileRemove( to, totype );
	if ( 0 == result )
	{
   		result = XP_FileRename(from, ftype, to, totype);
	}
    else {
    	err = CFileMgr::FSSpecFromLocalUnixPath(to, &fileSpec, true);
    	if (err != noErr)
		    return -1;
	    err = FindFolder(fileSpec.vRefNum, kTrashFolderType, true, &vRefNum, &dirID);
	    if (err != noErr)
    		return -1;
    	CMovePBRec pb;
    	pb.ioCompletion = NULL;
    	pb.ioNamePtr = (StringPtr)&fileSpec.name;
    	pb.ioDirID = fileSpec.parID;
    	pb.ioVRefNum = vRefNum;
    	pb.ioNewName = NULL;
    	pb.ioNewDirID = dirID;
    
    	err = PBCatMoveSync(&pb);
    	if ( err != noErr)
		    return -1;
	    result = XP_FileRename(from, ftype, to, totype);
    }
	return result;
}


