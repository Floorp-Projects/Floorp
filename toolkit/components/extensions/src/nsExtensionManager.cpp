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
 * The Original Code is the Extension Manager.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Goodger <ben@bengoodger.com>
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

#include "nsCRT.h"
#include "nsExtensionManager.h"
#include "nsExtensionManagerDataSource.h"
#include "nsIAppStartupNotifier.h"
#include "nsICategoryManager.h"
#include "nsIInputStream.h"
#include "nsIObserverService.h"
#include "nsIRDFXMLParser.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsToolkitCompsCID.h"

NS_IMPL_ISUPPORTS2(nsExtensionManager, nsIExtensionManager, nsIObserver)

nsExtensionManager::nsExtensionManager() : mDataSource(nsnull) { }

nsExtensionManager::~nsExtensionManager() 
{ 
  if (mDataSource) {
    delete mDataSource;
    mDataSource = nsnull;
  }
}

NS_IMETHODIMP
nsExtensionManager::InstallExtensionFromStream(nsIInputStream* aStream,
                                               PRBool aUseProfile)
{
  nsCOMPtr<nsIRDFXMLParser> parser(do_CreateInstance("@mozilla.org/rdf/xml-parser;1"));

  nsCOMPtr<nsIRDFDataSource> ds(do_CreateInstance("@mozilla.org/rdf/datasource;1?name=in-memory-datasource"));
  nsCOMPtr<nsIStreamListener> streamListener;
  parser->ParseAsync(ds, nsnull, getter_AddRefs(streamListener));

  PRUint32 bytesAvailable;

  do {
    aStream->Available(&bytesAvailable);
    if (!bytesAvailable) 
      break;
    streamListener->OnDataAvailable(nsnull, nsnull, aStream, 0, bytesAvailable);
  }
  while (1);

  nsresult rv = EnsureExtensionsDB();
  if (NS_FAILED(rv)) return rv;
  
  return mDataSource->InstallExtension(ds, aUseProfile);
}

NS_IMETHODIMP
nsExtensionManager::UninstallExtension(const char* aExtensionID)
{
  return mDataSource->UninstallExtension(aExtensionID);
}

NS_IMETHODIMP
nsExtensionManager::EnableExtension(const char* aExtensionID)
{
  return mDataSource->EnableExtension(aExtensionID);
}

NS_IMETHODIMP
nsExtensionManager::DisableExtension(const char* aExtensionID)
{
  return mDataSource->DisableExtension(aExtensionID);
}

NS_IMETHODIMP
nsExtensionManager::InstallTheme(const char* aThemeID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExtensionManager::UninstallTheme(const char* aThemeID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExtensionManager::EnableTheme(const char* aThemeID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExtensionManager::DisableTheme(const char* aThemeID)
{
  return NS_OK;
}

NS_METHOD
nsExtensionManager::Register(nsIComponentManager* aCompMgr,
                             nsIFile* aPath,
                             const char* aRegistryLocation,
                             const char* aComponentType,
                             const nsModuleComponentInfo* aInfo)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  catman->AddCategoryEntry(APPSTARTUP_CATEGORY,
                           "Extension Manager",
                           "service," NS_EXTENSIONMANAGER_CONTRACTID,
                           PR_TRUE,
                           PR_TRUE,
                           nsnull);
  return NS_OK;
}

#if 0
nsresult
nsExtensionManager::StartExtensions(PRBool aIsProfile)
{
  InitLexicalResources();

  nsCOMPtr<nsISimpleEnumerator> extensions;
  mDataSource->GetSources(gInstallLocationArc, 
                          aIsProfile ? gInstallProfile : gInstallGlobal, 
                          PR_TRUE, 
                          getter_AddRefs(extensions));

  do {
    PRBool hasMore;
    extensions->HasMoreElements(&hasMore);
    if (!hasMore)
      break;

    nsCOMPtr<nsIRDFResource> currExtension;
    extensions->GetNext(getter_AddRefs(currExtension));


  }
  while (1);

  return NS_OK;
/*
  walk the extensions list {
    if extension is to be uninstalled {
      locate install log
      parse log, reverting file copies, deregistering Chrome etc
      remove from extension list
    }
    if extension is to be enabled {
      register packages' overlays
      if the extension's components aren't yet registered
        register extension's components
      
      load extension's default prefs
    }
    else {
      if the extension's components have been registered
        unregister the extension's components
      
      for each package supplied by the extension
        unregister the package's overlays
    }
  
    if extension is compatible {
      clear uncompatible flag
      enable extension 
    }
    else {
      mark as being incompatible 
      disable extension
    }
*/
}
#endif

nsresult
nsExtensionManager::Init()
{
  // Register the profile extension launcher...
  nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1"));
  os->AddObserver(this, "profile-after-change", PR_FALSE);

  // Load global extensions
  printf("*** global extensions startup!\n");

  return NS_OK;
}

NS_IMETHODIMP
nsExtensionManager::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  if (nsCRT::strcmp(aTopic, "profile-after-change") == 0) {
    // Load profile extensions
    printf("*** profile extensions startup!\n");

  }

  return NS_OK;
}

NS_IMETHODIMP
nsExtensionManager::GetDatasource(nsIRDFDataSource** aResult)
{
  nsresult rv = EnsureExtensionsDB();

  *aResult = mDataSource;
  NS_IF_ADDREF(*aResult);

  return rv;
}

nsresult
nsExtensionManager::EnsureExtensionsDB()
{
  if (!mDataSource) {
    mDataSource = new nsExtensionManagerDataSource();
    if (mDataSource) {
      mDataSource->LoadExtensions(PR_FALSE);
      mDataSource->LoadExtensions(PR_TRUE);
    }
  }
  return mDataSource ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

