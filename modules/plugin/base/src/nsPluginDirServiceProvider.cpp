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

// little helper function to get version info
static char* GetKeyValue(char* verbuf, char* key)
{
  char        *buf = NULL;
  UINT        blen;

  long result = ::VerQueryValue(verbuf,
                                TEXT(key),
                                (void **)&buf, &blen);

  return buf;
}
#endif

#define RETURN_AND_FREE_STRINGS(ret)    \
PR_BEGIN_MACRO                          \
    PL_strfree(ver1);                   \
    PL_strfree(ver2);                   \
    return (ret);                       \
PR_END_MACRO

/*
 * Compares two string versions
 * Takes into account special Java versions with trailing underscores like "1.3.1_02"
 * Returns 0 if identical, 1 if aCharVer1 is newer or -1 if aCharVer2 is newer
 */
static PRInt32 CompareVersions(const char *aCharVer1, const char *aCharVer2)
{
  char *ver1 = PL_strdup(aCharVer1);
  char *ver2 = PL_strdup(aCharVer2);

  char *lasts1, *lasts2; 
  char *tmp1 = PL_strtok_r(ver1, ".", &lasts1);
  char *tmp2 = PL_strtok_r(ver2, ".", &lasts2);
  while (tmp1 || tmp2) {
    PRUint32 r1 = tmp1 ? atoi(tmp1) : 0;
    PRUint32 r2 = tmp2 ? atoi(tmp2) : 0;

    if (r1 > r2)
      RETURN_AND_FREE_STRINGS(1);
    else if (r2 > r1)
      RETURN_AND_FREE_STRINGS(-1);

    tmp1 = PL_strtok_r(nsnull, ".", &lasts1);
    tmp2 = PL_strtok_r(nsnull, ".", &lasts2);
  }

  // if we've gotten this far, check the buildID's.
  tmp1 = PL_strprbrk(aCharVer1, "_");
  tmp2 = PL_strprbrk(aCharVer2, "_");
  if (tmp1 || tmp2) {
    PRUint32 r1 = tmp1 ? atoi(++tmp1) : 0;
    PRUint32 r2 = tmp2 ? atoi(++tmp2) : 0;

    if (r1 > r2)
      RETURN_AND_FREE_STRINGS(1);
    else if (r2 > r1)
      RETURN_AND_FREE_STRINGS(-1);
  }

  RETURN_AND_FREE_STRINGS(0);  // everything is the same
}

//*****************************************************************************
// nsPluginDirServiceProvider::Constructor/Destructor
//*****************************************************************************

