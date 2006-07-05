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
 * The Original Code is the Mozilla Firefox browser.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Banner <bugzilla@standard8.demon.co.uk>
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

#include "nsSuiteDirectoryProvider.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCategoryManagerUtils.h"
#include "nsXULAppAPI.h"

NS_IMPL_ISUPPORTS2(nsSuiteDirectoryProvider,
                   nsIDirectoryServiceProvider,
                   nsIDirectoryServiceProvider2)

NS_IMETHODIMP
nsSuiteDirectoryProvider::GetFile(const char *aKey,
                                  PRBool *aPersist,
                                  nsIFile* *aResult)
{
  // NOTE: This function can be reentrant through the NS_GetSpecialDirectory
  // call, so be careful not to cause infinite recursion.
  // i.e. the check for supported files must come first.
  const char* leafName = nsnull;

  if (!strcmp(aKey, NS_APP_BOOKMARKS_50_FILE))
    leafName = "bookmarks.html";
  else if (!strcmp(aKey, NS_APP_USER_PANELS_50_FILE))
    leafName = "panels.rdf";
  else if (!strcmp(aKey, NS_APP_SEARCH_50_FILE))
    leafName = "search.rdf";
  else
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIFile> parentDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(parentDir));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFile> file;
  rv = parentDir->Clone(getter_AddRefs(file));
  if (NS_FAILED(rv))
    return rv;

  nsDependentCString leafStr(leafName);
  file->AppendNative(leafStr);

  PRBool exists;
  if (NS_SUCCEEDED(file->Exists(&exists)) && !exists)
    EnsureProfileFile(leafStr, parentDir, file);

  *aPersist = PR_TRUE;
  NS_IF_ADDREF(*aResult = file);

  return NS_OK;
}

NS_IMETHODIMP
nsSuiteDirectoryProvider::GetFiles(const char *aKey,
                                   nsISimpleEnumerator* *aResult)
{
  if (strcmp(aKey, NS_APP_SEARCH_DIR_LIST))
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIProperties> dirSvc(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISimpleEnumerator> list;
  rv = dirSvc->Get(XRE_EXTENSIONS_DIR_LIST,
                   NS_GET_IID(nsISimpleEnumerator),
                   getter_AddRefs(list));
  if (NS_FAILED(rv))
    return rv;

  *aResult = new AppendingEnumerator(list, "searchplugins");
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_SUCCESS_AGGREGATE_RESULT;
}

void
nsSuiteDirectoryProvider::EnsureProfileFile(const nsACString& aLeafName,
                                            nsIFile* aParentDir,
                                            nsIFile* aTarget)
{
  nsCOMPtr<nsIFile> defaults;
  NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR,
                         getter_AddRefs(defaults));
  if (!defaults)
    return;

  defaults->AppendNative(aLeafName);

  defaults->CopyToNative(aParentDir, aLeafName);
}

NS_IMPL_ISUPPORTS1(nsSuiteDirectoryProvider::AppendingEnumerator,
                   nsISimpleEnumerator)

NS_IMETHODIMP
nsSuiteDirectoryProvider::AppendingEnumerator::HasMoreElements(PRBool *aResult)
{
  *aResult = mNext != nsnull;
  return NS_OK;
}

void
nsSuiteDirectoryProvider::AppendingEnumerator::GetNext()
{
  // Ignore all errors

  PRBool more;
  while (NS_SUCCEEDED(mBase->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsISupports> nextSupports;
    mBase->GetNext(getter_AddRefs(nextSupports));

    mNext = do_QueryInterface(nextSupports);
    if (!mNext)
      continue;

    mNext->AppendNative(mLeafName);

    PRBool exists;
    if (NS_SUCCEEDED(mNext->Exists(&exists)) && exists)
      return;
  }

  mNext = nsnull;
}

NS_IMETHODIMP
nsSuiteDirectoryProvider::AppendingEnumerator::GetNext(nsISupports* *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (!mNext) {
    *aResult = nsnull;
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mNext);

  GetNext();

  return NS_OK;
}

nsSuiteDirectoryProvider::AppendingEnumerator::AppendingEnumerator
    (nsISimpleEnumerator* aBase, const char* const aLeafName) :
  mBase(aBase), mLeafName(aLeafName)
{
  // Initialize mNext to begin.
  GetNext();
}

NS_METHOD
nsSuiteDirectoryProvider::Register(nsIComponentManager* aCompMgr,
                                   nsIFile* aPath,
                                   const char *aLoaderStr,
                                   const char *aType,
                                   const nsModuleComponentInfo *aInfo)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan
    (do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;

  return catMan->AddCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
                                  "suite-directory-provider",
                                  NS_SUITEDIRECTORYPROVIDER_CONTRACTID,
                                  PR_TRUE, PR_TRUE, nsnull);
}


NS_METHOD
nsSuiteDirectoryProvider::Unregister(nsIComponentManager* aCompMgr,
                                     nsIFile* aPath, const char *aLoaderStr,
                                     const nsModuleComponentInfo *aInfo)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan
    (do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;

  return catMan->DeleteCategoryEntry(XPCOM_DIRECTORY_PROVIDER_CATEGORY,
                                     "suite-directory-provider", PR_TRUE);
}
