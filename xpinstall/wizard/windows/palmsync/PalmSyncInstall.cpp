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
 * Netscape Communications Corporation
 *
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Rajiv Dayal <rdayal@netscape.com>
 *		David Bienvenu <bienvenu@nventure.com>
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

#include <windows.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <Winreg.h>

#include "CondMgr.h"
#include "HSAPI.h"
#include "resource.h"

#define MOZ_PALMSYNC_PROXY      ".\\PalmSyncProxy.dll"
#define CREATOR                  "addr"
#define CONDUIT_FILENAME         "mozABConduit.dll"
#define REMOTE_DB                "AddressDB"
#define CONDUIT_NAME             "address"
#define CONDUIT_PRIORITY         2

#define CONDMGR_FILENAME         "CondMgr.dll"
#define HSAPI_FILENAME           "HsApi.dll"
#define DIRECTORY_SEPARATOR      '\\'
#define DIRECTORY_SEPARATOR_STR  "\\"
#define EXECUTABLE_EXTENSION     ".exe"
#define HOTSYNC_MAX_WAIT         30         // wait for HotSync to start/stop in seconds

#define MAX_LOADSTRING 256

// Define any Conduit Manager function pointer types
typedef int (WINAPI *CmGetCorePathPtr)(TCHAR *pPath, int *piSize);
typedef int (WINAPI *CmGetHotSyncExecPathPtr)(char *szPath, int *iSize);

typedef int (WINAPI *CmInstallCreatorPtr)(const char *pCreator, int iType);
typedef int (WINAPI *CmRemoveConduitByCreatorIDPtr)(const char *pCreator);
typedef int (WINAPI *CmRestoreHotSyncSettingsPtr)(BOOL bToDefaults);
typedef int (WINAPI *CmSetCreatorRemotePtr)(const char *pCreator, const TCHAR *pRemote);
typedef int (WINAPI *CmSetCreatorNamePtr)(const char *pCreator, const TCHAR *pConduitName);
typedef int (WINAPI *CmSetCreatorTitlePtr)(const char *pCreator, const TCHAR *pConduitTitle);
typedef int (WINAPI *CmSetCreatorFilePtr)(const char *pCreator, const TCHAR *pConduitFile);
typedef int (WINAPI *CmSetCreatorDirectoryPtr)(const char *pCreator, const TCHAR *pConduitDirectory);

typedef int (WINAPI *CmSetCreatorPriorityPtr)(const char *pCreator, DWORD dwPriority);
typedef int (WINAPI *CmSetCreatorIntegratePtr)(const char *pCreator, DWORD dwIntegrate);
typedef int (WINAPI *CmSetCreatorValueDwordPtr)(const char *pCreator, TCHAR *pValue, DWORD dwValue);
typedef int (WINAPI *CmSetCreatorValueStringPtr)(const char *pCreator, TCHAR *pValueName, TCHAR *value);
typedef int (WINAPI *CmSetCorePathPtr) (const char *pPath);
typedef int (WINAPI *CmSetHotSyncExePathPtr) (const char *pPath);
typedef int (WINAPI *CmSetCreatorModulePtr) (const char *pCreatorID, const TCHAR *pModule);

typedef int (WINAPI *CmGetCreatorNamePtr)(const char *pCreator, TCHAR *pConduitName, int *pSize);
typedef int (WINAPI *CmGetCreatorTitlePtr)(const char *pCreator, TCHAR *pConduitTitle, int *pSize);
typedef int (WINAPI *CmGetCreatorPriorityPtr)(const char *pCreator, DWORD *dwPriority);
typedef int (WINAPI *CmGetCreatorTypePtr)(const char *pCreator);
typedef int (WINAPI *CmGetCreatorIntegratePtr)(const char *pCreator, DWORD *dwIntegrate);
typedef int (WINAPI *CmGetCreatorValueDwordPtr)(const char *pCreator, TCHAR *pValueName, DWORD dwValue, DWORD dwDefault);
typedef int (WINAPI *CmGetCreatorValueStringPtr)(const char *pCreator, TCHAR *pValueName, TCHAR *pValue, int *pSize, TCHAR *pDefault);
typedef int (WINAPI *CmGetCreatorFilePtr) (const TCHAR *pCreatorID, TCHAR *pFile, int *piSize);
typedef int (WINAPI *CmGetCreatorDirectoryPtr) (const TCHAR *pCreatorID, TCHAR *pFile, int *piSize);
typedef int (WINAPI *CmGetCreatorModulePtr) (const char *pCreatorID, TCHAR *pModule, int *piSize);
typedef int (WINAPI *CmGetCreatorRemotePtr)(const char *pCreator, const TCHAR *pRemote, int*pSize);

// Define any HSAPI function pointer types
typedef int (WINAPI *HsCheckApiStatusPtr)(void);
typedef int (WINAPI *HsGetSyncStatusPtr)(DWORD *dwStatus);
typedef int (WINAPI *HsSetAppStatusPtr)(HsStatusType statusType, DWORD dwStartFlags);

