/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
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

#include "nsPluginDirServiceProvider.h"

#include "nsCRT.h"
#include "nsILocalFile.h"
#include "nsIPref.h"
#include "nsDependentString.h"
#include "nsXPIDLString.h"
#include "prmem.h"

#if defined (XP_WIN)
#include <windows.h>

typedef struct structVer
{
  WORD wMajor;
  WORD wMinor;
  WORD wRelease;
  WORD wBuild;
} verBlock;

static void ClearVersion(verBlock *ver)
{
  ver->wMajor   = 0;
  ver->wMinor   = 0;
  ver->wRelease = 0;
  ver->wBuild   = 0;
}

static BOOL FileExists(LPCSTR szFile)
{
  return GetFileAttributes(szFile) != 0xFFFFFFFF;
}

// Get file version information from a file
static BOOL GetFileVersion(LPSTR szFile, verBlock *vbVersion)
{
  UINT              uLen;
  UINT              dwLen;
  BOOL              bRv;
  DWORD             dwHandle;
  LPVOID            lpData;
  LPVOID            lpBuffer;
  VS_FIXEDFILEINFO  *lpBuffer2;

  ClearVersion(vbVersion);
  if(FileExists(szFile))
  {
    bRv    = TRUE;
    dwLen  = GetFileVersionInfoSize(szFile, &dwHandle);
    lpData = (LPVOID)malloc(dwLen);
    uLen   = 0;

    if(lpData && GetFileVersionInfo(szFile, dwHandle, dwLen, lpData) != 0)
    {
      if(VerQueryValue(lpData, "\\", &lpBuffer, &uLen) != 0)
      {
        lpBuffer2             = (VS_FIXEDFILEINFO *)lpBuffer;
        vbVersion->wMajor   = HIWORD(lpBuffer2->dwFileVersionMS);
        vbVersion->wMinor   = LOWORD(lpBuffer2->dwFileVersionMS);
        vbVersion->wRelease = HIWORD(lpBuffer2->dwFileVersionLS);
        vbVersion->wBuild   = LOWORD(lpBuffer2->dwFileVersionLS);
      }
    }
    free(lpData);
  }
  else
    /* File does not exist */
    bRv = FALSE;

  return(bRv);
}

// Will deep copy ver2 into ver1
static void CopyVersion(verBlock *ver1, verBlock *ver2)
{
  ver1->wMajor   = ver2->wMajor;
  ver1->wMinor   = ver2->wMinor;
  ver1->wRelease = ver2->wRelease;
  ver1->wBuild   = ver2->wBuild;
}

// convert a string version to a version struct
static void TranslateVersionStr(const char* szVersion, verBlock *vbVersion)
{
  LPSTR szNum1 = NULL;
  LPSTR szNum2 = NULL;
  LPSTR szNum3 = NULL;
  LPSTR szNum4 = NULL;
  LPSTR szJavaBuild = NULL;

  char *strVer = nsnull;
  if (szVersion)
    strVer = PL_strdup(szVersion);
  if (!strVer) {  // out of memory
    ClearVersion(vbVersion);
    return;
  }
  // Java may be using an underscore instead of a dot for the build ID
  szJavaBuild = strchr(strVer, '_');
  if (szJavaBuild)
    szJavaBuild[0] = '.';

  szNum1 = strtok(strVer, ".");
  szNum2 = strtok(NULL,   ".");
  szNum3 = strtok(NULL,   ".");
  szNum4 = strtok(NULL,   ".");

  vbVersion->wMajor   = szNum1 ? atoi(szNum1) : 0;
  vbVersion->wMinor   = szNum2 ? atoi(szNum2) : 0;
  vbVersion->wRelease = szNum3 ? atoi(szNum3) : 0;
  vbVersion->wBuild   = szNum4 ? atoi(szNum4) : 0;

  PL_strfree(strVer);
}

