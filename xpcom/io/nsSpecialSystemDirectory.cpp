/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include <Files.h>
#include <Memory.h>
#include <Processes.h>
#elif defined(XP_PC)
#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <stdio.h>
#elif defined(XP_UNIX)
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include "prenv.h"
#endif

#include "plstr.h"

#if XP_PC
//----------------------------------------------------------------------------------------
static char* MakeUpperCase(char* aPath)
//----------------------------------------------------------------------------------------
{
  // windows does not care about case.  push to uppercase:
  int length = strlen(aPath);
  for (int i = 0; i < length; i++)
      if (islower(aPath[i]))
        aPath[i] = _toupper(aPath[i]);
    
  return aPath;
}

//----------------------------------------------------------------------------------------
static void GetWindowsFolder(int folder, nsFileSpec& outDirectory)
//----------------------------------------------------------------------------------------
{
    LPMALLOC pMalloc = NULL;
    LPSTR pBuffer = NULL;
    LPITEMIDLIST pItemIDList = NULL;
    int len;
 
    // Get the shell's allocator. 
    if (!SUCCEEDED(SHGetMalloc(&pMalloc))) 
        return;

    // Allocate a buffer
    if ((pBuffer = (LPSTR) pMalloc->Alloc(MAX_PATH + 2)) == NULL) 
        return; 
 
    // Get the PIDL for the folder. 
    if (!SUCCEEDED(SHGetSpecialFolderLocation( 
            NULL, folder, &pItemIDList)))
        goto Clean;
 
    if (!SUCCEEDED(SHGetPathFromIDList(pItemIDList, pBuffer)))
        goto Clean;

    // Append the trailing slash
    len = PL_strlen(pBuffer);
    pBuffer[len]   = '\\';
    pBuffer[len + 1] = '\0';

    // Assign the directory
    outDirectory = MakeUpperCase(pBuffer);

Clean:
    // Clean up. 
    if (pItemIDList)
        pMalloc->Free(pItemIDList); 
    if (pBuffer)
        pMalloc->Free(pBuffer); 
} // GetWindowsFolder
#endif // XP_PC

//----------------------------------------------------------------------------------------
static void GetCurrentWorkingDirectory(nsFileSpec& aFileSpec)
//----------------------------------------------------------------------------------------
{
    aFileSpec = ".";
    return;
} // GetCurrentWorkingDirectory

//----------------------------------------------------------------------------------------
static void GetCurrentProcessDirectory(nsFileSpec& aFileSpec)
//----------------------------------------------------------------------------------------
{
#ifdef XP_PC
    char buf[MAX_PATH];
    if ( ::GetModuleFileName(0, buf, sizeof(buf)) ) {
        // chop of the executable name by finding the rightmost backslash
        char* lastSlash = PL_strrchr(buf, '\\');
        if (lastSlash)
            *(lastSlash + 1) = '\0';

        aFileSpec = buf;
        return;
    }

#elif defined(XP_MAC)
    // get info for the the current process to determine the directory
    // its located in
    OSErr err;
    ProcessSerialNumber psn;
    if (!(err = GetCurrentProcess(&psn)))
    {
        ProcessInfoRec pInfo;
        FSSpec         tempSpec;

        // initialize ProcessInfoRec before calling
        // GetProcessInformation() or die horribly.
        pInfo.processName = nil;
        pInfo.processAppSpec = &tempSpec;
        pInfo.processInfoLength = sizeof(ProcessInfoRec);

        if (!(err = GetProcessInformation(&psn, &pInfo)))
        {
            FSSpec appFSSpec = *(pInfo.processAppSpec);
            long theDirID = appFSSpec.parID;

            Str255 name;
            CInfoPBRec catInfo;
            catInfo.dirInfo.ioCompletion = NULL;
            catInfo.dirInfo.ioNamePtr = (StringPtr)&name;
            catInfo.dirInfo.ioVRefNum = appFSSpec.vRefNum;
            catInfo.dirInfo.ioDrDirID = theDirID;
            catInfo.dirInfo.ioFDirIndex = -1; // -1 = query dir in ioDrDirID

            if (!(err = PBGetCatInfoSync(&catInfo)))
            {
                aFileSpec = nsFileSpec(appFSSpec.vRefNum,
                                       catInfo.dirInfo.ioDrParID,
                                       name);
                return;
            }
        }
    }

#elif defined(XP_UNIX)

    // In the absence of a good way to get the executable directory let
    // us try this for unix:
    //	- if MOZILLA_FIVE_HOME is defined, that is it
    //	- else give the current directory
    char buf[MAXPATHLEN];
    char *moz5 = PR_GetEnv("MOZILLA_FIVE_HOME");
    if (moz5)
    {
        aFileSpec = moz5;
        return;
    }
    else
    {
        static PRBool firstWarning = PR_TRUE;

        if(firstWarning) {
            // Warn that MOZILLA_FIVE_HOME not set, once.
            printf("Warning: MOZILLA_FIVE_HOME not set.\n");
            firstWarning = PR_FALSE;
        }

        // Fall back to current directory.
        if (getcwd(buf, sizeof(buf)))
        {
            aFileSpec = buf;
            return;
        }
    }

#endif

    NS_ERROR("unable to get current process directory");
} // GetCurrentProcessDirectory()

