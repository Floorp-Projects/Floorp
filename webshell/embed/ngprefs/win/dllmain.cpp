/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
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

#include <ole2.h>
#include <comcat.h>
#include <olectl.h>
#include <tchar.h>
#include "factory.h"
#include "plstr.h"
#include "prmem.h"

#pragma data_seg(".text")
#define INITGUID
#include <initguid.h>
#include <shlguid.h>
#include "ngprefs.h"
#pragma data_seg()

extern "C" BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID);
BOOL RegisterServer(CLSID, LPTSTR, BOOL);

HINSTANCE   g_hInst;
UINT        g_DllRefCount;

extern "C" BOOL WINAPI DllMain(  HINSTANCE hInstance, 
                                 DWORD dwReason, 
                                 LPVOID lpReserved)
{
    switch(dwReason)
    {
    case DLL_PROCESS_ATTACH: {
        g_hInst = hInstance;
        // Get dll directory
        char binpath[_MAX_PATH];
        ::GetModuleFileName(g_hInst, binpath, _MAX_PATH);
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
    }
    break;
      
    case DLL_PROCESS_DETACH:
      break;
    }
    
    return TRUE;
}                                 

STDAPI DllCanUnloadNow(void)
{
    return (g_DllRefCount ? S_FALSE : S_OK);
}

STDAPI DllGetClassObject(  REFCLSID rclsid, 
                           REFIID riid, 
                           LPVOID *ppReturn)
{
    *ppReturn = NULL;

    //if we don't support this classid, return the proper error code
    if(!IsEqualCLSID(rclsid, CLSID_NGLayoutPrefs))
        return CLASS_E_CLASSNOTAVAILABLE;
   
    //create a CClassFactory object and check it for validity
    CClassFactory *pClassFactory = new CClassFactory(rclsid);
    if(NULL == pClassFactory)
       return E_OUTOFMEMORY;
   
    //get the QueryInterface return for our return value
    HRESULT hResult = pClassFactory->QueryInterface(riid, ppReturn);

    //call Release to decement the ref count - creating the object set it to one 
    //and QueryInterface incremented it - since its being used externally (not by 
    //us), we only want the ref count to be 1
    pClassFactory->Release();

    //return the result from QueryInterface
    return hResult;
}

STDAPI DllRegisterServer(void)
{
    //Register the explorer bar object.
    if(!RegisterServer(CLSID_NGLayoutPrefs, _T("NGLayoutPrefs"), TRUE))
        return SELFREG_E_CLASS;
    
    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    //Unregister the explorer bar object.
    if(!RegisterServer(CLSID_NGLayoutPrefs, _T("NGLayoutPrefs"), FALSE))
        return SELFREG_E_CLASS;

    return S_OK;
}

typedef struct{
   HKEY  hRootKey;
   LPTSTR szSubKey;//TCHAR szSubKey[MAX_PATH];
   LPTSTR lpszValueName;
   LPTSTR szData;//TCHAR szData[MAX_PATH];
} DOREGSTRUCT, *LPDOREGSTRUCT;

BOOL RegisterServer(CLSID clsid, LPTSTR lpszTitle, BOOL bRegister)
{
    int      i;
    LRESULT  lResult;
    DWORD    dwDisp;
    TCHAR    szSubKey[MAX_PATH];
    TCHAR    szCLSID[MAX_PATH];
    LPWSTR   pwsz;

    //get the CLSID in string form
    StringFromIID(clsid, &pwsz);

    if(pwsz)
    {
#ifdef UNICODE
        lstrcpy(szCLSID, pwsz);
#else
        WideCharToMultiByte(CP_ACP,
                            0,
                            pwsz,
                            -1,
                            szCLSID,
                            MAX_PATH,
                            NULL,
                            NULL);
#endif

        //free the string
        LPMALLOC pMalloc;
        CoGetMalloc(1, &pMalloc);
        pMalloc->Free(pwsz);
        pMalloc->Release();
    }

    if (bRegister)
    {
        HKEY     hKey;
        TCHAR    szModule[MAX_PATH];
        DOREGSTRUCT ClsidEntries[] = {HKEY_CLASSES_ROOT,   TEXT("CLSID\\%s"),                  NULL,                   lpszTitle,
                                      HKEY_CLASSES_ROOT,   TEXT("CLSID\\%s\\InprocServer32"),  NULL,                   szModule,
                                      HKEY_CLASSES_ROOT,   TEXT("CLSID\\%s\\InprocServer32"),  TEXT("ThreadingModel"), TEXT("Apartment"),
                                      NULL,                NULL,                               NULL,                   NULL};

        //get this app's path and file name
        GetModuleFileName(g_hInst, szModule, MAX_PATH);

        //register the CLSID entries
        for(i = 0; ClsidEntries[i].hRootKey; i++)
        {
           //create the sub key string - for this case, insert the file extension
           wsprintf(szSubKey, ClsidEntries[i].szSubKey, szCLSID);

           lResult = RegCreateKeyEx(  ClsidEntries[i].hRootKey,
                                      szSubKey,
                                      0,
                                      NULL,
                                      REG_OPTION_NON_VOLATILE,
                                      KEY_WRITE,
                                      NULL,
                                      &hKey,
                                      &dwDisp);
   
           if(NOERROR == lResult)
           {
              TCHAR szData[MAX_PATH];

              //if necessary, create the value string
              wsprintf(szData, ClsidEntries[i].szData, szModule);
   
              lResult = RegSetValueEx(   hKey,
                                         ClsidEntries[i].lpszValueName,
                                         0,
                                         REG_SZ,
                                         (LPBYTE)szData,
                                         lstrlen(szData) + 1);
      
              RegCloseKey(hKey);
           }
           else
              return FALSE;
        }
    } else {
       DOREGSTRUCT ClsidEntries[] = {HKEY_CLASSES_ROOT,   TEXT("CLSID\\%s\\InprocServer32"),  NULL,                   NULL,
                                     HKEY_CLASSES_ROOT,   TEXT("CLSID\\%s\\Implemented Categories"), NULL,            NULL,
                                     HKEY_CLASSES_ROOT,   TEXT("CLSID\\%s"),                  NULL,                   NULL,
                                     NULL,                NULL,                               NULL,                   NULL};
       //unregister the CLSID entries
       for(i = 0; ClsidEntries[i].hRootKey; i++)
       {
           //create the sub key string - for this case, insert the file extension
           wsprintf(szSubKey, ClsidEntries[i].szSubKey, szCLSID);

           lResult = RegDeleteKey(ClsidEntries[i].hRootKey,
                                  szSubKey);
           if (lResult != NOERROR && lResult != ERROR_FILE_NOT_FOUND)
               return FALSE;
       }
    }

    return TRUE;
}