nsPluginDirServiceProvider::nsPluginDirServiceProvider()
{
  NS_INIT_ISUPPORTS();
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
    nsXPIDLCString minVer;
    if ((NS_FAILED(prefs->GetBoolPref("security.enable_java", &isJavaEnabled)) || !isJavaEnabled) ||
        NS_FAILED(prefs->GetCharPref(prop, getter_Copies(minVer))))
      return NS_ERROR_FAILURE;

    // Look for the Java OJI plugin via the JRE install path
    HKEY baseloc;
    HKEY keyloc;
    FILETIME modTime;
    DWORD type; 
    DWORD index = 0;
    DWORD numChars = _MAX_PATH;
    DWORD pathlen;
    char maxVer[_MAX_PATH] = "0";
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
            if (CompareVersions(curKey, maxVer) >= 0 && CompareVersions(curKey, minVer.get()) >= 0) {
              PL_strcpy(newestPath, path);
              PL_strcpy(maxVer, curKey);
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
    nsXPIDLCString minVer;
    if (NS_FAILED(prefs->GetCharPref(prop, getter_Copies(minVer))))
      return NS_ERROR_FAILURE;

    // look for the Quicktime system installation plugins directory
    HKEY keyloc; 
    long result;
    DWORD type; 
    char qtVer[_MAX_PATH] = "0";
    char path[_MAX_PATH];
    DWORD pathlen = sizeof(path);      

    // first we need to check the version of Quicktime via checking the EXE's version table
    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\QuickTimePlayer.exe", 0, KEY_READ, &keyloc)) {
      if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, NULL, NULL, &type, (LPBYTE)&path, &pathlen)) {
        DWORD zerome, versionsize;
        char* verbuf = nsnull;
        versionsize = ::GetFileVersionInfoSize((char*)path, &zerome);
        if (versionsize > 0)
          verbuf = (char *)PR_Malloc(versionsize);
        if (!verbuf) rv = NS_ERROR_OUT_OF_MEMORY;

        else if (::GetFileVersionInfo((char*)path, NULL, versionsize, verbuf))
          PL_strcpy(qtVer, GetKeyValue(verbuf, "\\StringFileInfo\\040904b0\\FileVersion"));

        if (verbuf) PR_Free(verbuf);
      }
      ::RegCloseKey(keyloc);
    }
    if (CompareVersions(qtVer, minVer.get()) < 0)
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
    nsXPIDLCString minVer;
    if (NS_FAILED(prefs->GetCharPref(prop, getter_Copies(minVer))))
      return NS_ERROR_FAILURE;

    // look for Windows Media Player system installation plugins directory
    HKEY keyloc;
    DWORD type; 
    char wmpVer[_MAX_PATH] = "0";
    char path[_MAX_PATH];
    DWORD pathlen = sizeof(path); 

    // first we need to check the version of WMP
    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\wmplayer.exe", 0, KEY_READ, &keyloc)) {
      if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, NULL, NULL, &type, (LPBYTE)&path, &pathlen)) {
        DWORD zerome, versionsize;
        char* verbuf = nsnull;
        versionsize = ::GetFileVersionInfoSize((char*)path, &zerome);
        if (versionsize > 0)
          verbuf = (char *)PR_Malloc(versionsize);
        if (!verbuf) rv = NS_ERROR_OUT_OF_MEMORY;

        else if (::GetFileVersionInfo((char*)path, NULL, versionsize, verbuf))
          PL_strcpy(wmpVer, GetKeyValue(verbuf, "\\StringFileInfo\\040904E4\\FileVersion"));

        if (verbuf) PR_Free(verbuf);
      }
      ::RegCloseKey(keyloc);
    }
    if (CompareVersions(wmpVer, minVer.get()) < 0)
      return rv;

    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, "software\\Microsoft\\MediaPlayer", 0, KEY_READ, &keyloc)) {
      if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, "Installation Directory", NULL, &type, (LPBYTE)&path, &pathlen))
        rv = NS_NewNativeLocalFile(nsDependentCString(path), PR_TRUE, getter_AddRefs(localFile));
      ::RegCloseKey(keyloc);
    }
  } else if (nsCRT::strcmp(prop, NS_WIN_ACROBAT_SCAN_KEY) == 0) {
    nsXPIDLCString minVer;
    if (NS_FAILED(prefs->GetCharPref(prop, getter_Copies(minVer))))
      return NS_ERROR_FAILURE;

    // look for Adobe Acrobat system installation plugins directory
    HKEY baseloc;
    HKEY keyloc;
    FILETIME modTime;
    DWORD type; 
    DWORD index = 0;
    DWORD numChars = _MAX_PATH;
    DWORD pathlen;
    char maxVer[_MAX_PATH] = "0";
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
        PL_strcat(curKey, "\\InstallPath");
        if (ERROR_SUCCESS == ::RegOpenKeyEx(baseloc, curKey, 0, KEY_QUERY_VALUE, &keyloc)) {
          // we have a sub key
          if (ERROR_SUCCESS == ::RegQueryValueEx(keyloc, NULL, NULL, &type, (LPBYTE)&path, &pathlen)) {
            if (CompareVersions(curKey, maxVer) >= 0 && CompareVersions(curKey, minVer.get()) >= 0) {
              PL_strcpy(newestPath, path);
              PL_strcpy(maxVer, curKey);
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

