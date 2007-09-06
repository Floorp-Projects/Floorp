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
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
 *   Jungshik Shin <jshin@i18nl10n.com>
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

#if defined(XP_WIN)

#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>

// These are not defined by VC6. 
#ifndef CSIDL_LOCAL_APPDATA
#define CSIDL_LOCAL_APPDATA             0x001C
#endif
#ifndef CSIDL_PROGRAM_FILES
#define CSIDL_PROGRAM_FILES             0x0026
#endif

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
#include <fs_info.h>
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

#ifndef MAXPATHLEN
#ifdef MAX_PATH
#define MAXPATHLEN MAX_PATH
#elif defined(_MAX_PATH)
#define MAXPATHLEN _MAX_PATH
#elif defined(CCHMAXPATH)
#define MAXPATHLEN CCHMAXPATH
#else
#define MAXPATHLEN 1024
#endif
#endif

#ifdef XP_WIN
typedef HRESULT (WINAPI* nsGetKnownFolderPath)(GUID& rfid,
                                               DWORD dwFlags,
                                               HANDLE hToken,
                                               PWSTR *ppszPath);

static nsGetKnownFolderPath gGetKnownFolderPath = NULL;

static HINSTANCE gShell32DLLInst = NULL;
#endif

NS_COM void StartupSpecialSystemDirectory()
{
#if defined (XP_WIN) && !defined (WINCE)
    // SHGetKnownFolderPath is only available on Windows Vista
    // so that we need to use GetProcAddress to get the pointer.
    gShell32DLLInst = LoadLibrary("Shell32.dll");
    if(gShell32DLLInst)
    {
        gGetKnownFolderPath = (nsGetKnownFolderPath) 
            GetProcAddress(gShell32DLLInst, "SHGetKnownFolderPath");
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
        gGetKnownFolderPath = NULL;
    }
#endif
}

#if defined (XP_WIN)

static nsresult GetKnownFolder(GUID* guid, nsILocalFile** aFile)
{
    if (!guid || !gGetKnownFolderPath)
        return NS_ERROR_FAILURE;

    PWSTR path = NULL;
    gGetKnownFolderPath(*guid, 0, NULL, &path);

    if (!path)
        return NS_ERROR_FAILURE;

    nsresult rv = NS_NewLocalFile(nsDependentString(path), 
                                  PR_TRUE, 
                                  aFile);

    CoTaskMemFree(path);
    return rv;
}

//----------------------------------------------------------------------------------------
static nsresult GetWindowsFolder(int folder, nsILocalFile** aFile)
//----------------------------------------------------------------------------------------
{
    WCHAR path[MAX_PATH + 2];
    HRESULT result = ::SHGetSpecialFolderPathW(NULL, path, folder, true);
    
    if (!SUCCEEDED(result)) 
        return NS_ERROR_FAILURE;

    // Append the trailing slash
    int len = wcslen(path);
    if (len > 1 && path[len - 1] != L'\\') 
    {
        path[len]   = L'\\';
        path[++len] = L'\0';
    }

    return NS_NewLocalFile(nsDependentString(path, len), PR_TRUE, aFile);
} 

#endif // XP_WIN

#if defined (XP_BEOS)                                            
static nsresult
GetBeOSFolder( directory_which which, dev_t volume, nsILocalFile** aFile)
{
    char path[MAXPATHLEN];
    if (volume < 0)
        return NS_ERROR_FAILURE;
        
    status_t result = find_directory(which, volume, false, path, MAXPATHLEN - 2);
    if (result != B_OK)
        return NS_ERROR_FAILURE;
        
    int len = strlen(path);
    if (len == 0)
        return NS_ERROR_FAILURE;

    if (path[len-1] != '/') 
    {
        path[len]   = '/';
        path[len+1] = '\0';            
    }
    return NS_NewNativeLocalFile(nsDependentCString(path), PR_TRUE, aFile);
}
#endif // XP_BEOS

