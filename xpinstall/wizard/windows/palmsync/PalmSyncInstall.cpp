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
 *       Rajiv Dayal <rdayal@netscape.com>
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
typedef int (WINAPI *CmSetCreatorPriorityPtr)(const char *pCreator, DWORD dwPriority);
typedef int (WINAPI *CmSetCreatorIntegratePtr)(const char *pCreator, DWORD dwIntegrate);
typedef int (WINAPI *CmSetCreatorValueDwordPtr)(const char *pCreator, TCHAR *pValue, DWORD dwValue);

// Define any HSAPI function pointer types
typedef int (WINAPI *HsCheckApiStatusPtr)(void);
typedef int (WINAPI *HsGetSyncStatusPtr)(DWORD *dwStatus);
typedef int (WINAPI *HsSetAppStatusPtr)(HsStatusType statusType, DWORD dwStartFlags);

// Define general registration fn pointer types
typedef int (WINAPI *mozDllRegisterServerPtr)(void);
typedef int (WINAPI *mozDllUnregisterServerPtr)(void);

// forward declaration
int  InstallConduit();
int UninstallConduit();
BOOL    gWasHotSyncRunning = FALSE;

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
    TCHAR appTitle[128];
    LoadString(hInstance, IDS_APP_TITLE, appTitle, MAX_LOADSTRING);

    TCHAR msgStr[256];
    int strResource=0;
    int res=-1;
    if(!strcmp(lpCmdLine,"/u")) // un-install
    {
        LoadString(hInstance, IDS_CONFIRM_UNINSTALL, msgStr, MAX_LOADSTRING);
        if (MessageBox(NULL, msgStr, appTitle, MB_YESNO) == IDYES) 
        {
            res = UninstallConduit();
            if(!res)
                res = IDS_SUCCESS_UNINSTALL;
        }
    }
    else if (!strcmp(lpCmdLine,"/us")) // silent un-install
    {
        res = UninstallConduit();
        if(!res)
            return TRUE; // success
        return res;
    }
    else if (!strcmp(lpCmdLine,"/s")) // silent install
    {
        res = InstallConduit();
        if(!res)
            return TRUE; // success
        return res;
    }
    else // install
    {
        LoadString(hInstance, IDS_CONFIRM_INSTALL, msgStr, MAX_LOADSTRING);
        if (MessageBox(NULL, msgStr, appTitle, MB_YESNO) == IDYES) 
        {
            res = InstallConduit();
            if(!res)
                res = IDS_SUCCESS_INSTALL;
        }
    }

    // if user hits no
    if(res == -1)
        return TRUE;

    if(res > IDS_ERR_MAX || res < IDS_ERR_GENERAL)
        res = IDS_ERR_GENERAL;
    LoadString(hInstance, res, msgStr, MAX_LOADSTRING);
    MessageBox(NULL, msgStr, appTitle, MB_OK);

    return TRUE;
}

