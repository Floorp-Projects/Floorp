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

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include "prenv.h"

#endif

#if defined(VMS)
#include <unixlib.h>
#endif

#ifndef MAXPATHLEN
#ifdef PATH_MAX
#define MAXPATHLEN PATH_MAX
#elif defined(MAX_PATH)
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

void StartupSpecialSystemDirectory()
{
#if defined (XP_WIN)
    // SHGetKnownFolderPath is only available on Windows Vista
    // so that we need to use GetProcAddress to get the pointer.
    gShell32DLLInst = LoadLibraryW(L"shell32.dll");
    if(gShell32DLLInst)
    {
        gGetKnownFolderPath = (nsGetKnownFolderPath)
            GetProcAddress(gShell32DLLInst, "SHGetKnownFolderPath");
    }
#endif
}

void ShutdownSpecialSystemDirectory()
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
    WCHAR path_orig[MAX_PATH + 3];
    WCHAR *path = path_orig+1;
    HRESULT result = SHGetSpecialFolderPathW(NULL, path, folder, true);

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

/**
 * Provides a fallback for getting the path to APPDATA or LOCALAPPDATA by
 * querying the registry when the call to SHGetSpecialFolderPathW is unable to
 * provide these paths (Bug 513958).
 */
static nsresult GetRegWindowsAppDataFolder(bool aLocal, nsILocalFile** aFile)
{
    HKEY key;
    NS_NAMED_LITERAL_STRING(keyName,
    "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders");
    DWORD res = ::RegOpenKeyExW(HKEY_CURRENT_USER, keyName.get(), 0, KEY_READ,
                                &key);
    if (res != ERROR_SUCCESS)
        return NS_ERROR_FAILURE;

    WCHAR path[MAX_PATH + 2];
    DWORD type, size;
    res = RegQueryValueExW(key, (aLocal ? L"Local AppData" : L"AppData"), NULL,
                           &type, (LPBYTE)&path, &size);
    ::RegCloseKey(key);
    // The call to RegQueryValueExW must succeed, the type must be REG_SZ, the
    // buffer size must not equal 0, and the buffer size be a multiple of 2.
    if (res != ERROR_SUCCESS || type != REG_SZ || size == 0 || size % 2 != 0)
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
#elif defined(ANDROID)
    // XXX no home dir on android; maybe we should return the sdcard if present?
    return NS_ERROR_FAILURE;
#else
    return NS_NewNativeLocalFile(nsDependentCString(PR_GetEnv("HOME")),
                                 PR_TRUE, aFile);
#endif
}

/*
  The following license applies to the xdg_user_dir_lookup function:

  Copyright (c) 2007 Red Hat, Inc.

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

static char *
xdg_user_dir_lookup (const char *type)
{
  FILE *file;
  char *home_dir, *config_home, *config_file;
  char buffer[512];
  char *user_dir;
  char *p, *d;
  int len;
  int relative;

  home_dir = getenv ("HOME");

  if (home_dir == NULL)
    goto error;

  config_home = getenv ("XDG_CONFIG_HOME");
  if (config_home == NULL || config_home[0] == 0)
    {
      config_file = (char*) malloc (strlen (home_dir) + strlen ("/.config/user-dirs.dirs") + 1);
      if (config_file == NULL)
        goto error;

      strcpy (config_file, home_dir);
      strcat (config_file, "/.config/user-dirs.dirs");
    }
  else
    {
      config_file = (char*) malloc (strlen (config_home) + strlen ("/user-dirs.dirs") + 1);
      if (config_file == NULL)
        goto error;

      strcpy (config_file, config_home);
      strcat (config_file, "/user-dirs.dirs");
    }

  file = fopen (config_file, "r");
  free (config_file);
  if (file == NULL)
    goto error;

  user_dir = NULL;
  while (fgets (buffer, sizeof (buffer), file))
    {
      /* Remove newline at end */
      len = strlen (buffer);
      if (len > 0 && buffer[len-1] == '\n')
	buffer[len-1] = 0;

      p = buffer;
      while (*p == ' ' || *p == '\t')
	p++;

      if (strncmp (p, "XDG_", 4) != 0)
	continue;
      p += 4;
      if (strncmp (p, type, strlen (type)) != 0)
	continue;
      p += strlen (type);
      if (strncmp (p, "_DIR", 4) != 0)
	continue;
      p += 4;

      while (*p == ' ' || *p == '\t')
	p++;

      if (*p != '=')
	continue;
      p++;

      while (*p == ' ' || *p == '\t')
	p++;

      if (*p != '"')
	continue;
      p++;

      relative = 0;
      if (strncmp (p, "$HOME/", 6) == 0)
	{
	  p += 6;
	  relative = 1;
	}
      else if (*p != '/')
	continue;

      if (relative)
	{
	  user_dir = (char*) malloc (strlen (home_dir) + 1 + strlen (p) + 1);
          if (user_dir == NULL)
            goto error2;

	  strcpy (user_dir, home_dir);
	  strcat (user_dir, "/");
	}
      else
	{
	  user_dir = (char*) malloc (strlen (p) + 1);
          if (user_dir == NULL)
            goto error2;

	  *user_dir = 0;
	}

      d = user_dir + strlen (user_dir);
      while (*p && *p != '"')
	{
	  if ((*p == '\\') && (*(p+1) != 0))
	    p++;
	  *d++ = *p++;
	}
      *d = 0;
    }
