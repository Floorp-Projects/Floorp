/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef XP_PC

//
// This is a terrible hack which *must* go away soon!!!  
// We need some other mechanism to prime the nsComponentManager...
//
#include "../tests/viewer/nsSetupRegistry.cpp"


#include <windows.h>
#include <ole2.h>

#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "prlink.h"
#include "nsIFactory.h"
#include "nsIWebShell.h"
#include "nsString.h"
#include "plevent.h"
#include "prthread.h"
#include "private/pprthred.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"

static HMODULE g_DllInst = NULL;

#define GUID_SIZE 128

//
// Windows Registry keys and values...
//
#define WEBSHELL_GLOBAL_CONTRACTID_KEY          "nsWebShell"
#define WEBSHELL_GLOBAL_CONTRACTID_DESC         "Netscape NGLayout WebShell Component"

#define WEBSHELL_CONTRACTID_KEY                 WEBSHELL_GLOBAL_CONTRACTID_KEY ## "1.0"
#define WEBSHELL_CONTRACTID_DESC                WEBSHELL_GLOBAL_CONTRACTID_KEY ## " Version 1.0"

#define WEBSHELL_CLSID_DESC                 WEBSHELL_CONTRACTID_DESC



static GUID WebShellCID = NS_WEB_SHELL_CID;

BOOL WINAPI DllMain(HINSTANCE hDllInst,
                    DWORD fdwReason,
                    LPVOID lpvReserved)
{
    BOOL bResult = TRUE;

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
          {
            // save our instance
            g_DllInst = hDllInst;
          }
          break;

        case DLL_PROCESS_DETACH:
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        default:
            break;
  }

  return (bResult);
}

/*
 * COM entry-point for creating class factories...
 */
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    static PRBool isFirstTime = PR_TRUE;
    HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;
    nsIFactory* pFactory = NULL;
    nsresult rv;

    //
    // If this is the first time, then do the necessary global 
    // initialization....
    //
    // This initialization should really be done somewhere else, but
    // for now here is as good a place as any...
    //
    if (PR_TRUE == isFirstTime) {

      // Get dll directory
      char binpath[_MAX_PATH];
      ::GetModuleFileName(g_DllInst, binpath, _MAX_PATH);
      char *lastslash = PL_strrchr(binpath, '\\');
      if (lastslash) *lastslash = '\0';
      
      // Get existing search path
      int len = GetEnvironmentVariable("PATH", NULL, 0);
      char *newpath = (char *) PR_Malloc(sizeof(char) * (len +
                                                         PL_strlen(binpath) +
                                                         2)); // ';' + '\0'
      GetEnvironmentVariable("PATH", newpath, len + 1);
      PL_strcat(newpath, ";");
      PL_strcat(newpath, binpath);
      
      // Set new search path
      SetEnvironmentVariable("PATH", newpath);
      
      // Clean up
      PR_Free(newpath);
      
      //        PR_AttachThread(PR_USER_THREAD, PR_PRIORITY_NORMAL, NULL);
      NS_SetupRegistry();
      
      // Create the Event Queue for the UI thread...
      //
      // If an event queue already exists for the thread, then 
      // CreateThreadEventQueue(...) will fail...
      static NS_DEFINE_IID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);
      static NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);

      nsIEventQueueService* eventQService = nsnull;

      rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                        kIEventQueueServiceIID,
                                        (nsISupports **)&eventQService);
      if (NS_SUCCEEDED(rv)) {
        rv = eventQService->CreateThreadEventQueue();
        nsServiceManager::ReleaseService(kEventQueueServiceCID, eventQService);
      }


      isFirstTime = PR_FALSE;
    }

    if (WebShellCID == rclsid) {
        rv = NS_NewWebShellFactory(&pFactory);
        if (NS_OK != rv) {
            hr = E_OUTOFMEMORY;
        }
    }

    if (nsnull != pFactory) {
        // This is an evil cast, but it should be safe...
        rv = ((IUnknown*)pFactory)->QueryInterface(riid, ppv);
        if (NS_OK != rv) {
            hr = E_NOINTERFACE;
        } else {
            hr = S_OK;
        }
    }

    return hr;
}



/*
 * Helper function to register a key/sub-key in the Windows registry...
 */
void RegisterKey(char *aKey, const char *aSubKey, const char *aValue, const char* aValueName=NULL)
{
    LONG rv;
    HKEY hKey;
    char keyName[256];

    if (NULL != aSubKey) {
        PR_snprintf(keyName, sizeof(keyName), "%s\\%s", aKey, aSubKey);
    } else {
        PR_snprintf(keyName, sizeof(keyName), "%s", aKey);
    }

    rv = RegCreateKeyEx(HKEY_CLASSES_ROOT,
                        keyName,
                        0,
                        NULL,
                        REG_OPTION_NON_VOLATILE,
                        KEY_ALL_ACCESS,
                        NULL,
                        &hKey,
                        NULL);
    
    if (rv == ERROR_SUCCESS) {
        if (NULL != aValue) {
            RegSetValueEx(hKey,
                          aValueName,
                          0,
                          REG_SZ,
                          (const BYTE*)aValue,
                          strlen(aValue));
        }
        RegCloseKey(hKey);
    }
}

/*
 * Helper function to remove a key/sub-key from the Windows registry...
 */
void UnRegisterKey(char *aKey, const char *aSubKey)
{
    char keyName[256];

    if (NULL != aSubKey) {
        PR_snprintf(keyName, sizeof(keyName), "%s\\%s", aKey, aSubKey);
    } else {
        PR_snprintf(keyName, sizeof(keyName), "%s", aKey);
    }

    RegDeleteKey(HKEY_CLASSES_ROOT, keyName);
}



