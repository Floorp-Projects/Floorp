/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Doug Turner <dougt@netscape.com>
 *     IBM Corp.
 */

#include "nsSpecialSystemDirectory.h"
#include "nsDebug.h"

#ifdef XP_MAC
#include <Folders.h>
#include <Files.h>
#include <Memory.h>
#include <Processes.h>
#include <Gestalt.h>
#include "nsIInternetConfigService.h"
#elif defined(XP_WIN)
#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <stdio.h>
#elif defined(XP_OS2)
#define MAX_PATH _MAX_PATH
#define INCL_WINWORKPLACE
#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include "prenv.h"
#elif defined(XP_UNIX)
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include "prenv.h"
#elif defined(XP_BEOS)
#include <FindDirectory.h>
#include <Path.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <OS.h>
#include <image.h>
#include "prenv.h"
#endif

#if defined(VMS)
#include <unixlib.h>
#endif

#include "plstr.h"

#include "nsHashtable.h"
#include "prlog.h"

#if defined (XP_MAC) && UNIVERSAL_INTERFACES_VERSION < 0x0340
    enum {
      kSystemDomain                 = -32766, /* Read-only system hierarchy.*/
      kLocalDomain                  = -32765, /* All users of a single machine have access to these resources.*/
      kNetworkDomain                = -32764, /* All users configured to use a common network server has access to these resources.*/
      kUserDomain                   = -32763, /* Read/write. Resources that are private to the user.*/
      kClassicDomain                = -32762, /* Domain referring to the currently configured Classic System Folder*/

      kDomainLibraryFolderType      = FOUR_CHAR_CODE('dlib')
    };
#endif


class SystemDirectoriesKey : public nsHashKey {
public:

    SystemDirectoriesKey(nsSpecialSystemDirectory::SystemDirectories newKey) : sdKey(newKey) {}

    virtual PRUint32 HashCode(void) const
    {
        return PRUint32(sdKey);
    }
    
    virtual PRBool Equals(const nsHashKey *aKey) const
    {
        nsSpecialSystemDirectory::SystemDirectories other = 
            ((SystemDirectoriesKey*)aKey)->sdKey;
        return other == sdKey;
    }
    
    virtual nsHashKey *Clone(void) const
    {
        return new SystemDirectoriesKey(sdKey);
    }

private:
    nsSpecialSystemDirectory::SystemDirectories sdKey; // sd for SystemDirectories
};

PR_STATIC_CALLBACK(PRBool) DeleteSystemDirKeys(nsHashKey *aKey, void *aData, void* closure)
{
    delete ((nsFileSpec *)aData);
    return PR_TRUE;
}

#define NS_SYSTEMDIR_HASH_NUM (10)
static nsHashtable *systemDirectoriesLocations = NULL;

NS_COM void StartupSpecialSystemDirectory(){}

NS_COM void ShutdownSpecialSystemDirectory()
{
    if (systemDirectoriesLocations)
    {
        systemDirectoriesLocations->Reset(DeleteSystemDirKeys);
        delete systemDirectoriesLocations;
    }
}

#if defined (XP_WIN)

static PRBool gGlobalOSInitialized = PR_FALSE;
static PRBool gGlobalDBCSEnabledOS = PR_FALSE;

//----------------------------------------------------------------------------------------
static char* MakeUpperCase(char* aPath)
//----------------------------------------------------------------------------------------
{
  // check if the Windows is DBCSEnabled once.
  if (PR_FALSE == gGlobalOSInitialized) {
    if (GetSystemMetrics(SM_DBCSENABLED))
      gGlobalDBCSEnabledOS = PR_TRUE;
    gGlobalOSInitialized = PR_TRUE;
  }

  // windows does not care about case.  pu sh to uppercase:
  int length = strlen(aPath);
  int i = 0; /* C++ portability guide #20 */
  if (!gGlobalDBCSEnabledOS)  {
    // for non-DBCS windows
    for (i = 0; i < length; i++)
        if (islower(aPath[i]))
          aPath[i] = _toupper(aPath[i]);
  }
  else {
    // for DBCS windows
    for (i = 0; i < length; i++)  {
      if (IsDBCSLeadByte(aPath[i])) {
        // begining of the double bye char
        i++;
      }
      else  {
        if ( islower(aPath[i]))
          aPath[i] = _toupper(aPath[i]);
      }
    } //end of for loop
  }
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
    outDirectory = pBuffer;

Clean:
    // Clean up. 
    if (pItemIDList)
        pMalloc->Free(pItemIDList); 
    if (pBuffer)
        pMalloc->Free(pBuffer); 

	pMalloc->Release();
} // GetWindowsFolder
#endif // XP_WIN

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
#if defined (XP_WIN)
    char buf[MAX_PATH];
    if ( ::GetModuleFileName(0, buf, sizeof(buf)) ) {
        // chop of the executable name by finding the rightmost backslash
        char* lastSlash = PL_strrchr(buf, '\\');
        if (lastSlash)
            *(lastSlash + 1) = '\0';

        aFileSpec = buf;
        return;
    }

