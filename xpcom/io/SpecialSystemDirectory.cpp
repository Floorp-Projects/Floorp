/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@netscape.com>
 *   IBM Corp.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "SpecialSystemDirectory.h"
#include "nsString.h"
#include "nsDependentString.h"


#ifdef XP_MAC

#include <Folders.h>
#include <Files.h>
#include <Memory.h>
#include <Processes.h>
#include <Gestalt.h>
#include "nsIInternetConfigService.h"


#if UNIVERSAL_INTERFACES_VERSION < 0x0340
    enum {
      kSystemDomain                 = -32766, /* Read-only system hierarchy.*/
      kLocalDomain                  = -32765, /* All users of a single machine have access to these resources.*/
      kNetworkDomain                = -32764, /* All users configured to use a common network server has access to these resources.*/
      kUserDomain                   = -32763, /* Read/write. Resources that are private to the user.*/
      kClassicDomain                = -32762, /* Domain referring to the currently configured Classic System Folder*/

      kDomainLibraryFolderType      = FOUR_CHAR_CODE('dlib')
    };
#endif

#elif defined(XP_WIN)

#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#elif defined(XP_OS2)

#define MAX_PATH _MAX_PATH
#define INCL_WINWORKPLACE
#define INCL_DOSMISC
#define INCL_DOSMODULEMGR
#define INCL_DOSPROCESS
#define INCL_WINSHELLDATA
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



#if defined (XP_WIN)
typedef BOOL (WINAPI * GetSpecialPathProc) (HWND hwndOwner, LPSTR lpszPath, int nFolder, BOOL fCreate);
GetSpecialPathProc gGetSpecialPathProc = NULL;
static HINSTANCE gShell32DLLInst = NULL;
#endif
NS_COM void StartupSpecialSystemDirectory()
{
#if defined (XP_WIN)
    /* On windows, the old method to get file locations is incredibly slow.
       As of this writing, 3 calls to GetWindowsFolder accounts for 3% of mozilla
       startup. Replacing these older calls with a single call to SHGetSpecialFolderPath
       effectively removes these calls from the performace radar.  We need to 
       support the older way of file location lookup on systems that do not have
       IE4. (Note: gets the ansi version: SHGetSpecialFolderPathA).
    */ 
    gShell32DLLInst = LoadLibrary("Shell32.dll");
    if(gShell32DLLInst)
    {
        gGetSpecialPathProc  = (GetSpecialPathProc) GetProcAddress(gShell32DLLInst, 
                                                                   "SHGetSpecialFolderPathA");
    }
#endif
}

NS_COM void ShutdownSpecialSystemDirectory()
{
#if defined (XP_WIN)
   if (gShell32DLLInst)
   {
       FreeLibrary(gShell32DLLInst);
       gShell32DLLInst = NULL;
       gGetSpecialPathProc = NULL;
   }
#endif
}

#if defined (XP_WIN)

//----------------------------------------------------------------------------------------
static nsresult GetWindowsFolder(int folder, nsILocalFile** aFile)
//----------------------------------------------------------------------------------------
{
    if (gGetSpecialPathProc) {
        TCHAR path[MAX_PATH];
        HRESULT result = gGetSpecialPathProc(NULL, path, folder, true);
        
        if (!SUCCEEDED(result)) 
            return NS_ERROR_FAILURE;

        // Append the trailing slash
        int len = strlen(path);
        if (len>1 && path[len-1] != '\\') 
        {
            path[len]   = '\\';
            path[len + 1] = '\0';
        }

        return NS_NewNativeLocalFile(nsDependentCString(CharUpper(path)), 
                                     PR_TRUE, 
                                     aFile);
    }

    nsresult rv = NS_ERROR_FAILURE;
    LPMALLOC pMalloc = NULL;
    LPSTR pBuffer = NULL;
    LPITEMIDLIST pItemIDList = NULL;
    int len;
 
    // Get the shell's allocator. 
    if (!SUCCEEDED(SHGetMalloc(&pMalloc))) 
        return NS_ERROR_FAILURE;

    // Allocate a buffer
    if ((pBuffer = (LPSTR) pMalloc->Alloc(MAX_PATH + 2)) == NULL) 
        return NS_ERROR_FAILURE;
 
    // Get the PIDL for the folder. 
    if (!SUCCEEDED(SHGetSpecialFolderLocation( 
            NULL, folder, &pItemIDList)))
        goto Clean;
 
    if (!SUCCEEDED(SHGetPathFromIDList(pItemIDList, pBuffer)))
        goto Clean;

    // Append the trailing slash
    len = strlen(pBuffer);
    pBuffer[len]   = '\\';
    pBuffer[len + 1] = '\0';

    // Assign the directory
    rv = NS_NewNativeLocalFile(nsDependentCString(CharUpper(pBuffer)), 
                               PR_TRUE, 
                               aFile);

Clean:
    // Clean up. 
    if (pItemIDList)
        pMalloc->Free(pItemIDList); 
    if (pBuffer)
        pMalloc->Free(pBuffer); 

	pMalloc->Release();
    
    return rv;
} 