// Compare two version struct, return zero if the same
static int CompareVersion(verBlock vbVersionOld, verBlock vbVersionNew)
{
  if(vbVersionOld.wMajor > vbVersionNew.wMajor)
    return(4);
  else if(vbVersionOld.wMajor < vbVersionNew.wMajor)
    return(-4);

  if(vbVersionOld.wMinor > vbVersionNew.wMinor)
    return(3);
  else if(vbVersionOld.wMinor < vbVersionNew.wMinor)
    return(-3);

  if(vbVersionOld.wRelease > vbVersionNew.wRelease)
    return(2);
  else if(vbVersionOld.wRelease < vbVersionNew.wRelease)
    return(-2);

  if(vbVersionOld.wBuild > vbVersionNew.wBuild)
    return(1);
  else if(vbVersionOld.wBuild < vbVersionNew.wBuild)
    return(-1);

  /* the versions are all the same */
  return(0);
}
#endif

//*****************************************************************************
// nsPluginDirServiceProvider::Constructor/Destructor
//*****************************************************************************

nsPluginDirServiceProvider::nsPluginDirServiceProvider()
{
}

nsPluginDirServiceProvider::~nsPluginDirServiceProvider()
{
}

//*****************************************************************************
// nsPluginDirServiceProvider::nsISupports
//*****************************************************************************

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPluginDirServiceProvider, nsIDirectoryServiceProvider)

//*****************************************************************************
// nsPluginDirServiceProvider::nsIDirectoryServiceProvider
//*****************************************************************************

