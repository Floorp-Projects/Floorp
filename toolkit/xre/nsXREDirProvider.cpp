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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>
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

#include "nsILocalFile.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXREDirProvider.h"
#ifdef XP_UNIX
#include "prenv.h"
#endif
#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#endif
#ifdef XP_BEOS
#include <StorageDefs.h>
#include <FindDirectory.h>
#endif

// WARNING: These hard coded names need to go away. They need to
// come from localizable resources

#if defined(XP_MAC) || defined(XP_MACOSX)
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("Application Registry")
#elif defined(XP_WIN) || defined(XP_OS2)
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("registry.dat")
#else
#define APP_REGISTRY_NAME NS_LITERAL_CSTRING("appreg")
#endif

nsXREDirProvider::nsXREDirProvider(const nsACString& aProductName)
{
  NS_INIT_ISUPPORTS();
#if defined(XP_UNIX) && !defined(XP_MACOSX)
  mProductDir.Assign(NS_LITERAL_CSTRING(".") + aProductName);
  ToLowerCase(mProductDir);
#else
  mProductDir.Assign(aProductName);
#endif
}

nsXREDirProvider::~nsXREDirProvider()
{
}

NS_IMPL_ISUPPORTS1(nsXREDirProvider, nsIDirectoryServiceProvider);

NS_IMETHODIMP
nsXREDirProvider::GetFile(const char* aProperty, PRBool* aPersistent,
			  nsIFile** aFile)
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> localFile;

  *aPersistent = PR_TRUE;

  if (!strcmp(aProperty, NS_APP_APPLICATION_REGISTRY_DIR))
    rv = GetProductDirectory(getter_AddRefs(localFile));
  else if (!strcmp(aProperty, NS_APP_APPLICATION_REGISTRY_FILE)) {
    rv = GetProductDirectory(getter_AddRefs(localFile));
    if (NS_SUCCEEDED(rv))
      rv = localFile->AppendNative(APP_REGISTRY_NAME);
  }
  else if (!strcmp(aProperty, NS_APP_USER_PROFILES_ROOT_DIR)) {
    rv = GetProductDirectory(getter_AddRefs(localFile));
    NS_ENSURE_SUCCESS(rv, rv);

#if !defined(XP_UNIX) || defined(XP_MACOSX)
    rv = localFile->AppendRelativeNativePath(NS_LITERAL_CSTRING("Profiles"));
    NS_ENSURE_SUCCESS(rv, rv);
#endif

    // We must create the profile directory here if it does not exist.
    rv = EnsureDirectoryExists(localFile);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (localFile)
    return CallQueryInterface(localFile, aFile);

  return NS_ERROR_FAILURE;
}

nsresult
nsXREDirProvider::GetProductDirectory(nsILocalFile** aFile)
{
  // Copied from nsAppFileLocationProvider (more or less)
  nsresult rv;
  nsCOMPtr<nsILocalFile> localDir;

  nsCOMPtr<nsIProperties> directoryService =  do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

#if defined(XP_MAC)
  OSErr err;
  long response;
  err = ::Gestalt(gestaltSystemVersion, &response);
  const char *prop = (!err && response >= 0x00001000) ? NS_MAC_USER_LIB_DIR : NS_MAC_DOCUMENTS_DIR;
  rv = directoryService->Get(prop, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
#elif defined(XP_MACOSX)
  FSRef fsRef;
  OSErr err = ::FSFindFolder(kUserDomain, kDomainLibraryFolderType, kCreateFolder, &fsRef);
  if (err) return NS_ERROR_FAILURE;
  NS_NewLocalFile(nsString(), PR_TRUE, getter_AddRefs(localDir));
  if (!localDir) return NS_ERROR_FAILURE;
  nsCOMPtr<nsILocalFileMac> localDirMac(do_QueryInterface(localDir));
  rv = localDirMac->InitWithFSRef(&fsRef);
#elif defined(XP_OS2)
  rv = directoryService->Get(NS_OS2_HOME_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
#elif defined(XP_WIN)
  PRBool exists;
  rv = directoryService->Get(NS_WIN_APPDATA_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
  if (NS_SUCCEEDED(rv))
    rv = localDir->Exists(&exists);
  if (NS_FAILED(rv) || !exists) {
    // On some Win95 machines, NS_WIN_APPDATA_DIR does not exist - revert to NS_WIN_WINDOWS_DIR
    localDir = 0;
    rv = directoryService->Get(NS_WIN_WINDOWS_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localDir));
  }
#elif defined(XP_UNIX)
  rv = NS_NewNativeLocalFile(nsDependentCString(PR_GetEnv("HOME")), PR_TRUE, getter_AddRefs(localDir));
#elif defined(XP_BEOS)
  char path[MAXPATHLEN];
  find_directory(B_USER_SETTINGS_DIRECTORY, 0, 0, path, MAXPATHLEN);
  // Need enough space to add the trailing backslash
  int len = strlen(path);
  if (len > MAXPATHLEN - 2)
    return NS_ERROR_FAILURE;
  path[len]   = '/';
  path[len+1] = '\0';
  rv = NS_NewNativeLocalFile(nsDependentCString(path), PR_TRUE, getter_AddRefs(localDir));
#else
#error dont_know_how_to_get_product_dir_on_your_platform
#endif

  NS_ENSURE_SUCCESS(rv, rv);
  rv = localDir->AppendRelativeNativePath(mProductDir);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = EnsureDirectoryExists(localDir);
  NS_ENSURE_SUCCESS(rv, rv);

  *aFile = localDir;
  NS_ADDREF(*aFile);
  return NS_OK;
}

nsresult
nsXREDirProvider::EnsureDirectoryExists(nsILocalFile* aDirectory)
{
  PRBool exists;
  nsresult rv = aDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists)
    rv = aDirectory->Create(nsIFile::DIRECTORY_TYPE, 0775);

  return rv;
}
