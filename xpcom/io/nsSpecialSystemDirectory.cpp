/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Doug Turner <dougt@netscape.com>
 */

#include "nsSpecialSystemDirectory.h"


#ifdef XP_MAC
#include <Folders.h>
#elif defined XP_PC
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#endif


nsSpecialSystemDirectory::nsSpecialSystemDirectory()
:    nsFileSpec(nsnull)
{
}

nsSpecialSystemDirectory::nsSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory)
:    nsFileSpec(nsnull)
{
    SetSpecialSystemDirectory(aSystemSystemDirectory, this);
}

nsSpecialSystemDirectory::~nsSpecialSystemDirectory()
{
}


void nsSpecialSystemDirectory::operator = (SystemDirectories aSystemSystemDirectory)
{
    SetSpecialSystemDirectory(aSystemSystemDirectory, this);
}


void
nsSpecialSystemDirectory::SetSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory, nsFileSpec* fileSpec)
{

    
#ifdef XP_MAC
    OSErr  osErr;
    FSSpec aMacSpec;
#elif defined XP_PC
    char path[_MAX_PATH];
    PRInt32 len;
#endif

    *fileSpec  = nsnull;


    switch (aSystemSystemDirectory)
    {
        
        case OS_DriveDirectory:
#ifdef XP_PC
        {
            len = GetWindowsDirectory( path, _MAX_PATH );
            if (len)
            {
                if ( path[1] == ':' && path[2] == '\\' )
                {
                    path[3] = 0;
                    *fileSpec = path;
                }
            }
        }
#elif defined XP_MAC
        {
            if (GetFSSpecFromSystemEnum(kVolumeRootFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
        }
#else
        *fileSpec = "/";
#endif
        break;

            
        case OS_TemporaryDirectory:
#ifdef XP_PC

            if ( GetEnvironmentVariable(TEXT("TMP"),path,_MAX_PATH)==0) 
                if (GetEnvironmentVariable(TEXT("TEMP"),path,_MAX_PATH))
                {
                    // still not set!
                    len = GetWindowsDirectory( path, _MAX_PATH );
                    if (len)
                    {
                        strcat( path, "temp" );
                    }
                }

            strcat( path, "\\" );
           *fileSpec = path;
#elif defined XP_MAC
            if (GetFSSpecFromSystemEnum(kTemporaryFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
        
#elif defined XP_UNIX
            *fileSpec = "/tmp/";
#endif
        break;

#ifdef XP_MAC
        case Mac_SystemDirectory:
            if (GetFSSpecFromSystemEnum(kSystemFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
            break;

        case Mac_DesktopDirectory:
            if (GetFSSpecFromSystemEnum(kDesktopFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
            break;

        case Mac_TrashDirectory:
            if (GetFSSpecFromSystemEnum(kTrashFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
            break;

        case Mac_StartupDirectory:
            if (GetFSSpecFromSystemEnum(kStartupFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
            break;

        case Mac_ShutdownDirectory:
            if (GetFSSpecFromSystemEnum(kShutdownFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
            break;

        case Mac_AppleMenuDirectory:
            if (GetFSSpecFromSystemEnum(kAppleMenuFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec
            break;

        case Mac_ControlPanelDirectory:
            if (GetFSSpecFromSystemEnum(kControlPanelFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
            break;

        case Mac_ExtensionDirectory:
            if (GetFSSpecFromSystemEnum(kExtensionFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
            break;

        case Mac_FontsDirectory:
            if (GetFSSpecFromSystemEnum(kFontsFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
            break;

        case Mac_PreferencesDirectory:
            if (GetFSSpecFromSystemEnum(kPreferencesFolderType, aMacSpec) == noErr)
                *fileSpec = aMacSpec;
            break;
#endif
            
#ifdef XP_PC
        case Win_SystemDirectory:
        {    
            len = GetSystemDirectory( path, _MAX_PATH );
        
            // Need enough space to add the trailing backslash
            if(len > _MAX_PATH-2)
                break;
            path[len]   = '\\';
            path[len+1] = '\0';

            *fileSpec = path;

            break;
        }

        case Win_WindowsDirectory:
        {    
            len = GetWindowsDirectory( path, _MAX_PATH );
            
            // Need enough space to add the trailing backslash
            if(len > _MAX_PATH-2)
                break;
            
            path[len]   = '\\';
            path[len+1] = '\0';

            *fileSpec = path;
            break;
        }

#endif        

#ifdef XP_UNIX
        case Unix_LocalDirectory:
            *fileSpec = "/usr/local/netscape/";
            break;

        case Unix_LibDirectory:
            *fileSpec = "/usr/local/lib/netscape/";
            break;
#endif        

        default:
            break;    
    }
}



#ifdef XP_MAC

OSErr GetFSSpecFromSystemEnum(OSType folderType, FSSpec* folderSpec)
{
#if WHEN_MCMULLEN_REVIEWS
    OSErr err;
	short vRefNum;
	long dirID;
    

    err = FindFolder(kOnSystemDisk,folderType,true,&vRefNum,&dirID);
	if (err != noErr)
		return err;

    folderSpec.vRefNum = 0; // Initialize them to 0
    folderSpec.parID = 0;
    
    CInfoPBRec cinfo;
    DirInfo *dipb=(DirInfo *)&cinfo;
    
    dipb->ioNamePtr = (StringPtr)&folderSpec.name;
    dipb->ioVRefNum = vRefNum;
    dipb->ioFDirIndex = -1;
    dipb->ioDrDirID = dirID;
    
    err = PBGetCatInfoSync(&cinfo);
        
    if (err == noErr)
    {
        folderSpec.vRefNum = dipb->ioVRefNum;
        folderSpec.parID = dipb->ioDrParID;
    }

    return err;
#endif
}
#endif