error2:
  fclose (file);

  if (user_dir)
    return user_dir;

 error:
  return NULL;
}

static const char xdg_user_dirs[] =
    "DESKTOP\0"
    "DOCUMENTS\0"
    "DOWNLOAD\0"
    "MUSIC\0"
    "PICTURES\0"
    "PUBLICSHARE\0"
    "TEMPLATES\0"
    "VIDEOS";

static const PRUint8 xdg_user_dir_offsets[] = {
    0,
    8,
    18,
    27,
    33,
    42,
    54,
    64
};

static nsresult
GetUnixXDGUserDirectory(SystemDirectories aSystemDirectory,
                        nsILocalFile** aFile)
{
    char *dir = xdg_user_dir_lookup
                    (xdg_user_dirs + xdg_user_dir_offsets[aSystemDirectory -
                                                         Unix_XDG_Desktop]);

    nsresult rv;
    nsCOMPtr<nsILocalFile> file;
    if (dir) {
        rv = NS_NewNativeLocalFile(nsDependentCString(dir), PR_TRUE,
                                   getter_AddRefs(file));
        free(dir);
    } else if (Unix_XDG_Desktop == aSystemDirectory) {
        // for the XDG desktop dir, fall back to HOME/Desktop
        // (for historical compatibility)
        rv = GetUnixHomeDir(getter_AddRefs(file));
        if (NS_FAILED(rv))
            return rv;

        rv = file->AppendNative(NS_LITERAL_CSTRING("Desktop"));
    }
#if defined(MOZ_PLATFORM_MAEMO)
    // "MYDOCSDIR" is exported to point to "/home/user/MyDocs" in maemo.
    else if (Unix_XDG_Documents == aSystemDirectory) {

        char *myDocs = PR_GetEnv("MYDOCSDIR");
        if (!myDocs || !*myDocs)
            return NS_ERROR_FAILURE;

        rv = NS_NewNativeLocalFile(nsDependentCString(myDocs), PR_TRUE,
                                   getter_AddRefs(file));
        if (NS_FAILED(rv))
            return rv;

        rv = file->AppendNative(NS_LITERAL_CSTRING(".documents"));
    }
#endif
    else {
      // no fallback for the other XDG dirs
      rv = NS_ERROR_FAILURE;
    }

    if (NS_FAILED(rv))
        return rv;

    bool exists;
    rv = file->Exists(&exists);
    if (NS_FAILED(rv))
        return rv;
    if (!exists) {
        rv = file->Create(nsIFile::DIRECTORY_TYPE, 0755);
        if (NS_FAILED(rv))
            return rv;
    }

    *aFile = nsnull;
    file.swap(*aFile);

    return NS_OK;
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
#if defined (XP_WIN)
            {
            DWORD len = ::GetTempPathW(MAX_PATH, path);
            if (len == 0)
                break;
            return NS_NewLocalFile(nsDependentString(path, len),
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
#elif defined(MOZ_WIDGET_COCOA)
        {
            return GetOSXFolderType(kUserDomain, kTemporaryFolderType, aFile);
        }

#elif defined(XP_UNIX)
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
            nsresult rv = GetWindowsFolder(CSIDL_PROFILE, aFile);
            if (NS_SUCCEEDED(rv))
                return rv;

            PRInt32 len;
            if ((len = ::GetEnvironmentVariableW(L"HOME", path, MAX_PATH)) > 0)
            {
                // Need enough space to add the trailing backslash
                if (len > MAX_PATH - 2)
                    break;

                path[len]   = L'\\';
                path[++len] = L'\0';

                rv = NS_NewLocalFile(nsDependentString(path, len),
                                     PR_TRUE,
                                     aFile);
                if (NS_SUCCEEDED(rv))
                    return rv;
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
        case Win_Appdata:
        {
            nsresult rv = GetWindowsFolder(CSIDL_APPDATA, aFile);
            if (NS_FAILED(rv))
                rv = GetRegWindowsAppDataFolder(PR_FALSE, aFile);
            return rv;
        }
        case Win_LocalAppdata:
        {
            nsresult rv = GetWindowsFolder(CSIDL_LOCAL_APPDATA, aFile);
            if (NS_FAILED(rv))
                rv = GetRegWindowsAppDataFolder(PR_TRUE, aFile);
            return rv;
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

        case Unix_XDG_Desktop:
        case Unix_XDG_Documents:
        case Unix_XDG_Download:
        case Unix_XDG_Music:
        case Unix_XDG_Pictures:
        case Unix_XDG_PublicShare:
        case Unix_XDG_Templates:
        case Unix_XDG_Videos:
            return GetUnixXDGUserDirectory(aSystemSystemDirectory, aFile);
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

#if defined (MOZ_WIDGET_COCOA)
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