//nsSpecialSystemDirectory::nsSpecialSystemDirectory()
//:    nsFileSpec(nsnull)
//{
//}

//----------------------------------------------------------------------------------------
nsSpecialSystemDirectory::nsSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory)
//----------------------------------------------------------------------------------------
:    nsFileSpec(nsnull)
{
    *this = aSystemSystemDirectory;
}

//----------------------------------------------------------------------------------------
nsSpecialSystemDirectory::~nsSpecialSystemDirectory()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void nsSpecialSystemDirectory::operator = (SystemDirectories aSystemSystemDirectory)
//----------------------------------------------------------------------------------------
{
    *this = (const char*)nsnull;
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
            *this = MakeUpperCase(path);
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
        {
            char path[_MAX_PATH];
            DWORD len = GetTempPath(_MAX_PATH, path);
            *this = MakeUpperCase(path);
        }
#elif defined(XP_MAC)
            *this = kTemporaryFolderType;
        
#elif defined(XP_UNIX)
            *this = "/tmp/";
#endif
        break;

        case OS_CurrentProcessDirectory:
            GetCurrentProcessDirectory(*this);
            break;

        case OS_CurrentWorkingDirectory:
            GetCurrentWorkingDirectory(*this);
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

        case Mac_DocumentsDirectory:
            *this = kDocumentsFolderType;
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

            *this = MakeUpperCase(path);

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

            *this = MakeUpperCase(path);
            break;
        }

        case Win_HomeDirectory:
        {    
            char path[_MAX_PATH];
            if (GetEnvironmentVariable(TEXT("HOMEDRIVE"), path, _MAX_PATH) >= 0)
            {
                char temp[_MAX_PATH];
                if (GetEnvironmentVariable(TEXT("HOMEPATH"), temp, _MAX_PATH) > 0)
                    PL_strcatn(path, _MAX_PATH, temp);

                PRInt32 len = PL_strlen(path);

                // Need enough space to add the trailing backslash
                if (len > _MAX_PATH - 2)
                    break;
            
                path[len]   = '\\';
                path[len+1] = '\0';
            }
            
            *this = MakeUpperCase(path);
            break;
        }
        case Win_Desktop:
        {
            GetWindowsFolder(CSIDL_DESKTOP, *this);
            break;
        }
        case Win_Programs:
        {
            GetWindowsFolder(CSIDL_PROGRAMS, *this);
            break;
        }
        case Win_Controls:
        {
            GetWindowsFolder(CSIDL_CONTROLS, *this);
            break;
        }
        case Win_Printers:
        {
            GetWindowsFolder(CSIDL_PRINTERS, *this);
            break;
        }
        case Win_Personal:
        {
            GetWindowsFolder(CSIDL_PERSONAL, *this);
            break;
        }
        case Win_Favorites:
        {
            GetWindowsFolder(CSIDL_FAVORITES, *this);
            break;
        }
        case Win_Startup:
        {
            GetWindowsFolder(CSIDL_STARTUP, *this);
            break;
        }
        case Win_Recent:
        {
            GetWindowsFolder(CSIDL_RECENT, *this);
            break;
        }
        case Win_Sendto:
        {
            GetWindowsFolder(CSIDL_SENDTO, *this);
            break;
        }
        case Win_Bitbucket:
        {
            GetWindowsFolder(CSIDL_BITBUCKET, *this);
            break;
        }
        case Win_Startmenu:
        {
            GetWindowsFolder(CSIDL_STARTMENU, *this);
            break;
        }
        case Win_Desktopdirectory:
        {
            GetWindowsFolder(CSIDL_DESKTOPDIRECTORY, *this);
            break;
        }
        case Win_Drives:
        {
            GetWindowsFolder(CSIDL_DRIVES, *this);
            break;
        }
        case Win_Network:
        {
            GetWindowsFolder(CSIDL_NETWORK, *this);
            break;
        }
        case Win_Nethood:
        {
            GetWindowsFolder(CSIDL_NETHOOD, *this);
            break;
        }
        case Win_Fonts:
        {
            GetWindowsFolder(CSIDL_FONTS, *this);
            break;
        }
        case Win_Templates:
        {
            GetWindowsFolder(CSIDL_TEMPLATES, *this);
            break;
        }
        case Win_Common_Startmenu:
        {
            GetWindowsFolder(CSIDL_COMMON_STARTMENU, *this);
            break;
        }
        case Win_Common_Programs:
        {
            GetWindowsFolder(CSIDL_COMMON_PROGRAMS, *this);
            break;
        }
        case Win_Common_Startup:
        {
            GetWindowsFolder(CSIDL_COMMON_STARTUP, *this);
            break;
        }
        case Win_Common_Desktopdirectory:
        {
            GetWindowsFolder(CSIDL_COMMON_DESKTOPDIRECTORY, *this);
            break;
        }
        case Win_Appdata:
        {
            GetWindowsFolder(CSIDL_APPDATA, *this);
            break;
        }
        case Win_Printhood:
        {
            GetWindowsFolder(CSIDL_PRINTHOOD, *this);
            break;
        }