// Define general registration fn pointer types
typedef int (WINAPI *mozDllRegisterServerPtr)(void);
typedef int (WINAPI *mozDllUnregisterServerPtr)(void);

// forward declaration
int  InstallConduit(HINSTANCE hInstance, TCHAR *installPath);
int UninstallConduit();
void ConstructMessage(HINSTANCE hInstance, DWORD dwMessageId, TCHAR *formattedMsg);

// Global vars
BOOL    gWasHotSyncRunning = FALSE;

void ConstructMessage(HINSTANCE hInstance, DWORD dwMessageId, TCHAR *formattedMsg)
{
  // Load brand name and the format string.
  TCHAR brandName[MAX_LOADSTRING];
  TCHAR formatString[MAX_LOADSTRING];
  LoadString(hInstance, IDS_BRAND_NAME, brandName, MAX_LOADSTRING-1);
  LoadString(hInstance, dwMessageId, formatString, MAX_LOADSTRING-1);

  // A few msgs needs two brand name substitutions.
  if ((dwMessageId == IDS_SUCCESS_INSTALL) ||
      (dwMessageId == IDS_CONFIRM_INSTALL) ||
      (dwMessageId == IDS_ERR_REGISTERING_MOZ_DLL))
    _sntprintf(formattedMsg, MAX_LOADSTRING-1, formatString, brandName, brandName);
  else
    _sntprintf(formattedMsg, MAX_LOADSTRING-1, formatString, brandName);

  formattedMsg[MAX_LOADSTRING-1]='\0';
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
    TCHAR appTitle[MAX_LOADSTRING];
    TCHAR msgStr[MAX_LOADSTRING];

    int strResource=0;
    int res=-1;

    // /p can only be used with a standard install, i.e., non-silent install
    char *installDir = strstr(lpCmdLine, "/p");
    if (installDir)
      installDir += 2; // advance past "/p", e.g., "/pC:/program files/mozilla/dist/bin"

    if(!strcmpi(lpCmdLine,"/u")) // un-install
    {
        ConstructMessage(hInstance, IDS_APP_TITLE_UNINSTALL, appTitle);
        ConstructMessage(hInstance, IDS_CONFIRM_UNINSTALL, msgStr);
        if (MessageBox(NULL, msgStr, appTitle, MB_YESNO) == IDYES) 
        {
            res = UninstallConduit();
            if(!res)
                res = IDS_SUCCESS_UNINSTALL;
        }
        else
          return TRUE;;
    }
    else if (!strcmpi(lpCmdLine,"/us")) // silent un-install
    {
        res = UninstallConduit();
        if(!res)
            return TRUE; // success
        return res;
    }
    else if (!strcmpi(lpCmdLine,"/s")) // silent install
    {
        res = InstallConduit(hInstance, installDir);
        if(!res)
            return TRUE; // success
        return res;
    }
    else // install
    {
        ConstructMessage(hInstance, IDS_APP_TITLE_INSTALL, appTitle);
        ConstructMessage(hInstance, IDS_CONFIRM_INSTALL, msgStr);
        if (MessageBox(NULL, msgStr, appTitle, MB_YESNO) == IDYES) 
        {
            res = InstallConduit(hInstance, installDir);
            if(!res)
                res = IDS_SUCCESS_INSTALL;
        }
    }

    if(res > IDS_ERR_MAX || res < IDS_ERR_GENERAL)
        res = IDS_ERR_GENERAL;

    ConstructMessage(hInstance, res, msgStr);
    MessageBox(NULL, msgStr, appTitle, MB_OK);

    return TRUE;
}

// this function gets the install dir for installation
int GetPalmDesktopInstallDirectory(TCHAR *pPDInstallDirectory, unsigned long *pSize)
{
    HKEY   key;
    // open the key
    LONG rc = ::RegOpenKey(HKEY_CURRENT_USER, "Software\\U.S. Robotics\\Pilot Desktop\\Core", &key);
    if (rc == ERROR_SUCCESS) {
        // get key value
        rc = ::RegQueryValueEx(key, "Path", NULL, NULL, 
                               (LPBYTE)pPDInstallDirectory, pSize);
        if (rc == ERROR_SUCCESS) {
            *pSize = _tcslen(pPDInstallDirectory); // windows only
            rc=0; // 0 is success for us
        }
        // close the key
        ::RegCloseKey(key);
    }

    if(rc) {
        HKEY   key2;
        // open the key
        rc = ::RegOpenKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\palm.exe", &key2);
        if (rc == ERROR_SUCCESS) {
            // get the default key value
            rc = ::RegQueryValueEx(key2, "", NULL, NULL, 
                                   (LPBYTE)pPDInstallDirectory, pSize);
            // get only the path (ie, strip out the exe name). note that we don't use string match
            // for the exe name here since it's possilbe that the exe name in the default setting
            // is different from the exe name in RegOpenKey() call. For example, the exe name in
            // the default setting for "Software\\...\\App Paths\\pbrush.exe" is mspaint.exe.
            if (rc == ERROR_SUCCESS) {
              TCHAR *end = pPDInstallDirectory + _tcslen(pPDInstallDirectory);
              while ((*end != '\\') && (end != pPDInstallDirectory))
                end--;
              *end = '\0';
              rc=0; // 0 is success for us
            }
            // close the key
            ::RegCloseKey(key2);
        }
    }
    
    return rc;
}

