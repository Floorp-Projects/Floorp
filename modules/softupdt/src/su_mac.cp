/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*-
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
/* su_mac.cp
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
#include "FSpCompat.h"


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
			path = GetDirectoryPathFromSystemEnum(kSystemFolderType);
			/* Drive on mac is :Drive:, so look for second ':' */
			char* p = XP_STRCHR( path+1, ':' );
			if (p)
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
	OSErr 				err;
	FSSpec 				appSpec;
	char* 				doomedPath;
	LaunchParamBlockRec launchThis;
	
	
    if ( fileName == NULL )
    {
        return -1;
	}
	
	err = CFileMgr::FSSpecFromLocalUnixPath(fileName, &appSpec, true);
	
	if (err == noErr)
	{
		launchThis.launchAppSpec = (FSSpecPtr)&appSpec;
		launchThis.launchAppParameters = NULL;
		/* launch the thing */
		launchThis.launchBlockID = extendedBlock;
		launchThis.launchEPBLength = extendedBlockLen;
		launchThis.launchFileFlags = NULL;
		launchThis.launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;
		
		if (!IsFrontApplication())
		{
			launchThis.launchControlFlags += launchDontSwitch;
		}
		
		err = LaunchApplication(&launchThis);
		
		/* Returns a full pathname to the given file */
		doomedPath = CFileMgr::PathNameFromFSSpec(appSpec, true );    
		    
		if (doomedPath)
		{    
			su_DeleteOldFileLater(doomedPath);
			XP_FREEIF(doomedPath);
		}
	}
	
	
	if (err != noErr)
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
	FSSpec fileSpec;
	short vRefNum;
	long dirID;

	result = XP_FileRemove( to, totype );
	if ( 0 == result )
	{
   		result = XP_FileRename(from, ftype, to, totype);
	}
    else 
    {
		FSSpec 	tempFileSpec; /* Temp Spec for the unique file name */
    	FSSpec 	trashSpec;
    	char*	doomedPath;
    	
    	result = CFileMgr::FSSpecFromLocalUnixPath(to, &fileSpec, true);
    	if (result != noErr)
		    return -1;
		
		
		/* Get the trash DirID and RefNum */    
	    result = FindFolder(fileSpec.vRefNum, kTrashFolderType, true, &vRefNum, &dirID);
	    if (result != noErr)
    		return -1;
    		
    	/* 
    		We need to see if there is a file in the trash that is already named
    	   	what we are planning to move there.  If there is, give us another name
    	   	that will work
    	*/
    		
		trashSpec.vRefNum 	= vRefNum;
		trashSpec.parID 	= dirID;
		trashSpec.name[0]	= 0;
			
    	result = CFileMgr::UniqueFileSpec(trashSpec, fileSpec.name,  tempFileSpec);
		if ( result != noErr )
			return -1;	
    	
    	
    	/* Lets rename the file in-place.  This works on application an open files */
    	
    	result = FSpRename(&fileSpec, tempFileSpec.name);
    	if ( result != noErr)
		    return -1;
		    
		    
		/* now that we renamed it to something that does not exist in the trash, move it there */    
		CMovePBRec pb;
    	pb.ioCompletion = NULL;
    	pb.ioNamePtr = (StringPtr)&tempFileSpec.name;
    	pb.ioDirID = fileSpec.parID;
    	pb.ioVRefNum = vRefNum;
    	pb.ioNewName = NULL;
    	pb.ioNewDirID = dirID;
    
    	result = PBCatMoveSync(&pb);
    	if ( result != noErr)
		    return -1;
		
		
		/* add this file to the registry to be deleted when we restart communicator */
		
		
		result = FSMakeFSSpec(pb.ioVRefNum, pb.ioNewDirID, pb.ioNamePtr, &tempFileSpec);
		if ( result == noErr)
		{
		
			/* Returns a full pathname to the given file */
			doomedPath = CFileMgr::PathNameFromFSSpec(tempFileSpec, true );    
		    
			if (doomedPath)
			{    
				result = su_DeleteOldFileLater(doomedPath);
				XP_FREEIF(doomedPath);
			}
		}

		/* Rename the new file */    
		    
	    result = XP_FileRename(from, ftype, to, totype);
    	if ( result != noErr)
		    return -1;
    
    }
	return result;
}