#elif defined(XP_OS2)
    PPIB ppib;
    PTIB ptib;
    char buffer[CCHMAXPATH];
    DosGetInfoBlocks( &ptib, &ppib);
    DosQueryModuleName( ppib->pib_hmte, CCHMAXPATH, buffer);
    *strrchr( buffer, '\\') = '\0'; // XXX DBCS misery
    aFileSpec = buffer;
    return;


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
                                       name,
                                       PR_TRUE);
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

#elif defined(XP_BEOS)

    char *moz5 = getenv("MOZILLA_FIVE_HOME");
    if (moz5)
    {
        aFileSpec = moz5;
        return;
    }
    else
    {
      static char buf[MAXPATHLEN];
      int32 cookie = 0;
      image_info info;
      char *p;
      *buf = 0;
      if(get_next_image_info(0, &cookie, &info) == B_OK)
      {
        strcpy(buf, info.name);
        if((p = strrchr(buf, '/')) != 0)
        {
          *p = 0;
          aFileSpec = buf;
          return;
        }
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
    SystemDirectoriesKey dirKey(aSystemSystemDirectory);
    SystemDirectoriesKey mozBinDirKey(Moz_BinDirectory);

    // This flag is used to tell whether or not we need to append something
    // onto the *this.  Search for needToAppend to how it's used.
    // IT's VERY IMPORTANT that needToAppend is initialized to PR_TRUE.
    PRBool needToAppend = PR_TRUE;

#ifdef XP_MAC
    OSErr err;
    short vRefNum;
    long dirID;
#endif

    *this = (const char*)nsnull;
    switch (aSystemSystemDirectory)
    {
        
        case OS_DriveDirectory:
#if defined (XP_WIN)
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
#elif defined(XP_OS2)
        {
            // printf( "*** Warning warning OS_DriveDirectory called for");
            
            ULONG ulBootDrive = 0;
            char  buffer[] = " :\\OS2\\";
            DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                             &ulBootDrive, sizeof ulBootDrive);
            buffer[0] = 'A' - 1 + ulBootDrive; // duh, 1-based index...
            *this = buffer;
#ifdef DEBUG
            printf( "Got OS_DriveDirectory: %s\n", buffer);
#endif
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
#if defined (XP_WIN)
        {
            char path[_MAX_PATH];
            DWORD len = GetTempPath(_MAX_PATH, path);
            *this = MakeUpperCase(path);
        }
#elif defined(XP_OS2)
          {
             char buffer[CCHMAXPATH] = "";
             char *c = getenv( "TMP");
             if( c) strcpy( buffer, c);
             else
             {
                c = getenv( "TEMP");
                if( c) strcpy( buffer, c);
             }
             if( c) *this = buffer;
             // use exe's directory if not set
             else GetCurrentProcessDirectory(*this);
          }
#elif defined(XP_MAC)
            *this = kTemporaryFolderType;
        
#elif defined(XP_UNIX) || defined(XP_BEOS)
		{
			char *tPath = PR_GetEnv("TMPDIR");
			if (!tPath || PL_strlen(tPath) == 0)
				*this = "/tmp/";
			else
				*this = tPath;
       }
#endif
        break;

        case OS_CurrentProcessDirectory:
            GetCurrentProcessDirectory(*this);
            break;

        case OS_CurrentWorkingDirectory:
            GetCurrentWorkingDirectory(*this);
            break;

        case XPCOM_CurrentProcessComponentRegistry:
            {
                nsFileSpec *dirSpec = NULL;

                // if someone has called nsSpecialSystemDirectory::Set()
                if (systemDirectoriesLocations) {
                    // look for the value for the argument key
                    if (!(dirSpec = (nsFileSpec *)systemDirectoriesLocations->Get(&dirKey))) {
                        // if not found, try Moz_BinDirectory
                        dirSpec = (nsFileSpec *)
                            systemDirectoriesLocations->Get(&mozBinDirKey);
                    }
                    else {
                        // if the value is found for the argument key,
                        // we don't need to append.
                        needToAppend = PR_FALSE;
                    }
                }
                
                if (dirSpec)
                {
                    *this = *dirSpec;
                }
                else
                {
                    GetCurrentProcessDirectory(*this);
                }

                if (needToAppend) {
                    // XXX We need to unify these names across all platforms
#ifdef XP_MAC
                    *this += "Component Registry";
#else
                    *this += "component.reg";
#endif /* XP_MAC */
                }
            }
            break;

        case XPCOM_CurrentProcessComponentDirectory:
            {
                nsFileSpec *dirSpec = NULL;
                // if someone has called nsSpecialSystemDirectory::Set()
                if (systemDirectoriesLocations) {
                    // look for the value for the argument key
                    if (!(dirSpec = (nsFileSpec *)systemDirectoriesLocations->Get(&dirKey))) {
                        // if not found, try Moz_BinDirectory
                        dirSpec = (nsFileSpec *)
                            systemDirectoriesLocations->Get(&mozBinDirKey);
                    }
                    else {
                        // if the value is found for the argument key,
                        // we don't need to append.
                        needToAppend = PR_FALSE;
                    }
                }
                if (dirSpec)
                {
                    *this = *dirSpec;
                }
                else
                {
                    // <exedir>/Components
                    GetCurrentProcessDirectory(*this);
                }

                if (needToAppend) {
                    // XXX We need to unify these names across all platforms
#ifdef XP_MAC
                    *this += "Components";
#else
                    *this += "components";
#endif /* XP_MAC */
                }
            }
            break;

        case Moz_BinDirectory:
            {
                nsFileSpec *dirSpec = NULL;
                // if someone has called nsSpecialSystemDirectory::Set()
                if (systemDirectoriesLocations) {
                    // look for the value for the argument key
                    dirSpec = (nsFileSpec *)
                        systemDirectoriesLocations->Get(&dirKey);
                }
                if (dirSpec) {
                    *this = *dirSpec;
                }
                else {
                    GetCurrentProcessDirectory(*this);
                }
            }
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
        {
            short domain;
            long response;
            err = ::Gestalt(gestaltSystemVersion, &response);
            domain = (!err && response >= 0x00001000) ? kClassicDomain : kOnSystemDisk;
            err = ::FindFolder(domain, kPreferencesFolderType, true, &vRefNum, &dirID);
            if (!err) {
                err = ::FSMakeFSSpec(vRefNum, dirID, "\p", &mSpec);
            }
            mError = NS_FILE_RESULT(err);
            break;
        }

        case Mac_DocumentsDirectory:
            *this = kDocumentsFolderType;
            break;

        case Mac_InternetSearchDirectory:
            *this = kInternetSearchSitesFolderType;
            break;

        case Mac_DefaultDownloadDirectory:
            *this = kDefaultDownloadFolderType;
            break;
            
        case Mac_UserLibDirectory:
        {
            err = ::FindFolder(kUserDomain, kDomainLibraryFolderType, true, &vRefNum, &dirID);
            if (!err) {
                err = ::FSMakeFSSpec(vRefNum, dirID, "\p", &mSpec);
            }
            mError = NS_FILE_RESULT(err);
            break;
        }
#endif
            
#if defined (XP_WIN)
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
            if (GetEnvironmentVariable(TEXT("HOME"), path, _MAX_PATH) > 0)
            {
                PRInt32 len = PL_strlen(path);
                // Need enough space to add the trailing backslash
                if (len > _MAX_PATH - 2)
                    break;
               
                path[len]   = '\\';
                path[len+1] = '\0';
                
                *this = MakeUpperCase(path);
                break;
            }

            if (GetEnvironmentVariable(TEXT("HOMEDRIVE"), path, _MAX_PATH) > 0)
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
                
                *this = MakeUpperCase(path);
                break;
            }
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
#endif  // XP_WIN

#ifdef XP_UNIX
        case Unix_LocalDirectory:
            *this = "/usr/local/netscape/";
            break;

        case Unix_LibDirectory:
            *this = "/usr/local/lib/netscape/";
            break;

        case Unix_HomeDirectory:
#ifdef VMS
	    {
	        char *pHome;
	        pHome = getenv("HOME");
		if (*pHome == '/')
        	    *this = pHome;
		else
        	    *this = decc$translate_vms(pHome);
	    }
#else
            *this = PR_GetEnv("HOME");
#endif
            break;

#endif        

#ifdef XP_BEOS
        case BeOS_SettingsDirectory:
		{
            char path[MAXPATHLEN];
			find_directory(B_USER_SETTINGS_DIRECTORY, 0, 0, path, MAXPATHLEN);
            // Need enough space to add the trailing backslash
			int len = strlen(path);
            if (len > MAXPATHLEN-2)
                break;
            path[len]   = '/';
            path[len+1] = '\0';
			*this = path;
            break;
		}

        case BeOS_HomeDirectory:
		{
            char path[MAXPATHLEN];
			find_directory(B_USER_DIRECTORY, 0, 0, path, MAXPATHLEN);
            // Need enough space to add the trailing backslash
			int len = strlen(path);
            if (len > MAXPATHLEN-2)
                break;
            path[len]   = '/';
            path[len+1] = '\0';
			*this = path;
            break;
		}

        case BeOS_DesktopDirectory:
		{
            char path[MAXPATHLEN];
			find_directory(B_DESKTOP_DIRECTORY, 0, 0, path, MAXPATHLEN);
            // Need enough space to add the trailing backslash
			int len = strlen(path);
            if (len > MAXPATHLEN-2)
                break;
            path[len]   = '/';
            path[len+1] = '\0';
			*this = path;
            break;
		}

        case BeOS_SystemDirectory:
		{
            char path[MAXPATHLEN];
			find_directory(B_BEOS_DIRECTORY, 0, 0, path, MAXPATHLEN);
            // Need enough space to add the trailing backslash
			int len = strlen(path);
            if (len > MAXPATHLEN-2)
                break;
            path[len]   = '/';
            path[len+1] = '\0';
			*this = path;
            break;
		}
#endif        
#ifdef XP_OS2
        case OS2_SystemDirectory:
        {
            ULONG ulBootDrive = 0;
            char  buffer[] = " :\\OS2\\System\\";
            DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                             &ulBootDrive, sizeof ulBootDrive);
            buffer[0] = 'A' - 1 + ulBootDrive; // duh, 1-based index...
            *this = buffer;
#ifdef DEBUG
            printf( "Got OS2_SystemDirectory: %s\n", buffer);
#endif
            break;
        }

     case OS2_OS2Directory:
        {
            ULONG ulBootDrive = 0;
            char  buffer[] = " :\\OS2\\";
            DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                             &ulBootDrive, sizeof ulBootDrive);
            buffer[0] = 'A' - 1 + ulBootDrive; // duh, 1-based index...
            *this = buffer;
#ifdef DEBUG
            printf( "Got OS2_OS2Directory: %s\n", buffer);
#endif
            break;
        }

     case OS2_HomeDirectory:
        {
            char *tPath = PR_GetEnv("MOZILLA_HOME");
            /* If MOZILLA_HOME is not set, use GetCurrentProcessDirectory */
            /* To ensure we get a long filename system */
            if (!tPath || PL_strlen(tPath) == 0)
              GetCurrentProcessDirectory(*this);
            else
              *this = tPath;
            break;
        }

        case OS2_DesktopDirectory:
        {
            char szPath[CCHMAXPATH + 1];        
            BOOL fSuccess;
            fSuccess = WinQueryActiveDesktopPathname (szPath, sizeof(szPath));
            int len = strlen (szPath);   
            if (len > CCHMAXPATH -1)
               break;
            szPath[len] = '\\';     
            szPath[len + 1] = '\0';
#ifdef DEBUG
            if (fSuccess) {
               printf ("Got OS2_DesktopDirectory: %s\n", szPath);
            } else {
               printf ("Failed getting OS2_DesktopDirectory: %s\n", szPath);
            }
#endif
            break;           
        }

#endif
        default:
            break;    
    }
}

void
nsSpecialSystemDirectory::Set(SystemDirectories dirToSet, nsFileSpec *dirSpec)
{
    SystemDirectoriesKey dirKey(dirToSet);
    
    PR_ASSERT(NULL != dirSpec);
    
    if (NULL == systemDirectoriesLocations) {
        systemDirectoriesLocations = new nsHashtable(NS_SYSTEMDIR_HASH_NUM);
    }
    PR_ASSERT(NULL != systemDirectoriesLocations);
    
    nsFileSpec *newSpec = new nsFileSpec(*dirSpec);
    if (NULL != newSpec) {
        systemDirectoriesLocations->Put(&dirKey, newSpec);
    }
    
    return;
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
    
    // hack: first check for kDefaultDownloadFolderType
    // if it is, get Internet Config Download folder, if that's
    // not availble use desktop folder
    if (folderType == kDefaultDownloadFolderType)
    {
      nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
      if (icService)
      {
        if (NS_SUCCEEDED(icService->GetDownloadFolder(&mSpec)))
          return;
        else
          folderType = kDesktopFolderType;
      }
      else
      {
        folderType = kDesktopFolderType;
      }
    }
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
