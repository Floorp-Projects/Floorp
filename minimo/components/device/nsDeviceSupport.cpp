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
 * The Original Code is Device Support
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>
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

#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#include "nsIDirectoryService.h"

#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsISimpleEnumerator.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsCategoryManagerUtils.h"
#include "nsCOMArray.h"
#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsArrayEnumerator.h"

#include "string.h"
#include "nsMemory.h"

#include "nsIDeviceSupport.h"

class nsDeviceSupport : public nsIDeviceSupport
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDEVICESUPPORT

  nsDeviceSupport();

private:
  ~nsDeviceSupport();

protected:
};

NS_IMPL_ISUPPORTS1(nsDeviceSupport, nsIDeviceSupport)

nsDeviceSupport::nsDeviceSupport()
{
}

nsDeviceSupport::~nsDeviceSupport()
{
}

NS_IMETHODIMP nsDeviceSupport::RotateScreen(PRBool aLandscapeMode)
{
#ifdef WINCE
    DEVMODE DevMode;
    memset (&DevMode, 0, sizeof (DevMode));
    DevMode.dmSize   = sizeof (DevMode);

    DevMode.dmFields = DM_DISPLAYORIENTATION;
    DevMode.dmDisplayOrientation = aLandscapeMode ? DMDO_90 : DMDO_0;
    ChangeDisplaySettingsEx(NULL, &DevMode, NULL, CDS_RESET, NULL);
#endif

    return NS_OK;
}

#ifdef WINCE
static BOOL IsSmartphone() 
{
  unsigned short platform[64];
  
  if (TRUE == SystemParametersInfo(SPI_GETPLATFORMTYPE,
                                   sizeof(platform),
                                   platform,
                                   0))
  {
    if (0 == _wcsicmp(L"Smartphone", platform)) 
    {
      return TRUE;
    }
  }
  return FALSE;   
}
#endif

NS_IMETHODIMP nsDeviceSupport::Has(const char* aProperty, char **aValue)
{
  *aValue = nsnull;

#ifdef WINCE
  if (!strcmp(aProperty, "hasSoftwareKeyboard"))
  {
    *aValue = (char*) nsMemory::Alloc(4);

    if (!IsSmartphone())
      strcpy(*aValue, "yes");
    else
      strcpy(*aValue, "no");
  }
#endif

  return NS_OK;
}

#ifdef XP_WIN
static void SetRegistryKey(HKEY root, char *key, char *set)
{
  HKEY hResult;
  LONG nResult = RegOpenKeyEx(root, key, 0, KEY_WRITE, &hResult);
  if(nResult != ERROR_SUCCESS)
    return;
  
  RegSetValueEx(hResult,
                NULL,
                0,
                REG_SZ,
                (CONST BYTE*)set,
                strlen(set));  

  RegCloseKey(hResult);
}

static void GetRegistryKey(HKEY root, char *key, char *get)
{
  HKEY hResult;
  LONG nResult = RegOpenKeyEx(root, key, 0, KEY_READ, &hResult);
  if(nResult != ERROR_SUCCESS)
    return;
  
  DWORD length = MAX_PATH;
  RegQueryValueEx(hResult,
                  NULL,
                  0,
                  NULL,
                  (BYTE*)get,
                  (LPDWORD)&length);  

  RegCloseKey(hResult);
}

#endif

NS_IMETHODIMP nsDeviceSupport::IsDefaultBrowser(PRBool *_retval)
{
  *_retval = PR_FALSE;

#ifdef XP_WIN

  char buffer[MAX_PATH];
  GetRegistryKey(HKEY_CLASSES_ROOT, "http\\Shell\\Open\\Command", (char*)&buffer);

  if (strstr( buffer, "minimo_runner"))
    *_retval = PR_TRUE;
  
#endif
  return NS_OK;
}

NS_IMETHODIMP nsDeviceSupport::SetDefaultBrowser()
{
#ifdef XP_WIN
  char *cp;
  char exe[MAX_PATH];
  char cmdline[MAX_PATH];
  GetModuleFileName(GetModuleHandle(NULL), exe, sizeof(exe));
  cp = strrchr(exe,'\\');
  if (cp != NULL)
  {
    cp++; // pass the \ char.
    *cp = 0;
  }

  strcpy(cmdline, "\"");
  strcat(cmdline, exe);
  strcat(cmdline, "minimo_runner.exe\" -url %1");
  
  SetRegistryKey(HKEY_CLASSES_ROOT, "http\\Shell\\Open\\Command", cmdline);
  SetRegistryKey(HKEY_CLASSES_ROOT, "https\\Shell\\Open\\Command", cmdline);
  SetRegistryKey(HKEY_CLASSES_ROOT, "ftp\\Shell\\Open\\Command", cmdline);
  SetRegistryKey(HKEY_CLASSES_ROOT, "file\\Shell\\Open\\Command", cmdline);
#endif

  return NS_OK;
}

#define PREF_CHECKDEFAULTBROWSER "browser.shell.checkDefaultBrowser"