// this function gets the install dir for installation
int GetPalmDesktopInstallDirectory(TCHAR *pPDInstallDirectory, unsigned long *pSize)
            {
    HKEY   key;
    // open the key
    LONG   rc = ::RegOpenKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\palm.exe", &key);
    if (rc == ERROR_SUCCESS) {
        // get key value
        rc = ::RegQueryValueEx(key, "Path", NULL, NULL, 
                               (LPBYTE)pPDInstallDirectory, pSize);
        if (rc == ERROR_SUCCESS)
            rc=0; // 0 is success for us
        // close the key
        ::RegCloseKey(key);
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

// installs our Conduit
int InstallConduit()
{ 
    int dwReturnCode;
    BOOL    bHotSyncRunning = FALSE;

    // Prepare the full path of the conduit.
    // Applications should not place conduits in the Palm Desktop directory.
    // The Palm Desktop installer only manages the Palm Desktop conduits.
    TCHAR   szConduitPath[_MAX_PATH];
    if(!GetCurrentDirectory(_MAX_PATH, szConduitPath))
        return IDS_ERR_CONDUIT_NOT_FOUND;
    // take care of any possible string overwrites
    if((strlen(szConduitPath) + strlen(DIRECTORY_SEPARATOR_STR) + strlen(CONDUIT_FILENAME)) > _MAX_PATH)
        return IDS_ERR_LOADING_CONDMGR;
    strcat(szConduitPath, DIRECTORY_SEPARATOR_STR);
    strcat(szConduitPath, CONDUIT_FILENAME);
    
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
    unsigned long size=_MAX_PATH;
    // Load the Conduit Manager DLL.
    HINSTANCE hConduitManagerDLL;
    if( (dwReturnCode=GetPalmDesktopInstallDirectory(szPalmDesktopDir, &size)) == 0 ) {
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
    CmSetCreatorPriorityPtr lpfnCmSetCreatorPriority;
    lpfnCmSetCreatorPriority = (CmSetCreatorPriorityPtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorPriority");
    CmSetCreatorIntegratePtr    lpfnCmSetCreatorIntegrate;
    lpfnCmSetCreatorIntegrate = (CmSetCreatorIntegratePtr) GetProcAddress(hConduitManagerDLL, "CmSetCreatorIntegrate");
    CmRemoveConduitByCreatorIDPtr   lpfnCmRemoveConduitByCreatorID;
    lpfnCmRemoveConduitByCreatorID = (CmRemoveConduitByCreatorIDPtr) GetProcAddress(hConduitManagerDLL, "CmRemoveConduitByCreatorID");
    
    if( (lpfnCmInstallCreator == NULL) 
        || (lpfnCmSetCreatorRemote == NULL)
        || (lpfnCmSetCreatorName == NULL)
        || (lpfnCmSetCreatorPriority == NULL)
        || (lpfnCmSetCreatorIntegrate == NULL)
        )
    {
        // Return error code.
        return(IDS_ERR_LOADING_CONDMGR);
    }
    
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
            return InstallConduit();
        }
    }
    if( dwReturnCode == 0 )
    {
        dwReturnCode = (*lpfnCmSetCreatorName)(CREATOR, szConduitPath);
        if( dwReturnCode != 0 ) return dwReturnCode;
        dwReturnCode = (*lpfnCmSetCreatorRemote)(CREATOR, REMOTE_DB);
        if( dwReturnCode != 0 ) return dwReturnCode;
        dwReturnCode = (*lpfnCmSetCreatorPriority)(CREATOR, CONDUIT_PRIORITY);
        if( dwReturnCode != 0 ) return dwReturnCode;
        // Applications should always set Integrate to 0
        dwReturnCode = (*lpfnCmSetCreatorIntegrate)(CREATOR, (DWORD)0);
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
    unsigned long size=_MAX_PATH;
    // Load the Conduit Manager DLL.
    HINSTANCE hConduitManagerDLL;
    if( (dwReturnCode=GetPalmDesktopInstallDirectory(szPalmDesktopDir, &size)) == 0 ) {
        if( (dwReturnCode = LoadConduitManagerDll(&hConduitManagerDLL, szPalmDesktopDir)) != 0 )
            // load it from local dir if present by any chance
            if( (dwReturnCode = LoadConduitManagerDll(&hConduitManagerDLL, ".")) != 0 )
                return(dwReturnCode);
    }
    else // if registery key not load it from local dir if present by any chance
        if( (dwReturnCode = LoadConduitManagerDll(&hConduitManagerDLL, ".")) != 0 )
        return(dwReturnCode);
    
    // Prepare to uninstall the conduit using Conduit Manager functions
    CmRemoveConduitByCreatorIDPtr   lpfnCmRemoveConduitByCreatorID;
    lpfnCmRemoveConduitByCreatorID = (CmRemoveConduitByCreatorIDPtr) GetProcAddress(hConduitManagerDLL, "CmRemoveConduitByCreatorID");
    if( (lpfnCmRemoveConduitByCreatorID == NULL) )
        return(IDS_ERR_LOADING_CONDMGR);
    CmRestoreHotSyncSettingsPtr lpfnCmRestoreHotSyncSettings;
    lpfnCmRestoreHotSyncSettings = (CmRestoreHotSyncSettingsPtr) GetProcAddress(hConduitManagerDLL, "CmRestoreHotSyncSettings");
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
    
    // Actually uninstall the conduit
    dwReturnCode = (*lpfnCmRemoveConduitByCreatorID)(CREATOR);
    if(dwReturnCode >= 0) {
        // uninstall Mozilla Palm Sync Support Proxy Dll
        UnregisterMozPalmSyncDll();
        dwReturnCode = (*lpfnCmRestoreHotSyncSettings)(TRUE);
    }
    
    // Re-start HotSync if it was running before
    if( bHotSyncRunning )
        StartHotSync(hHsapiDLL);
        
    if( dwReturnCode < 0 ) 
        return dwReturnCode;
    else 
        return(0);
}

