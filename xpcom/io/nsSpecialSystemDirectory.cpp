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
#include "nsDebug.h"

#ifdef XP_MAC
#include <Folders.h>
#elif defined(XP_PC)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#endif


//nsSpecialSystemDirectory::nsSpecialSystemDirectory()
//:    nsFileSpec(nsnull)
//{
//}

nsSpecialSystemDirectory::nsSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory)
:    nsFileSpec(nsnull)
{
    *this = aSystemSystemDirectory;
}

nsSpecialSystemDirectory::~nsSpecialSystemDirectory()
{
}

void nsSpecialSystemDirectory::operator = (SystemDirectories aSystemSystemDirectory)
{
    *this = nsnull;
    switch (aSystemSystemDirectory)
    {
        
        case OS_DriveDirectory:
#ifdef XP_PC
        {
            char path[_MAX_PATH];
            PRInt32 len = GetWindowsDirectory( path, _MAX_PATH );
            if (len)
            {
                if ( path[1] == ':' && path[2] == '\\' )
                    path[3] = 0;
            }
            *this = path;
        }
#elif defined(XP_MAC)
        {
            *this = kVolumeRootFolderType;
        }
#else
        *this = "/";
#endif
        break;

            
        case OS_TemporaryDirectory:
#ifdef XP_PC
            char path[_MAX_PATH];
            if ( GetEnvironmentVariable(TEXT("TMP"), path, _MAX_PATH) == 0 ) 
                if (GetEnvironmentVariable(TEXT("TEMP"), path, _MAX_PATH))
                {
                    // still not set!
                    PRInt32 len = GetWindowsDirectory( path, _MAX_PATH );
                    if (len)
                    {
                        strcat( path, "temp" );
                    }
                }

            strcat( path, "\\" );
            *this = path;
#elif defined(XP_MAC)
            *this = kTemporaryFolderType;
        
#elif defined(XP_UNIX)
            *this = "/tmp/";
#endif
        break;

        case OS_CurrentProcessDirectory:
            NS_NOTYETIMPLEMENTED("Please do so");
            break;

#ifdef XP_MAC
        case Mac_SystemDirectory:
            *this = kSystemFolderType;
            break;

        case Mac_DesktopDirectory:
            *this = kDesktopFolderType;
            break;

        case Mac_TrashDirectory:
            *this = kTrashFolderType;
            break;

        case Mac_StartupDirectory:
            *this = kStartupFolderType;
            break;

        case Mac_ShutdownDirectory:
            *this = kShutdownFolderType;
            break;

        case Mac_AppleMenuDirectory:
            *this = kAppleMenuFolderType;
            break;

        case Mac_ControlPanelDirectory:
            *this = kControlPanelFolderType;
            break;

        case Mac_ExtensionDirectory:
            *this = kExtensionFolderType;
            break;

        case Mac_FontsDirectory:
            *this = kFontsFolderType;
            break;

        case Mac_PreferencesDirectory:
            *this = kPreferencesFolderType;
            break;
#endif
            
#ifdef XP_PC
        case Win_SystemDirectory:
        {    
            char path[_MAX_PATH];
            PRInt32 len = GetSystemDirectory( path, _MAX_PATH );
        
            // Need enough space to add the trailing backslash
            if (len > _MAX_PATH-2)
                break;
            path[len]   = '\\';
            path[len+1] = '\0';

            *this = path;

            break;
        }

        case Win_WindowsDirectory:
        {    
            char path[_MAX_PATH];
            PRInt32 len = GetWindowsDirectory( path, _MAX_PATH );
            
            // Need enough space to add the trailing backslash
            if (len > _MAX_PATH-2)
                break;
            
            path[len]   = '\\';
            path[len+1] = '\0';

            *this = path;
            break;
        }

#endif        

#ifdef XP_UNIX
        case Unix_LocalDirectory:
            *this = "/usr/local/netscape/";
            break;

        case Unix_LibDirectory:
            *this = "/usr/local/lib/netscape/";
            break;
#endif        

        default:
            break;    
    }
}



#ifdef XP_MAC
//----------------------------------------------------------------------------------------
nsSpecialSystemDirectory::nsSpecialSystemDirectory(OSType folderType)
//----------------------------------------------------------------------------------------
{
	*this = folderType;
}

//----------------------------------------------------------------------------------------
void nsSpecialSystemDirectory::operator = (OSType folderType)
//----------------------------------------------------------------------------------------
{
    CInfoPBRec cinfo;
    DirInfo *dipb=(DirInfo *)&cinfo;
    
    // Call FindFolder to fill in the vrefnum and dirid
    mError = NS_FILE_RESULT(
        FindFolder(
            kOnSystemDisk,
            folderType,
            true,
            &dipb->ioVRefNum,
            &dipb->ioDrDirID));
	if (NS_FAILED(mError))
		return;

    mSpec.name[0] = '\0';
    dipb->ioNamePtr = (StringPtr)&mSpec.name;
    dipb->ioFDirIndex = -1;
    
    mError = PBGetCatInfoSync(&cinfo);
        
    if (NS_SUCCEEDED(mError))
    {
        mSpec.vRefNum = dipb->ioVRefNum;
        mSpec.parID = dipb->ioDrParID;
    }
}
#endif // XP_MAC