NS_IMETHODIMP nsDeviceSupport::GetShouldCheckDefaultBrowser(PRBool *aShouldCheckDefaultBrowser)
{
   nsCOMPtr<nsIPrefBranch> prefs;
   nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
   if (pserve)
     pserve->GetBranch("", getter_AddRefs(prefs));
 
   prefs->GetBoolPref(PREF_CHECKDEFAULTBROWSER, aShouldCheckDefaultBrowser);
   return NS_OK;
}

NS_IMETHODIMP nsDeviceSupport::SetShouldCheckDefaultBrowser(PRBool aShouldCheckDefaultBrowser)
{
   nsCOMPtr<nsIPrefBranch> prefs;
   nsCOMPtr<nsIPrefService> pserve(do_GetService(NS_PREFSERVICE_CONTRACTID));
   if (pserve)
     pserve->GetBranch("", getter_AddRefs(prefs));
 
   prefs->SetBoolPref(PREF_CHECKDEFAULTBROWSER, aShouldCheckDefaultBrowser);
   return NS_OK;
}

class nsStorageCardDirectoryProvider : public nsIDirectoryServiceProvider2
{

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2
};

NS_IMPL_ISUPPORTS2(nsStorageCardDirectoryProvider,
                   nsIDirectoryServiceProvider,
                   nsIDirectoryServiceProvider2)

NS_IMETHODIMP
nsStorageCardDirectoryProvider::GetFile(const char *aKey, PRBool *aPersist, nsIFile* *aResult)
{
  *aResult = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsStorageCardDirectoryProvider::GetFiles(const char *aKey, nsISimpleEnumerator* *aResult)
{

  nsresult rv;

#ifdef WINCE

#define CF_CARDS_FLAGS (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_TEMPORARY)

  if (!strcmp(aKey, "SCDirList")) 
  {
    nsCOMArray<nsIFile> directories;
    
    WIN32_FIND_DATA fd;
    HANDLE hCF = FindFirstFileA("\\*",&fd);

    if (INVALID_HANDLE_VALUE == hCF) return 0;
    
    do
    {
      if ((fd.dwFileAttributes & CF_CARDS_FLAGS) == CF_CARDS_FLAGS)
      {
        nsCOMPtr<nsILocalFile> dir = do_CreateInstance("@mozilla.org/file/local;1", &rv);

        dir->InitWithNativePath(NS_LITERAL_CSTRING("\\"));
        dir->AppendNative(nsDependentCString(fd.cFileName));

        directories.AppendObject(dir);
      }
    } while (FindNextFileA(hCF,&fd));
    
    FindClose(hCF);

    return NS_NewArrayEnumerator(aResult, directories);
  }
#endif

  return NS_ERROR_FAILURE;
}

//------------------------------------------------------------------------------
//  XPCOM REGISTRATION BELOW
//------------------------------------------------------------------------------

static char const kDirectoryProviderContractID[] = "@mozilla.org/device/directory-provider;1";

static NS_METHOD
RegisterDirectoryProvider(nsIComponentManager* aCompMgr,
                          nsIFile* aPath, const char *aLoaderStr,
                          const char *aType,
                          const nsModuleComponentInfo *aInfo)
{
  nsresult rv;
  
  nsCOMPtr<nsICategoryManager> catMan = do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan)
    return NS_ERROR_FAILURE;
  
  rv = catMan->AddCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
                                "device-directory-provider",
                                kDirectoryProviderContractID,
                                PR_TRUE, PR_TRUE, nsnull);
  return rv;
}


static NS_METHOD
UnregisterDirectoryProvider(nsIComponentManager* aCompMgr,
                            nsIFile* aPath, const char *aLoaderStr,
                            const nsModuleComponentInfo *aInfo)
{
  nsresult rv;
  
  nsCOMPtr<nsICategoryManager> catMan
    (do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  if (!catMan)
    return NS_ERROR_FAILURE;
  
  rv = catMan->DeleteCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
                                   "device-directory-provider", 
                                   PR_TRUE);
  return rv;
}


#define NS_STORAGECARDDIRECTORYPROVIDER_CID \
  { 0x268b9c42, 0x86c4, 0x4ab3, { 0x92, 0xbe, 0xfe, 0x3d, 0x5f, 0x23, 0x08, 0x0a } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsStorageCardDirectoryProvider)

#define DeviceSupport_CID \
{  0x6b60b326, 0x483e, 0x47d6, {0x82, 0x87, 0x88, 0x1a, 0xd9, 0x51, 0x0c, 0x8f} }

#define DeviceSupport_ContractID "@mozilla.org/device/support;1"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceSupport)

static const nsModuleComponentInfo components[] =
{
  { "Device Service", 
    DeviceSupport_CID, 
    DeviceSupport_ContractID,
    nsDeviceSupportConstructor,
  },

  {
    "nsStorageCardDirectoryProvider",
    NS_STORAGECARDDIRECTORYPROVIDER_CID,
    kDirectoryProviderContractID,
    nsStorageCardDirectoryProviderConstructor,
    //    RegisterDirectoryProvider,
    //    UnregisterDirectoryProvider
  }

};

NS_IMPL_NSGETMODULE(DeviceSupportModule, components)