#endif // XP_WIN




nsresult
GetSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory,
                         nsILocalFile** aFile)
{
#ifdef XP_MAC
    OSErr err;
    short vRefNum;
    long dirID;
#endif

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

            return NS_NewNativeLocalFile(nsDependentCString(CharUpper(path)), 
                                         PR_TRUE, 
                                         aFile);
        }
#elif defined(XP_OS2)
        {
            ULONG ulBootDrive = 0;
            char  buffer[] = " :\\OS2\\";
            DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                             &ulBootDrive, sizeof ulBootDrive);
            buffer[0] = 'A' - 1 + ulBootDrive; // duh, 1-based index...

            return NS_NewNativeLocalFile(nsDependentCString(buffer),
                                         PR_TRUE, 
                                         aFile);
        }
#elif defined(XP_MAC)
        {
            return nsIFileFromOSType(kVolumeRootFolderType, aFile);
        }
#else
        return NS_NewNativeLocalFile(nsDependentCString("/"), 
                                     PR_TRUE, 
                                     aFile);

#endif
            
        case OS_TemporaryDirectory:
#if defined (XP_WIN)
        {
            char path[_MAX_PATH];
            DWORD len = GetTempPath(_MAX_PATH, path);
            return NS_NewNativeLocalFile(nsDependentCString(CharUpper(path)), 
                                         PR_TRUE, 
                                         aFile);
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

            return NS_NewNativeLocalFile(nsDependentCString(buffer), 
                                         PR_TRUE, 
                                         aFile);
        } 
#elif defined(XP_MAC)
        return nsIFileFromOSType(kTemporaryFolderType, aFile);

#elif defined(XP_MACOSX)
        {
            return GetOSXFolderType(kUserDomain, kTemporaryFolderType, aFile);
        }

#elif defined(XP_UNIX) || defined(XP_BEOS)
        {
            static const char *tPath = nsnull;
            if (!tPath) {
                tPath = PR_GetEnv("TMPDIR");
                if (!tPath || !*tPath) {
                    tPath = PR_GetEnv("TMP");
                    if (!tPath || !*tPath) {
                        tPath = PR_GetEnv("TEMP");
                        if (!tPath || !*tPath) {
                            tPath = "/tmp/";
                        }
                    }
                }
            }
            return NS_NewNativeLocalFile(nsDependentCString(tPath), 
                                         PR_TRUE, 
                                         aFile);
        }
#else
        break;
#endif

