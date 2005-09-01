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
 * The Original Code is Mozilla libXUL embedding.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Mozilla Foundation. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsXULAppAPI.h"

#include <stdlib.h>

#include "nsIAppStartupNotifier.h"
#include "nsIDirectoryService.h"
#include "nsIEventQueueService.h"
#include "nsILocalFile.h"
#include "nsIToolkitChromeRegistry.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsArrayEnumerator.h"
#include "nsCOMArray.h"
#include "nsDirectoryServiceDefs.h"
#include "nsEnumeratorUtils.h"
#include "nsStaticComponents.h"
#include "nsString.h"

class nsEmbeddingDirProvider : public nsIDirectoryServiceProvider2
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER
  NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

  nsEmbeddingDirProvider(nsILocalFile* aGREDir,
                         nsILocalFile* aAppDir,
                         nsIDirectoryServiceProvider* aAppProvider) :
    mGREDir(aGREDir),
    mAppDir(aAppDir),
    mAppProvider(aAppProvider) { }

private:
  nsCOMPtr<nsILocalFile> mGREDir;
  nsCOMPtr<nsILocalFile> mAppDir;
  nsCOMPtr<nsIDirectoryServiceProvider> mAppProvider;
};

NS_IMPL_ISUPPORTS2(nsEmbeddingDirProvider,
                   nsIDirectoryServiceProvider,
                   nsIDirectoryServiceProvider2)

NS_IMETHODIMP
nsEmbeddingDirProvider::GetFile(const char *aProperty, PRBool *aPersistent,
                                nsIFile* *aFile)
{
  nsresult rv;

  if (mAppProvider) {
    rv = mAppProvider->GetFile(aProperty, aPersistent, aFile);
    if (NS_SUCCEEDED(rv) && *aFile)
      return rv;
  }

  if (!strcmp(aProperty, NS_OS_CURRENT_PROCESS_DIR) ||
      !strcmp(aProperty, NS_APP_INSTALL_CLEANUP_DIR)) {
    // NOTE: this is *different* than NS_XPCOM_CURRENT_PROCESS_DIR. This points
    // to the application dir. NS_XPCOM_CURRENT_PROCESS_DIR points to the toolkit.
    return mAppDir->Clone(aFile);
  }

  if (!strcmp(aProperty, NS_GRE_DIR)) {
    return mGREDir->Clone(aFile);
  }

  if (!strcmp(aProperty, NS_APP_PREF_DEFAULTS_50_DIR))
  {
    nsCOMPtr<nsIFile> file;
    rv = mAppDir->Clone(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = file->AppendNative(NS_LITERAL_CSTRING("defaults"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = file->AppendNative(NS_LITERAL_CSTRING("pref"));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*aFile = file);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsEmbeddingDirProvider::GetFiles(const char* aProperty,
                                 nsISimpleEnumerator** aResult)
{
  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> appEnum;
  nsCOMPtr<nsIDirectoryServiceProvider2> appP2
    (do_QueryInterface(mAppProvider));
  if (appP2) {
    rv = appP2->GetFiles(aProperty, getter_AddRefs(appEnum));
    if (NS_SUCCEEDED(rv) && rv != NS_SUCCESS_AGGREGATE_RESULT) {
      NS_ADDREF(*aResult = appEnum);
      return NS_OK;
    }
  }

  nsCOMArray<nsIFile> dirs;

  if (!strcmp(aProperty, NS_CHROME_MANIFESTS_FILE_LIST) ||
      !strcmp(aProperty, NS_APP_CHROME_DIR_LIST)) {
    nsCOMPtr<nsIFile> manifest;
    mGREDir->Clone(getter_AddRefs(manifest));
    manifest->AppendNative(NS_LITERAL_CSTRING("chrome"));
    dirs.AppendObject(manifest);

    mAppDir->Clone(getter_AddRefs(manifest));
    manifest->AppendNative(NS_LITERAL_CSTRING("chrome"));
    dirs.AppendObject(manifest);
  }

  if (dirs.Count()) {
    nsCOMPtr<nsISimpleEnumerator> thisEnum;
    rv = NS_NewArrayEnumerator(getter_AddRefs(thisEnum), dirs);
    NS_ENSURE_SUCCESS(rv, rv);

    if (appEnum) {
      return NS_NewUnionEnumerator(aResult, appEnum, thisEnum);
    }

    NS_ADDREF(*aResult = thisEnum);
    return NS_OK;
  }

  if (appEnum) {
    NS_ADDREF(*aResult = appEnum);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

void
XRE_GetStaticComponents(nsStaticModuleInfo const **aStaticComponents,
                        PRUint32 *aComponentCount)
{
  *aStaticComponents = kPStaticModules;
  *aComponentCount = kStaticModuleCount;
}

static nsStaticModuleInfo *sCombined;
static PRInt32 sInitCounter;

nsresult
XRE_InitEmbedding(nsILocalFile *aLibXULDirectory,
                  nsILocalFile *aAppDirectory,
                  nsIDirectoryServiceProvider *aAppDirProvider,
                  nsStaticModuleInfo const *aStaticComponents,
                  PRUint32 aStaticComponentCount)
{
  if (++sInitCounter > 1)
    return NS_OK;

  NS_ENSURE_ARG(aLibXULDirectory);
  NS_ENSURE_ARG(aAppDirectory);

  nsresult rv;

  nsCOMPtr<nsIDirectoryServiceProvider> dirSvc
    (new nsEmbeddingDirProvider(aLibXULDirectory,
                                aAppDirectory,
                                aAppDirProvider));
  if (!dirSvc)
    return NS_ERROR_OUT_OF_MEMORY;

  // Combine the toolkit static components and the app components.
  PRUint32 combinedCount = kStaticModuleCount + aStaticComponentCount;

  sCombined = new nsStaticModuleInfo[combinedCount];
  if (!sCombined)
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy(sCombined, kPStaticModules,
         sizeof(nsStaticModuleInfo) * kStaticModuleCount);
  memcpy(sCombined + kStaticModuleCount, aStaticComponents,
         sizeof(nsStaticModuleInfo) * aStaticComponentCount);

  rv = NS_InitXPCOM3(nsnull, aAppDirectory, dirSvc,
                     sCombined, combinedCount);
  if (NS_FAILED(rv))
    return rv;

  // We do not need to autoregister components here. The CheckUpdateFile()
  // bits in NS_InitXPCOM3 check for an .autoreg file. If the app wants
  // to autoregister every time (for instance, if it's debug), it can do
  // so after we return from this function.

  nsCOMPtr<nsIEventQueueService> eventQService
    (do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;

  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIObserver> startupNotifier
    (do_CreateInstance(NS_APPSTARTUPNOTIFIER_CONTRACTID));
  if (!startupNotifier)
    return NS_ERROR_FAILURE;

  startupNotifier->Observe(nsnull, APPSTARTUP_TOPIC, nsnull);

  return NS_OK;
}

void
XRE_TermEmbedding()
{
  if (--sInitCounter != 0)
    return;

  NS_ShutdownXPCOM(nsnull);
  delete [] sCombined;
}