/*
 * COM entry-point to register all COM Components for the DLL
 * in the Windows registry...
 *
 * Typically this entry-point is called by regsvr32.exe or an
 * installer...
 */
STDAPI DllRegisterServer(void)
{
    char*     WebShellCLSID;
    char      WebShellCLSIDkey[255];
    char      WebShellDLLPath[MAX_PATH];

    //
    // Create a printable string from the WebShell CLSID
    //
    // This is a hack to convert the Unicode string returned by 
    // StringFromIID(...) into an ansi string...
    //
    PRUnichar IIDString[255];
    nsString tmp;

    StringFromGUID2(WebShellCID, IIDString, sizeof(IIDString));
    tmp = IIDString;
    WebShellCLSID = tmp.ToNewCString();
    
    // end hack...

    PR_snprintf(WebShellCLSIDkey, sizeof(WebShellCLSIDkey), "CLSID\\%s", WebShellCLSID);


    // Obtain the path to this module's executable file for later use.
    GetModuleFileName(g_DllInst, WebShellDLLPath, sizeof(WebShellDLLPath));

    //
    // Register/Create the following registry keys:
    //      nsWebShell1.0
    //      nsWebShell1.0/CLSID
    //
    RegisterKey(WEBSHELL_CONTRACTID_KEY, NULL,    WEBSHELL_CONTRACTID_DESC);
    RegisterKey(WEBSHELL_CONTRACTID_KEY, "CLSID", WebShellCLSID);

    //
    // Register/Create the following registry keys:
    //      nsWebShell
    //      nsWebShell/CurVer
    //      nsWebShell/CLSID
    //
    RegisterKey(WEBSHELL_GLOBAL_CONTRACTID_KEY, NULL,     WEBSHELL_GLOBAL_CONTRACTID_DESC);
    RegisterKey(WEBSHELL_GLOBAL_CONTRACTID_KEY, "CurVer", WEBSHELL_CONTRACTID_KEY);
    RegisterKey(WEBSHELL_GLOBAL_CONTRACTID_KEY, "CLSID",  WebShellCLSID);

    //
    // Register/Create the following registry keys:
    //      CLSID/{ CLSID }
    //      CLSID/{ CLSID }/ContractID
    //      CLSID/{ CLSID }/VersionIndependentContractID
    //      CLSID/{ CLSID }/NotInsertable
    //      CLSID/{ CLSID }/InprocServer32
    //
    RegisterKey(WebShellCLSIDkey, NULL,                       WEBSHELL_CLSID_DESC);
    RegisterKey(WebShellCLSIDkey, "ContractID",                   WEBSHELL_CONTRACTID_KEY);
    RegisterKey(WebShellCLSIDkey, "VersionIndependentContractID", WEBSHELL_GLOBAL_CONTRACTID_KEY);
    RegisterKey(WebShellCLSIDkey, "NotInsertable",            NULL);
    RegisterKey(WebShellCLSIDkey, "InprocServer32",           WebShellDLLPath);
    RegisterKey(WebShellCLSIDkey, "InprocServer32",           "Apartment", "ThreadingModel");

    // Free up memory...
    if (WebShellCLSID) {
        delete[] WebShellCLSID;
    }

    return NOERROR;
}


/*
 * COM entry-point to remove  all COM Components for the DLL
 * from the Windows registry...
 *
 * Typically this entry-point is called by regsvr32.exe /u or an
 * installer...
 */
STDAPI DllUnregisterServer(void)
{
    char*     WebShellCLSID;
    char      WebShellCLSIDkey[255];

    //
    // Create a printable string from the WebShell CLSID
    //
    // This is a hack to convert the Unicode string returned by 
    // StringFromIID(...) into an ansi string...
    //
    PRUnichar IIDString[255];
    nsString tmp;

    StringFromGUID2(WebShellCID, IIDString, sizeof(IIDString));
    tmp = IIDString;
    WebShellCLSID = tmp.ToNewCString();

    // end hack...

    PR_snprintf(WebShellCLSIDkey, sizeof(WebShellCLSIDkey), "CLSID\\%s", WebShellCLSID);

    //
    // Remove the following registry keys:
    //      nsWebShell1.0/CLSID
    //      nsWebShell1.0
    //
    UnRegisterKey(WEBSHELL_CONTRACTID_KEY, "CLSID");
    UnRegisterKey(WEBSHELL_CONTRACTID_KEY, NULL);

    //
    // Remove the following registry keys:
    //      nsWebShell/CLSID
    //      nsWebShell/CurVer
    //      nsWebShell
    //
    UnRegisterKey(WEBSHELL_GLOBAL_CONTRACTID_KEY, "CLSID");
    UnRegisterKey(WEBSHELL_GLOBAL_CONTRACTID_KEY, "CurVer");
    UnRegisterKey(WEBSHELL_GLOBAL_CONTRACTID_KEY, NULL);

    //
    // Remove the following registry keys:
    //      CLSID/{ CLSID }/InprocServer32
    //      CLSID/{ CLSID }/NotInsertable
    //      CLSID/{ CLSID }/VersionIndependentContractID
    //      CLSID/{ CLSID }/ContractID
    //      CLSID/{ CLSID }
    //
    UnRegisterKey(WebShellCLSIDkey, "InprocServer32");
    UnRegisterKey(WebShellCLSIDkey, "NotInsertable");
    UnRegisterKey(WebShellCLSIDkey, "VersionIndependentContractID");
    UnRegisterKey(WebShellCLSIDkey, "ContractID");
    UnRegisterKey(WebShellCLSIDkey, NULL);

    // Free up memory...
    if (WebShellCLSID) {
        delete[] WebShellCLSID;
    }

    return NOERROR;
}

#endif // XP_PC