#if defined(XP_MAC)
        case Mac_SystemDirectory:
            return nsIFileFromOSType(kSystemFolderType, aFile);

        case Mac_DesktopDirectory:
            return nsIFileFromOSType(kDesktopFolderType, aFile);

        case Mac_TrashDirectory:
            return nsIFileFromOSType(kTrashFolderType, aFile);

        case Mac_StartupDirectory:
            return nsIFileFromOSType(kStartupFolderType, aFile);

        case Mac_ShutdownDirectory:
            return nsIFileFromOSType(kShutdownFolderType, aFile);

        case Mac_AppleMenuDirectory:
            return nsIFileFromOSType(kAppleMenuFolderType, aFile);

        case Mac_ControlPanelDirectory:
            return nsIFileFromOSType(kControlPanelFolderType, aFile);

        case Mac_ExtensionDirectory:
            return nsIFileFromOSType(kExtensionFolderType, aFile);

        case Mac_FontsDirectory:
            return nsIFileFromOSType(kFontsFolderType, aFile);

        case Mac_ClassicPreferencesDirectory:
        {
            // whether Mac OS X or pre-Mac OS X, return Classic's Prefs folder
            short domain;
            long response;
            err = ::Gestalt(gestaltSystemVersion, &response);
            domain = (!err && response >= 0x00001000) ? kClassicDomain : kOnSystemDisk;
            err = ::FindFolder(domain, kPreferencesFolderType, true, &vRefNum, &dirID);
            if (!err) {
                err = ::FSMakeFSSpec(vRefNum, dirID, "\p", &mSpec);
            }
            return NS_FILE_RESULT(err);
        }

        case Mac_PreferencesDirectory:
        {
            // if Mac OS X, return Mac OS X's Prefs folder
            // if pre-Mac OS X, return Mac OS's Prefs folder
            err = ::FindFolder(kOnSystemDisk, kPreferencesFolderType, true, &vRefNum, &dirID);
            if (!err) {
                err = ::FSMakeFSSpec(vRefNum, dirID, "\p", &mSpec);
            }
            return NS_FILE_RESULT(err);
        }

        case Mac_DocumentsDirectory:
            return nsIFileFromOSType(kDocumentsFolderType, aFile);

        case Mac_InternetSearchDirectory:
            return nsIFileFromOSType(kInternetSearchSitesFolderType, aFile);

        case Mac_DefaultDownloadDirectory:
            return nsIFileFromOSType(kDefaultDownloadFolderType, aFile);
            
        case Mac_UserLibDirectory:
        {
            FSSpec spec;
            err = ::FindFolder(kUserDomain, kDomainLibraryFolderType, true, &vRefNum, &dirID);
            if (!err) {
                err = ::FSMakeFSSpec(vRefNum, dirID, "\p", &spec);
            }

            return NS_NewLocalFileWithFSSpec(&spec, PR_FALUE, aFile);
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

            return NS_NewNativeLocalFile(nsDependentCString(CharUpper(path)), 
                                         PR_TRUE, 
                                         aFile);
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

            return NS_NewNativeLocalFile(nsDependentCString(CharUpper(path)), 
                                         PR_TRUE, 
                                         aFile);
        }

        case Win_HomeDirectory:
        {    
            char path[_MAX_PATH];
            if (GetEnvironmentVariable(TEXT("HOME"), path, _MAX_PATH) > 0)
            {
                PRInt32 len = strlen(path);
                // Need enough space to add the trailing backslash
                if (len > _MAX_PATH - 2)
                    break;
               
                path[len]   = '\\';
                path[len+1] = '\0';

                return NS_NewNativeLocalFile(nsDependentCString(CharUpper(path)), 
                                             PR_TRUE, 
                                             aFile);
            }

            if (GetEnvironmentVariable(TEXT("HOMEDRIVE"), path, _MAX_PATH) > 0)
            {
                char temp[_MAX_PATH];
                if (GetEnvironmentVariable(TEXT("HOMEPATH"), temp, _MAX_PATH) > 0)
                   strncat(path, temp, _MAX_PATH);
        
                PRInt32 len = strlen(path);

                // Need enough space to add the trailing backslash
                if (len > _MAX_PATH - 2)
                    break;
            
                path[len]   = '\\';
                path[len+1] = '\0';
                
                return NS_NewNativeLocalFile(nsDependentCString(CharUpper(path)), 
                                             PR_TRUE, 
                                             aFile);
            }
        }
        case Win_Desktop:
        {
            return GetWindowsFolder(CSIDL_DESKTOP, aFile);
        }
        case Win_Programs:
        {
            return GetWindowsFolder(CSIDL_PROGRAMS, aFile);
        }
        case Win_Controls:
        {
            return GetWindowsFolder(CSIDL_CONTROLS, aFile);
        }
        case Win_Printers:
        {
            return GetWindowsFolder(CSIDL_PRINTERS, aFile);
        }
        case Win_Personal:
        {
            return GetWindowsFolder(CSIDL_PERSONAL, aFile);
        }
        case Win_Favorites:
        {
            return GetWindowsFolder(CSIDL_FAVORITES, aFile);
        }
        case Win_Startup:
        {
            return GetWindowsFolder(CSIDL_STARTUP, aFile);
        }
        case Win_Recent:
        {
            return GetWindowsFolder(CSIDL_RECENT, aFile);
        }
        case Win_Sendto:
        {
            return GetWindowsFolder(CSIDL_SENDTO, aFile);
        }
        case Win_Bitbucket:
        {
            return GetWindowsFolder(CSIDL_BITBUCKET, aFile);
        }
        case Win_Startmenu:
        {
            return GetWindowsFolder(CSIDL_STARTMENU, aFile);
        }
        case Win_Desktopdirectory:
        {
            return GetWindowsFolder(CSIDL_DESKTOPDIRECTORY, aFile);
        }
        case Win_Drives:
        {
            return GetWindowsFolder(CSIDL_DRIVES, aFile);
        }
        case Win_Network:
        {
            return GetWindowsFolder(CSIDL_NETWORK, aFile);
        }
        case Win_Nethood:
        {
            return GetWindowsFolder(CSIDL_NETHOOD, aFile);
        }
        case Win_Fonts:
        {
            return GetWindowsFolder(CSIDL_FONTS, aFile);
        }
        case Win_Templates:
        {
            return GetWindowsFolder(CSIDL_TEMPLATES, aFile);
        }
        case Win_Common_Startmenu:
        {
            return GetWindowsFolder(CSIDL_COMMON_STARTMENU, aFile);
        }
        case Win_Common_Programs:
        {
            return GetWindowsFolder(CSIDL_COMMON_PROGRAMS, aFile);
        }
        case Win_Common_Startup:
        {
            return GetWindowsFolder(CSIDL_COMMON_STARTUP, aFile);
        }
        case Win_Common_Desktopdirectory:
        {
            return GetWindowsFolder(CSIDL_COMMON_DESKTOPDIRECTORY, aFile);
        }
        case Win_Appdata:
        {
            return GetWindowsFolder(CSIDL_APPDATA, aFile);
        }
        case Win_Printhood:
        {
            return GetWindowsFolder(CSIDL_PRINTHOOD, aFile);
        }
        case Win_Cookies:
        {
            return GetWindowsFolder(CSIDL_COOKIES, aFile);
        }