// this function loads the Conduit Manager
int LoadConduitManagerDll(HINSTANCE* hCondMgrDll, const TCHAR * szPalmDesktopDirectory)
{
    // Initialize the return value
    *hCondMgrDll=NULL;
    
    // Construct the path of the Palm Desktop Conduit Manager
    TCHAR   szPDCondMgrPath[_MAX_PATH];
    // take care of any possible string overwrites
    if((strlen(szPalmDesktopDirectory) + strlen(DIRECTORY_SEPARATOR_STR) +  strlen(CONDMGR_FILENAME)) >= _MAX_PATH)
        return IDS_ERR_LOADING_CONDMGR;
    strcpy(szPDCondMgrPath, szPalmDesktopDirectory);
    strcat(szPDCondMgrPath, DIRECTORY_SEPARATOR_STR);
    strcat(szPDCondMgrPath, CONDMGR_FILENAME);
    // Load the Conduit Manager library from the Palm Desktop directory
    if( (*hCondMgrDll=LoadLibrary(szPDCondMgrPath)) != NULL )
        // Successfully loaded CondMgr Library from Palm Desktop Directory
    return 0;

    return IDS_ERR_LOADING_CONDMGR;
}

// this function loads the Hsapi Dll
int LoadHsapiDll(HINSTANCE* hHsapiDLL, const TCHAR * szPalmDesktopDirectory)
{
    // Initialize the return value
    *hHsapiDLL=NULL;
    
    TCHAR   szHsapiPath[_MAX_PATH];
    // take care of any possible string overwrites
    if((strlen(szPalmDesktopDirectory) + strlen(DIRECTORY_SEPARATOR_STR) +  strlen(HSAPI_FILENAME)) >= _MAX_PATH)
        return IDS_ERR_LOADING_CONDMGR;
    strcpy(szHsapiPath, szPalmDesktopDirectory);
    strcat(szHsapiPath, DIRECTORY_SEPARATOR_STR);
    strcat(szHsapiPath, HSAPI_FILENAME);
    if( (*hHsapiDLL=LoadLibrary(szHsapiPath)) != NULL )
        // Successfully loaded HSAPI Library from Palm Desktop Directory
        return 0;

    // If we get here, then there was an error loading the library
    return IDS_ERR_HSAPI_NOT_FOUND;
}

// finds if HotSync Manager is running
BOOL IsHotSyncRunning(HINSTANCE hHsapiDLL)
{
    BOOL    bRetVal = FALSE;
    
    if(!hHsapiDLL)
        return bRetVal;
    
    // Prepare to use the HSAPI functions
    HsCheckApiStatusPtr lpfnHsCheckApiStatus;
    lpfnHsCheckApiStatus = (HsCheckApiStatusPtr) GetProcAddress(hHsapiDLL, "HsCheckApiStatus");
    
    if( lpfnHsCheckApiStatus )
    {
        if( (*lpfnHsCheckApiStatus)() == 0 )
            bRetVal = TRUE;
    }
    return bRetVal;
}

// finds if a sync process is going on
BOOL IsHotSyncInProgress(HINSTANCE hHsapiDLL)
{
    DWORD dwStatus;
    
    if(!hHsapiDLL)
        return FALSE;
    
    if(IsHotSyncRunning(hHsapiDLL))
    {
        // Prepare to use the HSAPI functions
        HsGetSyncStatusPtr  lpfnHsGetSyncStatus;
        lpfnHsGetSyncStatus = (HsGetSyncStatusPtr) GetProcAddress(hHsapiDLL, "HsGetSyncStatus");
    
        if( lpfnHsGetSyncStatus )
        {
            if( (*lpfnHsGetSyncStatus)(&dwStatus) == 0 )
                if( dwStatus == HOTSYNC_STATUS_IDLE )
                    return FALSE;
        }
    }
    return TRUE;
}