#if defined(XP_UNIX)
static nsresult
GetUnixHomeDir(nsILocalFile** aFile)
{
#ifdef VMS
    char *pHome;
    pHome = getenv("HOME");
    if (*pHome == '/') {
        return NS_NewNativeLocalFile(nsDependentCString(pHome), 
                                     PR_TRUE, 
                                     aFile);
    } else {
        return NS_NewNativeLocalFile(nsDependentCString(decc$translate_vms(pHome)), 
                                     PR_TRUE, 
                                     aFile);
    }
#else
    return NS_NewNativeLocalFile(nsDependentCString(PR_GetEnv("HOME")), 
                                 PR_TRUE, aFile);
#endif
}
#endif

nsresult
GetSpecialSystemDirectory(SystemDirectories aSystemSystemDirectory,
                          nsILocalFile** aFile)
{
#if defined(XP_WIN)
    WCHAR path[MAX_PATH];
#else
    char path[MAXPATHLEN];
#endif

    switch (aSystemSystemDirectory)
    {
        case OS_CurrentWorkingDirectory:
#if defined(XP_WIN)
            if (!_wgetcwd(path, MAX_PATH))
                return NS_ERROR_FAILURE;
            return NS_NewLocalFile(nsDependentString(path), 
                                   PR_TRUE, 
                                   aFile);
#elif defined(XP_OS2)
            if (DosQueryPathInfo( ".", FIL_QUERYFULLNAME, path, MAXPATHLEN))
                return NS_ERROR_FAILURE;
#else
            if(!getcwd(path, MAXPATHLEN))
                return NS_ERROR_FAILURE;
#endif

#if !defined(XP_WIN)
            return NS_NewNativeLocalFile(nsDependentCString(path), 
                                         PR_TRUE, 
                                         aFile);
#endif

        case OS_DriveDirectory:
#if defined (XP_WIN)
        {
            PRInt32 len = ::GetWindowsDirectoryW(path, MAX_PATH);
            if (len == 0)
                break;
            if (path[1] == PRUnichar(':') && path[2] == PRUnichar('\\'))
                path[3] = 0;

            return NS_NewLocalFile(nsDependentString(path),
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
#else
        return NS_NewNativeLocalFile(nsDependentCString("/"), 
                                     PR_TRUE, 
                                     aFile);

#endif
            
        case OS_TemporaryDirectory:
#if defined (XP_WIN) && !defined (WINCE)
        {
            DWORD len = ::GetTempPathW(MAX_PATH, path);
            if (len == 0)
                break;
            return NS_NewLocalFile(nsDependentString(path, len), 
                                   PR_TRUE, 
                                   aFile);
        }
#elif defined (WINCE)
        {
            return NS_NewNativeLocalFile(NS_LITERAL_CSTRING("\\Temp"), 
                                         PR_TRUE, 
                                         aFile);
        }
#elif defined(XP_OS2)
        {
            char *tPath = PR_GetEnv("TMP");
            if (!tPath || !*tPath) {
                tPath = PR_GetEnv("TEMP");
                if (!tPath || !*tPath) {
                    // if an OS/2 system has neither TMP nor TEMP defined
                    // then it is severely broken, so this will never happen.
                    return NS_ERROR_UNEXPECTED;
                }
            }
            nsCString tString = nsDependentCString(tPath);
            if (tString.Find("/", PR_FALSE, 0, -1)) {
                tString.ReplaceChar('/','\\');
            }
            return NS_NewNativeLocalFile(tString, PR_TRUE, aFile);
        }
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
#if defined (XP_WIN)
        case Win_SystemDirectory:
        {    
            PRInt32 len = ::GetSystemDirectoryW(path, MAX_PATH);
        
            // Need enough space to add the trailing backslash
            if (!len || len > MAX_PATH - 2)
                break;
            path[len]   = L'\\';
            path[++len] = L'\0';

            return NS_NewLocalFile(nsDependentString(path, len), 
                                   PR_TRUE, 
                                   aFile);
        }

        case Win_WindowsDirectory:
        {    
            PRInt32 len = ::GetWindowsDirectoryW(path, MAX_PATH);
            
            // Need enough space to add the trailing backslash
            if (!len || len > MAX_PATH - 2)
                break;
            
            path[len]   = L'\\';
            path[++len] = L'\0';

            return NS_NewLocalFile(nsDependentString(path, len), 
                                   PR_TRUE, 
                                   aFile);
        }

        case Win_ProgramFiles:
        {
            return GetWindowsFolder(CSIDL_PROGRAM_FILES, aFile);
        }

        case Win_HomeDirectory:
        {    
            PRInt32 len;
            if ((len = ::GetEnvironmentVariableW(L"HOME", path, MAX_PATH)) > 0)
            {
                // Need enough space to add the trailing backslash
                if (len > MAX_PATH - 2)
                    break;
               
                path[len]   = L'\\';
                path[++len] = L'\0';

                return NS_NewLocalFile(nsDependentString(path, len), 
                                       PR_TRUE, 
                                       aFile);
            }

            len = ::GetEnvironmentVariableW(L"HOMEDRIVE", path, MAX_PATH);
            if (0 < len && len < MAX_PATH)
            {
                WCHAR temp[MAX_PATH];
                DWORD len2 = ::GetEnvironmentVariableW(L"HOMEPATH", temp, MAX_PATH);
                if (0 < len2 && len + len2 < MAX_PATH)
                    wcsncat(path, temp, len2);
        
                len = wcslen(path);

                // Need enough space to add the trailing backslash
                if (len > MAX_PATH - 2)
                    break;
            
                path[len]   = L'\\';
                path[++len] = L'\0';
                
                return NS_NewLocalFile(nsDependentString(path, len), 
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

        case Win_Downloads:
        {
            // Defined in KnownFolders.h.
            GUID folderid_downloads = {0x374de290, 0x123f, 0x4565, {0x91, 0x64,
                                       0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b}};
            nsresult rv = GetKnownFolder(&folderid_downloads, aFile);
            // On WinXP and 2k, there is no downloads folder, default
            // to 'Desktop'.
            if(NS_ERROR_FAILURE == rv)
            {
              rv = GetWindowsFolder(CSIDL_DESKTOP, aFile);
            }
            return rv;
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
#ifndef WINCE
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
        case Win_Printhood:
        {
            return GetWindowsFolder(CSIDL_PRINTHOOD, aFile);
        }
        case Win_Cookies:
        {
            return GetWindowsFolder(CSIDL_COOKIES, aFile);
        }
#endif
        case Win_Appdata:
        {
            return GetWindowsFolder(CSIDL_APPDATA, aFile);
        }

        case Win_LocalAppdata:
        {
            return GetWindowsFolder(CSIDL_LOCAL_APPDATA, aFile);
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
            return GetUnixHomeDir(aFile);

        case Unix_DesktopDirectory:
        {
            nsCOMPtr<nsILocalFile> home;
            nsresult rv = GetUnixHomeDir(getter_AddRefs(home));
            if (NS_FAILED(rv))
                return rv;
            rv = home->AppendNative(NS_LITERAL_CSTRING("Desktop"));
            if (NS_FAILED(rv))
                return rv;
            PRBool exists;
            rv = home->Exists(&exists);
            if (NS_FAILED(rv))
                return rv;
            if (!exists)
                return GetUnixHomeDir(aFile);
              
            NS_ADDREF(*aFile = home);
            return NS_OK;
        }
#endif

#ifdef XP_BEOS
        case BeOS_SettingsDirectory:
        {
            return GetBeOSFolder(B_USER_SETTINGS_DIRECTORY,0, aFile);
        }

        case BeOS_HomeDirectory:
        {
            return GetBeOSFolder(B_USER_DIRECTORY,0, aFile);
        }

        case BeOS_DesktopDirectory:
        {
            /* Get the user's desktop folder, which in the future may differ from the boot desktop */
            char path[MAXPATHLEN];
            if (find_directory(B_USER_DIRECTORY, 0, false, path, MAXPATHLEN) != B_OK )
                break;
            return GetBeOSFolder(B_DESKTOP_DIRECTORY, dev_for_path(path), aFile);
        }

        case BeOS_SystemDirectory:
        {
            return GetBeOSFolder(B_BEOS_DIRECTORY,0, aFile);
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
            if (!fSuccess) {
                // this could happen if we are running without the WPS, return
                // the Home directory instead
                return GetSpecialSystemDirectory(OS2_HomeDirectory, aFile);
            }
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