#endif  // XP_WIN

#if defined(XP_UNIX)
        case Unix_LocalDirectory:
            return NS_NewNativeLocalFile(nsDependentCString("/usr/local/netscape/"), 
                                         PR_TRUE, 
                                         aFile);
        case Unix_LibDirectory:
            return NS_NewNativeLocalFile(nsDependentCString("/usr/local/lib/netscape/"), 
                                         PR_TRUE, 
                                         aFile);

        case Unix_HomeDirectory:
#ifdef VMS
        {
            char *pHome;
            pHome = getenv("HOME");
            if (*pHome == '/') {
                return NS_NewNativeLocalFile(nsDependentCString(pHome), 
                                             PR_TRUE, 
                                             aFile);

            }
            else
            {
                return NS_NewNativeLocalFile(nsDependentCString(decc$translate_vms(pHome)), 
                                             PR_TRUE, 
                                             aFile);
            } 
        }
#else
            return NS_NewNativeLocalFile(nsDependentCString(PR_GetEnv("HOME")), 
                                             PR_TRUE, 
                                             aFile);

#endif

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
            return NS_NewNativeLocalFile(nsDependentCString(path), 
                                         PR_TRUE, 
                                         aFile);
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

            return NS_NewNativeLocalFile(nsDependentCString(path), 
                                         PR_TRUE, 
                                         aFile);
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

            return NS_NewNativeLocalFile(nsDependentCString(path), 
                                         PR_TRUE, 
                                         aFile);
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

            return NS_NewNativeLocalFile(nsDependentCString(path), 
                                         PR_TRUE, 
                                         aFile);
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

            return NS_NewNativeLocalFile(nsDependentCString(buffer), 
                                         PR_TRUE, 
                                         aFile);
        }

     case OS2_OS2Directory:
        {
            ULONG ulBootDrive = 0;
            char  buffer[] = " :\\OS2\\";
            DosQuerySysInfo( QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                             &ulBootDrive, sizeof ulBootDrive);
            buffer[0] = 'A' - 1 + ulBootDrive; // duh, 1-based index...

            return NS_NewNativeLocalFile(nsDependentCString(buffer), 
                                         PR_TRUE, 
                                         aFile);
        }

     case OS2_HomeDirectory:
        {
            nsresult rv;
            char *tPath = PR_GetEnv("MOZILLA_HOME");
            char buffer[CCHMAXPATH];
            /* If MOZILLA_HOME is not set, use GetCurrentProcessDirectory */
            /* To ensure we get a long filename system */
            if (!tPath || !*tPath) {
                PPIB ppib;
                PTIB ptib;
                DosGetInfoBlocks( &ptib, &ppib);
                DosQueryModuleName( ppib->pib_hmte, CCHMAXPATH, buffer);
                *strrchr( buffer, '\\') = '\0'; // XXX DBCS misery
                tPath = buffer;
            }
            rv = NS_NewNativeLocalFile(nsDependentCString(tPath),
                                       PR_TRUE, 
                                       aFile);

            PrfWriteProfileString(HINI_USERPROFILE, "Mozilla", "Home", tPath);
            return rv;
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

            return NS_NewNativeLocalFile(nsDependentCString(szPath),
                                         PR_TRUE, 
                                         aFile);
        }
#endif
        default:
            break;
    }
    return NS_ERROR_NOT_AVAILABLE;
}

#if defined (XP_MACOSX)
nsresult
GetOSXFolderType(short aDomain, OSType aFolderType, nsILocalFile **localFile)
{
    OSErr err;
    FSRef fsRef;
    nsresult rv = NS_ERROR_FAILURE;

    err = ::FSFindFolder(aDomain, aFolderType, kCreateFolder, &fsRef);
    if (err == noErr)
    {
        NS_NewLocalFile(EmptyString(), PR_TRUE, localFile);
        nsCOMPtr<nsILocalFileMac> localMacFile(do_QueryInterface(*localFile));
        if (localMacFile)
            rv = localMacFile->InitWithFSRef(&fsRef);
    }
    return rv;
}                                                                      
#endif