// shuts down the HotSync Manager
void ShutdownHotSync(HINSTANCE hHsapiDLL)
{
    if(!hHsapiDLL)
        return;

    BOOL    bHotSyncRunning=IsHotSyncRunning(hHsapiDLL);
    
    if(bHotSyncRunning)
    {
        // Prepare to use the HSAPI functions
        HsSetAppStatusPtr   lpfnHsSetAppStatus;
        lpfnHsSetAppStatus = (HsSetAppStatusPtr) GetProcAddress(hHsapiDLL, "HsSetAppStatus");
    
        if( lpfnHsSetAppStatus )
            (*lpfnHsSetAppStatus)(HsCloseApp, HSFLAG_NONE);
        
        // Wait for HotSync to stop
        for( int i=0; (i<HOTSYNC_MAX_WAIT*2) && bHotSyncRunning; i++ )
        {
            if( (bHotSyncRunning=IsHotSyncRunning(hHsapiDLL)) == TRUE )
                Sleep(500);
        }
    }
}

// starts HotSync Manager if not runnning
void StartHotSync(HINSTANCE hHsapiDLL)
{
    if(!hHsapiDLL)
        return;

    BOOL    bHotSyncRunning=IsHotSyncRunning(hHsapiDLL);
    
    if(!bHotSyncRunning)
    {
        // Prepare to use the HSAPI functions
        HsSetAppStatusPtr   lpfnHsSetAppStatus;
        lpfnHsSetAppStatus = (HsSetAppStatusPtr) GetProcAddress(hHsapiDLL, "HsSetAppStatus");
    
        if( lpfnHsSetAppStatus )
            (*lpfnHsSetAppStatus)(HsStartApp, HSFLAG_NONE);
        
        // Wait for HotSync to start
        for( int i=0; (i<HOTSYNC_MAX_WAIT*2) && !bHotSyncRunning; i++ )
        {
            if( (bHotSyncRunning=IsHotSyncRunning(hHsapiDLL)) == FALSE )
                Sleep(500);
        }
    }
}

int RegisterMozPalmSyncDll()
{
    HINSTANCE hMozPalmSyncProxyDll = NULL;
    if( (hMozPalmSyncProxyDll=LoadLibrary(MOZ_PALMSYNC_PROXY)) != NULL ) {
        mozDllRegisterServerPtr lpfnmozDllRegisterServer;
        lpfnmozDllRegisterServer = (mozDllRegisterServerPtr) GetProcAddress(hMozPalmSyncProxyDll, "DllRegisterServer");
        DWORD dwReturnCode = (*lpfnmozDllRegisterServer)();
        if(dwReturnCode == S_OK)
            // Successfully registered
            return 0;
    }

    return IDS_ERR_REGISTERING_MOZ_DLL;
}

int UnregisterMozPalmSyncDll()
{
    HINSTANCE hMozPalmSyncProxyDll = NULL;
    if( (hMozPalmSyncProxyDll=LoadLibrary(MOZ_PALMSYNC_PROXY)) != NULL ) {
        mozDllUnregisterServerPtr lpfnmozDllUnregisterServer;
        lpfnmozDllUnregisterServer = (mozDllUnregisterServerPtr) GetProcAddress(hMozPalmSyncProxyDll, "DllUnregisterServer");
        DWORD dwReturnCode = (*lpfnmozDllUnregisterServer)();
        if(dwReturnCode == S_OK)
            // Successfully registered
            return 0;
    }

    return IDS_ERR_UNREGISTERING_MOZ_DLL;
}


char *mystrsep(char **stringp, char delim)
{
  char *endStr = strchr(*stringp, delim);
  char *retStr;
  if (endStr)
  {
    bool foundDelim = (*endStr == delim);
    *endStr = '\0';
    retStr = *stringp;
    *stringp = endStr + !!foundDelim;
  }
  else
    return NULL;
  return retStr;
}

char oldSettingsStr[500];