NS_IMETHODIMP
nsPluginDirServiceProvider::GetFile(const char *prop, PRBool *persistant, nsIFile **_retval)
{
  nsCOMPtr<nsILocalFile>  localFile;
  nsresult rv = NS_ERROR_FAILURE;

  NS_ENSURE_ARG(prop);
  *_retval = nsnull;
  *persistant = PR_TRUE;

#if defined(XP_WIN)    
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
  if (!prefs) return rv;

  if (nsCRT::strcmp(prop, NS_WIN_4DOTX_SCAN_KEY) == 0) {
    // check our prefs to see if scanning the 4.x folder has been explictly overriden
    // failure to get the pref is okay, we'll do what we've been doing -- a filtered scan
    PRBool bScan4x;
    if (NS_SUCCEEDED(prefs->GetBoolPref(NS_WIN_4DOTX_SCAN_KEY, &bScan4x)) && !bScan4x)
      return rv;

    // look for the plugin folder that the user has in their Communicator 4x install
    HKEY keyloc; 
    long result;
    DWORD type; 
    char szKey[_MAX_PATH] = "Software\\Netscape\\Netscape Navigator";
    char path[_MAX_PATH];

    result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ, &keyloc); 

    if (result == ERROR_SUCCESS) {
      char current_version[80];
      DWORD length = sizeof(current_version);

      result = ::RegQueryValueEx(keyloc, "CurrentVersion", NULL, &type, (LPBYTE)&current_version, &length); 

      ::RegCloseKey(keyloc);
      PL_strcat(szKey, "\\");
      PL_strcat(szKey, current_version);
      PL_strcat(szKey, "\\Main");
      result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ, &keyloc);

      if (result == ERROR_SUCCESS) {
        DWORD pathlen = sizeof(path); 

        result = ::RegQueryValueEx(keyloc, "Plugins Directory", NULL, &type, (LPBYTE)&path, &pathlen); 
        if (result == ERROR_SUCCESS)
          rv = NS_NewNativeLocalFile(nsDependentCString(path), PR_TRUE, getter_AddRefs(localFile));
        ::RegCloseKey(keyloc); 
      }
    }
  } else if (nsCRT::strcmp(prop, NS_WIN_JRE_SCAN_KEY) == 0) {
    PRBool isJavaEnabled;
    nsXPIDLCString strVer;
    if ((NS_FAILED(prefs->GetBoolPref("security.enable_java", &isJavaEnabled)) || !isJavaEnabled) ||
        NS_FAILED(prefs->GetCharPref(prop, getter_Copies(strVer))))
      return NS_ERROR_FAILURE;
    verBlock minVer;
    TranslateVersionStr(strVer.get(), &minVer);

    // Look for the Java OJI plugin via the JRE install path
    HKEY baseloc;
    HKEY keyloc;
    FILETIME modTime;
    DWORD type; 
    DWORD index = 0;
    DWORD numChars = _MAX_PATH;
    DWORD pathlen;
    verBlock maxVer;
    ClearVersion(&maxVer);
    char curKey[_MAX_PATH] = "Software\\JavaSoft\\Java Plug-in";
    char path[_MAX_PATH];
    char newestPath[_MAX_PATH + 4]; // to prevent buffer overrun when adding \bin

    newestPath[0] = 0;
    LONG result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, curKey, 0, KEY_READ, &baseloc);

    // we must enumerate through the keys because what if there is more than one version?
    while (ERROR_SUCCESS == result) {
      path[0] = 0;
      numChars = _MAX_PATH;
      pathlen = sizeof(path);
      result = ::RegEnumKeyEx(baseloc, index, curKey, &numChars, NULL, NULL, NULL, &modTime);
      index++;
      if (ERROR_SUCCESS == result) {
        if (ERROR_SUCCESS == ::RegOpenKeyEx(baseloc, curKey, 0, KEY_QUERY_VALUE, &keyloc)) {
          // we have a sub key
          if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, "JavaHome", NULL, &type, (LPBYTE)&path, &pathlen)) {
            verBlock curVer;
            TranslateVersionStr(curKey, &curVer);            
            if (CompareVersion(curVer, maxVer) >= 0 && CompareVersion(curVer, minVer) >= 0) {
              PL_strcpy(newestPath, path);
              CopyVersion(&maxVer, &curVer);
            }
            ::RegCloseKey(keyloc);
          }
        }
      }
    }

    ::RegCloseKey(baseloc);

    // if nothing is found, then don't add \bin dir
    if (newestPath[0] != 0) {
      PL_strcat(newestPath,"\\bin");
      rv = NS_NewNativeLocalFile(nsDependentCString(newestPath), PR_TRUE, getter_AddRefs(localFile));
    }
  } else if (nsCRT::strcmp(prop, NS_WIN_QUICKTIME_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(prop, getter_Copies(strVer))))
      return NS_ERROR_FAILURE;
    verBlock minVer;
    TranslateVersionStr(strVer.get(), &minVer);

    // look for the Quicktime system installation plugins directory
    HKEY keyloc; 
    long result;
    DWORD type; 
    verBlock qtVer;
    ClearVersion(&qtVer);
    char path[_MAX_PATH];
    DWORD pathlen = sizeof(path);      

    // first we need to check the version of Quicktime via checking the EXE's version table
    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\QuickTimePlayer.exe", 0, KEY_READ, &keyloc)) {
      if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, NULL, NULL, &type, (LPBYTE)&path, &pathlen)) {
        GetFileVersion((char*)path, &qtVer);
      }
      ::RegCloseKey(keyloc);
    }
    if (CompareVersion(qtVer, minVer) < 0)
      return rv;

    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "software\\Apple Computer, Inc.\\QuickTime", 0, KEY_READ, &keyloc)) {
      DWORD pathlen = sizeof(path); 

      result = ::RegQueryValueEx(keyloc, "InstallDir", NULL, &type, (LPBYTE)&path, &pathlen); 
      PL_strcat(path, "\\Plugins");
      if (result == ERROR_SUCCESS)
        rv = NS_NewNativeLocalFile(nsDependentCString(path), PR_TRUE, getter_AddRefs(localFile));
      ::RegCloseKey(keyloc);
    }
  } else if (nsCRT::strcmp(prop, NS_WIN_WMP_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(prop, getter_Copies(strVer))))
      return NS_ERROR_FAILURE;
    verBlock minVer;
    TranslateVersionStr(strVer.get(), &minVer);

    // look for Windows Media Player system installation plugins directory
    HKEY keyloc;
    DWORD type; 
    verBlock wmpVer;
    ClearVersion(&wmpVer);
    char path[_MAX_PATH];
    DWORD pathlen = sizeof(path); 

    // first we need to check the version of WMP
    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\wmplayer.exe", 0, KEY_READ, &keyloc)) {
      if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, NULL, NULL, &type, (LPBYTE)&path, &pathlen)) {
        GetFileVersion((char*)path, &wmpVer);
      }
      ::RegCloseKey(keyloc);
    }
    if (CompareVersion(wmpVer, minVer) < 0)
      return rv;

    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "software\\Microsoft\\MediaPlayer", 0, KEY_READ, &keyloc)) {
      if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, "Installation Directory", NULL, &type, (LPBYTE)&path, &pathlen))
        rv = NS_NewNativeLocalFile(nsDependentCString(path), PR_TRUE, getter_AddRefs(localFile));
      ::RegCloseKey(keyloc);
    }
  } else if (nsCRT::strcmp(prop, NS_WIN_ACROBAT_SCAN_KEY) == 0) {
    nsXPIDLCString strVer;
    if (NS_FAILED(prefs->GetCharPref(prop, getter_Copies(strVer))))
      return NS_ERROR_FAILURE;
    verBlock minVer;
    TranslateVersionStr(strVer.get(), &minVer);

    // look for Adobe Acrobat system installation plugins directory
    HKEY baseloc;
    HKEY keyloc;
    FILETIME modTime;
    DWORD type; 
    DWORD index = 0;
    DWORD numChars = _MAX_PATH;
    DWORD pathlen;
    verBlock maxVer;
    ClearVersion(&maxVer);
    char curKey[_MAX_PATH] = "software\\Adobe\\Acrobat Reader";
    char path[_MAX_PATH];
    char newestPath[_MAX_PATH + 8]; // to prevent buffer overrun when adding \browser

    newestPath[0] = 0;
    if (ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, curKey, 0, KEY_READ, &baseloc)) {
      PL_strcpy(curKey, "software\\Adobe\\Adobe Acrobat");
      if (ERROR_SUCCESS != ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, curKey, 0, KEY_READ, &baseloc))
        return NS_ERROR_FAILURE;
    }

    // we must enumerate through the keys because what if there is more than one version?
    LONG result = ERROR_SUCCESS;
    while (ERROR_SUCCESS == result) {
      path[0] = 0;
      numChars = _MAX_PATH;
      pathlen = sizeof(path);
      result = ::RegEnumKeyEx(baseloc, index, curKey, &numChars, NULL, NULL, NULL, &modTime);
      index++;
      if (ERROR_SUCCESS == result) {
        verBlock curVer;
        TranslateVersionStr(curKey, &curVer);
        PL_strcat(curKey, "\\InstallPath");
        if (ERROR_SUCCESS == ::RegOpenKeyEx(baseloc, curKey, 0, KEY_QUERY_VALUE, &keyloc)) {
          // we have a sub key
          if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, NULL, NULL, &type, (LPBYTE)&path, &pathlen)) {
            if (CompareVersion(curVer, maxVer) >= 0 && CompareVersion(curVer, minVer) >= 0) {
              PL_strcpy(newestPath, path);
              CopyVersion(&maxVer, &curVer);
            }
            ::RegCloseKey(keyloc);
          }
        }
      }
    }
    ::RegCloseKey(baseloc);
    if (newestPath[0] != 0) {
      PL_strcat(newestPath,"\\browser");
      rv = NS_NewNativeLocalFile(nsDependentCString(newestPath), PR_TRUE, getter_AddRefs(localFile));
    }

  }
#endif

  if (localFile && NS_SUCCEEDED(rv))
    return localFile->QueryInterface(NS_GET_IID(nsIFile), (void**)_retval);

  return rv;
}