#endif  // XP_PC     

#ifdef XP_UNIX
        case Unix_LocalDirectory:
            *this = "/usr/local/netscape/";
            break;

        case Unix_LibDirectory:
            *this = "/usr/local/lib/netscape/";
            break;

        case Unix_HomeDirectory:
            *this = PR_GetEnv("HOME");
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
    for (int attempts = 0; attempts < 2; attempts++)
    {
        mError = NS_FILE_RESULT(
            FindFolder(
                kOnSystemDisk,
                folderType,
                true,
                &dipb->ioVRefNum,
                &dipb->ioDrDirID));
        if (NS_SUCCEEDED(mError))
            break;
        if (attempts > 0)
		    return;
		switch (folderType)
		{
	    case kDocumentsFolderType:
	        // Find folder will find this, as long as it exists.
	        // The "create" parameter, however, is sadly ignored.
	        // How do we internationalize this?
	        *this = kVolumeRootFolderType;
	        *this += "Documents";
	        CreateDirectory();
	        break;
		} // switch
    } // for
    StrFileName filename;
    filename[0] = '\0';
    dipb->ioNamePtr = (StringPtr)&filename;
    dipb->ioFDirIndex = -1;
    
    mError = NS_FILE_RESULT(PBGetCatInfoSync(&cinfo));
    if (NS_SUCCEEDED(mError))
    {
	    mError = NS_FILE_RESULT(FSMakeFSSpec(dipb->ioVRefNum, dipb->ioDrParID, filename, &mSpec));
    }
}
#endif // XP_MAC