// installs our Conduit
int InstallConduit(HINSTANCE hInstance, TCHAR *installDir)
{ 
    int dwReturnCode;
    BOOL    bHotSyncRunning = FALSE;

    // Prepare the full path of the conduit.
    // Applications should not place conduits in the Palm Desktop directory.
    // The Palm Desktop installer only manages the Palm Desktop conduits.

    TCHAR szConduitPath[_MAX_PATH];
    if (!installDir)
    {
      if(!GetModuleFileName(NULL, szConduitPath, _MAX_PATH))
          return IDS_ERR_CONDUIT_NOT_FOUND;
      // extract the dir path (without the module name)
      int index = strlen(szConduitPath)-1;
      while((szConduitPath[index] != DIRECTORY_SEPARATOR) && index)
          index--;
      szConduitPath[index] = 0;
    }
    else
      strncpy(szConduitPath, installDir, sizeof(szConduitPath) - 1);


    // take care of any possible string overwrites
    if((strlen(szConduitPath) + strlen(DIRECTORY_SEPARATOR_STR) + strlen(CONDUIT_FILENAME)) > _MAX_PATH)
        return IDS_ERR_LOADING_CONDMGR;
    // might already have conduit filename in szConduitPath if we're called recursively
    if (!strstr(szConduitPath, CONDUIT_FILENAME)) 
    {
      strcat(szConduitPath, DIRECTORY_SEPARATOR_STR);
      strcat(szConduitPath, CONDUIT_FILENAME);
    }
    // Make sure the conduit dll exists
    struct _finddata_t dll_file;
    long hFile;
    if( (hFile = _findfirst( szConduitPath, &dll_file )) == -1L )
        return IDS_ERR_CONDUIT_NOT_FOUND;

    // now register the Mozilla Palm Sync Support Dll
    if( (dwReturnCode = RegisterMozPalmSyncDll()) != 0)
        return dwReturnCode;
    
    // Get the Palm Desktop Installation directory
    TCHAR   szPalmDesktopDir[_MAX_PATH];
    unsigned long desktopSize=_MAX_PATH;
    
    // old conduit settings - MAX_PATH is arbitrarily long...
    TCHAR szOldCreatorName[_MAX_PATH];
    TCHAR szOldRemote[_MAX_PATH];
    TCHAR szOldCreatorTitle[_MAX_PATH];
    TCHAR szOldCreatorFile[_MAX_PATH];
    TCHAR szOldCreatorDirectory[_MAX_PATH];
    DWORD oldPriority;
    DWORD oldIntegrate;
    int   oldType;

    // Load the Conduit Manager DLL.
    HINSTANCE hConduitManagerDLL;
    if( (dwReturnCode=GetPalmDesktopInstallDirectory(szPalmDesktopDir, &desktopSize)) == 0 ) 
    {
        if( (dwReturnCode = LoadConduitManagerDll(&hConduitManagerDLL, szPalmDesktopDir)) != 0 )
            // load it from local dir if present by any chance
            if( (dwReturnCode = LoadConduitManagerDll(&hConduitManagerDLL, ".")) != 0 )
                return(dwReturnCode);
    }
    else // if registery key not load it from local dir if present by any chance
        if( (dwReturnCode = LoadConduitManagerDll(&hConduitManagerDLL, ".")) != 0 )
        return(dwReturnCode);
    
    // Prepare to install the conduit using Conduit Manager functions
    CmInstallCreatorPtr lpfnCmInstallCreator;
    lpfnCmInstallCreator = (CmInstallCreatorPtr) GetProcAddress(hConduitManagerDLL, "CmInstallCreator");
    CmSetCreatorRemotePtr   lpfnCmSetCreatorRemote;
    lpfnCmSetCreatorRemote = (CmSetCreatorRemotePtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorRemote");
    CmSetCreatorNamePtr lpfnCmSetCreatorName;
    lpfnCmSetCreatorName = (CmSetCreatorNamePtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorName");
    CmSetCreatorTitlePtr lpfnCmSetCreatorTitle;
    lpfnCmSetCreatorTitle = (CmSetCreatorTitlePtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorTitle");
    CmSetCreatorPriorityPtr lpfnCmSetCreatorPriority;
    lpfnCmSetCreatorPriority = (CmSetCreatorPriorityPtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorPriority");
    CmSetCreatorIntegratePtr    lpfnCmSetCreatorIntegrate;
    lpfnCmSetCreatorIntegrate = (CmSetCreatorIntegratePtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorIntegrate");
    CmRemoveConduitByCreatorIDPtr   lpfnCmRemoveConduitByCreatorID;
    lpfnCmRemoveConduitByCreatorID = (CmRemoveConduitByCreatorIDPtr) GetProcAddress(hConduitManagerDLL, "CmRemoveConduitByCreatorID");
    CmSetCreatorValueStringPtr lpfnCmSetCreatorValueString = (CmSetCreatorValueStringPtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorValueString");

    CmGetCreatorRemotePtr   lpfnCmGetCreatorRemote;
    lpfnCmGetCreatorRemote = (CmGetCreatorRemotePtr) GetProcAddress(hConduitManagerDLL, "CmGetCreatorRemote");
    CmGetCreatorNamePtr lpfnCmGetCreatorName;
    lpfnCmGetCreatorName = (CmGetCreatorNamePtr) GetProcAddress(hConduitManagerDLL, "CmGetCreatorName");
    CmGetCreatorTitlePtr lpfnCmGetCreatorTitle;
    lpfnCmGetCreatorTitle = (CmGetCreatorTitlePtr) GetProcAddress(hConduitManagerDLL, "CmGetCreatorTitle");
    CmGetCreatorPriorityPtr lpfnCmGetCreatorPriority;
    lpfnCmGetCreatorPriority = (CmGetCreatorPriorityPtr) GetProcAddress(hConduitManagerDLL, "CmGetCreatorPriority");
    CmGetCreatorIntegratePtr    lpfnCmGetCreatorIntegrate;
    lpfnCmGetCreatorIntegrate = (CmGetCreatorIntegratePtr) GetProcAddress(hConduitManagerDLL, "CmGetCreatorIntegrate");
    CmGetCreatorTypePtr lpfnCmGetCreatorType = (CmGetCreatorTypePtr) GetProcAddress(hConduitManagerDLL, "CmGetCreatorType");
    CmGetCreatorFilePtr lpfnCmGetCreatorFile = (CmGetCreatorFilePtr) GetProcAddress(hConduitManagerDLL, "CmGetCreatorFile");
    CmGetCreatorDirectoryPtr lpfnCmGetCreatorDirectory = (CmGetCreatorDirectoryPtr) GetProcAddress(hConduitManagerDLL, "CmGetCreatorDirectory");
    if( (lpfnCmInstallCreator == NULL) 
        || (lpfnCmSetCreatorRemote == NULL)
        || (lpfnCmSetCreatorName == NULL)
        || (lpfnCmSetCreatorTitle == NULL)
        || (lpfnCmSetCreatorPriority == NULL)
        || (lpfnCmSetCreatorIntegrate == NULL)
        )
    {
        // Return error code.
        return(IDS_ERR_LOADING_CONDMGR);
    }
 
    // get settings for old conduit
    int remoteBufSize = sizeof(szOldRemote);
    (*lpfnCmGetCreatorRemote) (CREATOR, szOldRemote, &remoteBufSize);
    int creatorBufSize = sizeof(szOldCreatorName);
    (*lpfnCmGetCreatorName)(CREATOR, szOldCreatorName, &creatorBufSize);
    int creatorTitleBufSize = sizeof(szOldCreatorTitle);
    (*lpfnCmGetCreatorTitle)(CREATOR, szOldCreatorTitle, &creatorTitleBufSize);
    int creatorFileBufSize = sizeof(szOldCreatorFile);
    int creatorDirectoryBufSize = sizeof(szOldCreatorDirectory);
    (*lpfnCmGetCreatorFile)(CREATOR, szOldCreatorFile, &creatorFileBufSize);
    (*lpfnCmGetCreatorDirectory)(CREATOR, szOldCreatorDirectory, &creatorDirectoryBufSize);
    (*lpfnCmGetCreatorPriority)(CREATOR, &oldPriority);
    (*lpfnCmGetCreatorIntegrate)(CREATOR, &oldIntegrate);
    oldType = (*lpfnCmGetCreatorType) (CREATOR);

    // if not set by previous pass through here
    if (!oldSettingsStr[0])
      _snprintf(oldSettingsStr, sizeof(oldSettingsStr), "%s,%s,%s,%s,%s,%d,%d,%d", szOldRemote, szOldCreatorName, 
        szOldCreatorTitle, szOldCreatorFile, szOldCreatorDirectory, oldType, oldPriority, oldIntegrate);

    // Load the HSAPI DLL.
    HINSTANCE hHsapiDLL;
    if( (dwReturnCode = LoadHsapiDll(&hHsapiDLL, szPalmDesktopDir)) != 0 )
        // load it from local dir if present by any chance
        if( (dwReturnCode = LoadHsapiDll(&hHsapiDLL, ".")) != 0 )
        return(dwReturnCode);
        
    // Shutdown the HotSync Process if it is running
    if( (bHotSyncRunning=IsHotSyncRunning(hHsapiDLL)) )
    {
        // Check for any synchronizations in progress
        if( IsHotSyncInProgress(hHsapiDLL) )
            return IDS_ERR_HOTSYNC_IN_PROGRESS;
            
        ShutdownHotSync(hHsapiDLL);
        // store the flag in temp global so that in the recursive call it is restarted
        gWasHotSyncRunning = TRUE;
    }
    
    // Actually install the conduit as an Application Conduit
    dwReturnCode = (*lpfnCmInstallCreator)(CREATOR, CONDUIT_APPLICATION);
    if(dwReturnCode == ERR_CREATORID_ALREADY_IN_USE) {
        dwReturnCode = (*lpfnCmRemoveConduitByCreatorID)(CREATOR);
        if(dwReturnCode >= 0 ) {
            //free the library so that the existing AB Conduit is unloaded properly
            FreeLibrary(hConduitManagerDLL);
            FreeLibrary(hHsapiDLL);
            return InstallConduit(hInstance, szConduitPath);
        }
    }
    if( dwReturnCode == 0 )
    {
        (*lpfnCmSetCreatorValueString) (CREATOR, "oldConduitSettings", oldSettingsStr);
        dwReturnCode = (*lpfnCmSetCreatorName)(CREATOR, szConduitPath);
        if( dwReturnCode != 0 ) return dwReturnCode;
        TCHAR title[MAX_LOADSTRING];
        // Construct conduit title (the one displayed in HotSync Mgr's Custom...list)..
        ConstructMessage(hInstance, IDS_CONDUIT_TITLE, title);
        dwReturnCode = (*lpfnCmSetCreatorTitle)(CREATOR, title);
        if( dwReturnCode != 0 ) return dwReturnCode;
        dwReturnCode = (*lpfnCmSetCreatorRemote)(CREATOR, REMOTE_DB);
        if( dwReturnCode != 0 ) return dwReturnCode;
        dwReturnCode = (*lpfnCmSetCreatorPriority)(CREATOR, CONDUIT_PRIORITY);
        if( dwReturnCode != 0 ) return dwReturnCode;
        // Applications should always set Integrate to 0
        dwReturnCode = (*lpfnCmSetCreatorIntegrate)(CREATOR, (DWORD)0);
        (*lpfnCmSetCreatorValueString) (CREATOR, "oldConduitSettings", oldSettingsStr);
    }
    
    // Re-start HotSync if it was running before
    if( gWasHotSyncRunning )
        StartHotSync(hHsapiDLL);
        
    return(dwReturnCode);
}

// uninstalls our conduit
int UninstallConduit()
{ 
    int dwReturnCode;
    BOOL    bHotSyncRunning = FALSE;

    // Get the Palm Desktop Installation directory
    TCHAR   szPalmDesktopDir[_MAX_PATH];
    unsigned long desktopSize=_MAX_PATH;
    // Load the Conduit Manager DLL.
    HINSTANCE hConduitManagerDLL;
    if( (dwReturnCode=GetPalmDesktopInstallDirectory(szPalmDesktopDir, &desktopSize)) == 0 )
    {
        if( (dwReturnCode = LoadConduitManagerDll(&hConduitManagerDLL, szPalmDesktopDir)) != 0 )
            // load it from local dir if present by any chance
            if( (dwReturnCode = LoadConduitManagerDll(&hConduitManagerDLL, ".")) != 0 )
                return(dwReturnCode);
    }
    // if registery key not load it from local dir if present by any chance
    else if( (dwReturnCode = LoadConduitManagerDll(&hConduitManagerDLL, ".")) != 0 )
          return(dwReturnCode);
    
    // Prepare to uninstall the conduit using Conduit Manager functions
    CmRemoveConduitByCreatorIDPtr   lpfnCmRemoveConduitByCreatorID;
    lpfnCmRemoveConduitByCreatorID = (CmRemoveConduitByCreatorIDPtr) GetProcAddress(hConduitManagerDLL, "CmRemoveConduitByCreatorID");
    if( (lpfnCmRemoveConduitByCreatorID == NULL) )
        return(IDS_ERR_LOADING_CONDMGR);
        CmSetCorePathPtr lpfnCmSetCorePath = (CmSetCorePathPtr) GetProcAddress(hConduitManagerDLL, "CmSetCorePath");
    CmSetHotSyncExePathPtr lpfnCmSetHotSyncExePath = (CmSetHotSyncExePathPtr) GetProcAddress(hConduitManagerDLL, "CmSetHotSyncExecPath");
    CmRestoreHotSyncSettingsPtr lpfnCmRestoreHotSyncSettings;
    lpfnCmRestoreHotSyncSettings = (CmRestoreHotSyncSettingsPtr) GetProcAddress(hConduitManagerDLL, "CmRestoreHotSyncSettings");
    CmGetCreatorValueStringPtr lpfnCmGetCreatorValueString = (CmGetCreatorValueStringPtr) GetProcAddress(hConduitManagerDLL, "CmGetCreatorValueString");
    CmInstallCreatorPtr lpfnCmInstallCreator;
    lpfnCmInstallCreator = (CmInstallCreatorPtr) GetProcAddress(hConduitManagerDLL, "CmInstallCreator");
    CmSetCreatorRemotePtr   lpfnCmSetCreatorRemote;
    lpfnCmSetCreatorRemote = (CmSetCreatorRemotePtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorRemote");
    CmSetCreatorNamePtr lpfnCmSetCreatorName;
    lpfnCmSetCreatorName = (CmSetCreatorNamePtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorName");
    CmSetCreatorTitlePtr lpfnCmSetCreatorTitle;
    lpfnCmSetCreatorTitle = (CmSetCreatorTitlePtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorTitle");
    CmSetCreatorPriorityPtr lpfnCmSetCreatorPriority;
    lpfnCmSetCreatorPriority = (CmSetCreatorPriorityPtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorPriority");
    CmSetCreatorIntegratePtr    lpfnCmSetCreatorIntegrate;
    lpfnCmSetCreatorIntegrate = (CmSetCreatorIntegratePtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorIntegrate");
    CmSetCreatorFilePtr lpfnCmSetCreatorFile = (CmSetCreatorFilePtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorFile");
    CmSetCreatorDirectoryPtr lpfnCmSetCreatorDirectory = (CmSetCreatorDirectoryPtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorDirectory");
    if( (lpfnCmRestoreHotSyncSettings == NULL) )
        return(IDS_ERR_LOADING_CONDMGR);
    
    // Load the HSAPI DLL.
    HINSTANCE hHsapiDLL;
    if( (dwReturnCode = LoadHsapiDll(&hHsapiDLL, szPalmDesktopDir)) != 0 )
        // load it from local dir if present by any chance
        if( (dwReturnCode = LoadHsapiDll(&hHsapiDLL, ".")) != 0 )
          return(dwReturnCode);
        
    // Shutdown the HotSync Process if it is running
    if( (bHotSyncRunning=IsHotSyncRunning(hHsapiDLL)) )
    {
        // Check for any synchronizations in progress
        if( IsHotSyncInProgress(hHsapiDLL) )
            return IDS_ERR_HOTSYNC_IN_PROGRESS;
            
        ShutdownHotSync(hHsapiDLL);
    }
    
    TCHAR oldConduitSettings[500];
    int strSize = sizeof(oldConduitSettings);
    dwReturnCode = (*lpfnCmGetCreatorValueString)(CREATOR, "oldConduitSettings", oldConduitSettings, &strSize, "");

    // Actually uninstall the conduit
    dwReturnCode = (*lpfnCmRemoveConduitByCreatorID)(CREATOR);

    if(dwReturnCode >= 0) 
    {
        // uninstall Mozilla Palm Sync Support Proxy Dll
        dwReturnCode = UnregisterMozPalmSyncDll();
//        dwReturnCode = (*lpfnCmRestoreHotSyncSettings)(TRUE);
    }

    if (dwReturnCode >= 0)
    {
      char * szOldCreatorName;
      char *szOldRemote;
      char *szOldCreatorTitle;
      char *szOldCreatorFile;
      char *szOldCreatorDirectory;
      char *oldIntStr;
      DWORD oldPriority;
      DWORD oldIntegrate;
      int   oldType;

      char *strPtr = oldConduitSettings;
      szOldRemote = mystrsep(&strPtr, ',');
      szOldCreatorName = mystrsep(&strPtr, ',');
      szOldCreatorTitle = mystrsep(&strPtr, ',');
      szOldCreatorFile = mystrsep(&strPtr, ',');
      szOldCreatorDirectory = mystrsep(&strPtr, ',');
      oldIntStr = mystrsep(&strPtr, ',');
      oldType = (oldIntStr) ? atoi(oldIntStr) : 0;
      oldIntStr = mystrsep(&strPtr, ',');
      oldPriority = (oldIntStr) ? atoi(oldIntStr) : 0;
      oldIntStr = mystrsep(&strPtr, ',');
      oldIntegrate = (oldIntStr) ? atoi(oldIntStr) : 0;

      dwReturnCode = (*lpfnCmInstallCreator)(CREATOR, oldType);
      if( dwReturnCode == 0 )
      {
          dwReturnCode = (*lpfnCmSetCreatorName)(CREATOR, szOldCreatorName);
          if( dwReturnCode != 0 ) return dwReturnCode;
          // Construct conduit title (the one displayed in HotSync Mgr's Custom...list)..
          dwReturnCode = (*lpfnCmSetCreatorTitle)(CREATOR, szOldCreatorTitle);
          if( dwReturnCode != 0 ) return dwReturnCode;
          dwReturnCode = (*lpfnCmSetCreatorRemote)(CREATOR, szOldRemote);
          if( dwReturnCode != 0 ) return dwReturnCode;
          dwReturnCode = (*lpfnCmSetCreatorFile)(CREATOR, szOldCreatorFile);
          if( dwReturnCode != 0 ) return dwReturnCode;
          dwReturnCode = (*lpfnCmSetCreatorDirectory)(CREATOR, szOldCreatorDirectory);
          if( dwReturnCode != 0 ) return dwReturnCode;
          dwReturnCode = (*lpfnCmSetCreatorPriority)(CREATOR, oldPriority);
          if( dwReturnCode != 0 ) return dwReturnCode;
          // Applications should always set Integrate to 0
          dwReturnCode = (*lpfnCmSetCreatorIntegrate)(CREATOR, oldIntegrate);
      }
    }

//    if (lpfnCmSetCorePath)
//      (*lpfnCmSetCorePath)(szPalmDesktopDir);
//    if (lpfnCmSetHotSyncExePath)
//      (*lpfnCmSetHotSyncExePath)(szPalmHotSyncInstallDir);
    
    // this registry key is set by the RestoreHotSyncSettings to point incorrectly to Mozilla dir
    // this should point to the Palm directory to enable sync with Palm Desktop.
#if 0
    HKEY key;
    LONG rc = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\U.S. Robotics\\Pilot Desktop\\Core",
                                0, KEY_ALL_ACCESS, &key);
    if(rc == ERROR_SUCCESS)
        ::RegSetValueEx(key, "Path", 0, REG_SZ, (const BYTE *) szPalmDesktopDir, desktopSize);
 
    if(rc == ERROR_SUCCESS)
        ::RegSetValueEx(key, "HotSyncPath", 0, REG_SZ, (const BYTE *) szPalmHotSyncInstallDir, installSize);
#endif
    // Re-start HotSync if it was running before
    if( bHotSyncRunning )
        StartHotSync(hHsapiDLL);
        
    if( dwReturnCode < 0 ) 
        return dwReturnCode;
    else 
        return(0);
}

