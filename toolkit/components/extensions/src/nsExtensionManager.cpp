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
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsExtensionManager.h"
#include "nsIAppStartupNotifier.h"
#include "nsICategoryManager.h"
#include "nsIFileProtocolHandler.h"
#include "nsIInputStream.h"
#include "nsIIOService.h"
#include "nsILocalFile.h"
#include "nsIObserverService.h"
#include "nsIProtocolHandler.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLParser.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsToolkitCompsCID.h"

NS_IMPL_ISUPPORTS2(nsExtensionManager, nsIExtensionManager, nsIObserver)

static nsIRDFService* gRDFService = nsnull;

static nsIRDFResource* gIDArc = nsnull;
static nsIRDFResource* gNameArc = nsnull;
static nsIRDFResource* gDescriptionArc = nsnull;
static nsIRDFResource* gCreatorArc = nsnull;
static nsIRDFResource* gContributorArc = nsnull;
static nsIRDFResource* gHomepageURLArc = nsnull;
static nsIRDFResource* gUpdateURLArc = nsnull;
static nsIRDFResource* gVersionArc = nsnull;
static nsIRDFResource* gTargetApplicationArc = nsnull;
static nsIRDFResource* gMinAppVersionArc = nsnull;
static nsIRDFResource* gMaxAppVersionArc = nsnull;
static nsIRDFResource* gRequiresArc = nsnull;
static nsIRDFResource* gOptionsURLArc = nsnull;
static nsIRDFResource* gAboutURLArc = nsnull;
static nsIRDFResource* gIconURLArc = nsnull;
static nsIRDFResource* gPackageArc = nsnull;
static nsIRDFResource* gSkinArc = nsnull;
static nsIRDFResource* gLanguageArc = nsnull;
static nsIRDFResource* gToBeEnabledArc = nsnull;
static nsIRDFResource* gToBeDisabledArc = nsnull;
static nsIRDFResource* gToBeUninstalledArc = nsnull;
static nsIRDFResource* gInstallLocationArc = nsnull;
static nsIRDFLiteral* gTrueValue = nsnull;
static nsIRDFLiteral* gFalseValue = nsnull;
static nsIRDFLiteral* gInstallProfile = nsnull;
static nsIRDFLiteral* gInstallGlobal = nsnull;

#define CHROME_RDF_URI "http://www.mozilla.org/rdf/chrome#"

nsExtensionManager::nsExtensionManager() { }

nsExtensionManager::~nsExtensionManager() 
{ 
  NS_IF_RELEASE(gRDFService);
  NS_IF_RELEASE(gIDArc);
  NS_IF_RELEASE(gNameArc);
  NS_IF_RELEASE(gDescriptionArc);
  NS_IF_RELEASE(gCreatorArc);
  NS_IF_RELEASE(gContributorArc);
  NS_IF_RELEASE(gHomepageURLArc);
  NS_IF_RELEASE(gUpdateURLArc);
  NS_IF_RELEASE(gVersionArc);
  NS_IF_RELEASE(gTargetApplicationArc);
  NS_IF_RELEASE(gMinAppVersionArc);
  NS_IF_RELEASE(gMaxAppVersionArc);
  NS_IF_RELEASE(gRequiresArc);
  NS_IF_RELEASE(gOptionsURLArc);
  NS_IF_RELEASE(gAboutURLArc);
  NS_IF_RELEASE(gIconURLArc);
  NS_IF_RELEASE(gPackageArc);
  NS_IF_RELEASE(gSkinArc);
  NS_IF_RELEASE(gLanguageArc);
  NS_IF_RELEASE(gToBeEnabledArc);
  NS_IF_RELEASE(gToBeDisabledArc);
  NS_IF_RELEASE(gToBeUninstalledArc);
  NS_IF_RELEASE(gInstallLocationArc);
  NS_IF_RELEASE(gTrueValue);
  NS_IF_RELEASE(gFalseValue);
  NS_IF_RELEASE(gInstallProfile);
  NS_IF_RELEASE(gInstallGlobal);
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

  return InstallExtension(ds, aUseProfile ? mProfileExtensions : mAppExtensions);
}

#define SET_PROPERTY(datasource, source, property, value) \
    rv |= datasource->GetTarget(source, property, PR_FALSE, \
                                getter_AddRefs(oldNode)); \
    if (oldNode) \
      rv |= datasource->Change(source, property, oldNode, value); \
    else \
      rv |= datasource->Assert(source, property, value, PR_FALSE);

nsresult
nsExtensionManager::InstallExtension(nsIRDFDataSource* aSourceDataSource, 
                                     nsIRDFDataSource* aTargetDataSource)
{
  nsresult rv = NS_OK;

  // Install an extension from a source manifest datasource into the specified
  // datasource (profile or target). 

  // Step 1 is to copy all the properties from the manifest to the application
  // datasource. We check for existing assertions because an extension with
  // the same id (basically, a previous version) may already exist and we need
  // to change property values. 
  nsCOMPtr<nsIRDFResource> manifest;
  gRDFService->GetResource(NS_LITERAL_CSTRING("extension:manifest"), 
                           getter_AddRefs(manifest));

  nsCOMPtr<nsIRDFNode> node;
  aSourceDataSource->GetTarget(manifest, gIDArc, PR_FALSE, getter_AddRefs(node));

  nsCOMPtr<nsIRDFLiteral> id(do_QueryInterface(node));
  nsXPIDLString val;
  id->GetValue(getter_Copies(val));

  nsCOMPtr<nsIRDFResource> extension;
  gRDFService->GetUnicodeResource(val, getter_AddRefs(extension));

  nsCOMPtr<nsIRDFNode> oldNode, newNode;
  PRInt32 i;

  nsIRDFResource* resources[] = { gNameArc, gDescriptionArc, gHomepageURLArc, 
                                  gUpdateURLArc, gVersionArc, gTargetApplicationArc,
                                  gMinAppVersionArc, gMaxAppVersionArc, 
                                  gOptionsURLArc, gAboutURLArc, gIconURLArc };
  for (i = 0; i < 11; ++i) {
    rv |= aSourceDataSource->GetTarget(manifest, resources[i], PR_FALSE,
                                       getter_AddRefs(newNode));
    SET_PROPERTY(aTargetDataSource, extension, resources[i], newNode)
  }

  nsCOMPtr<nsISimpleEnumerator> e;
  nsIRDFResource* resources2[] = { gCreatorArc, gContributorArc, gRequiresArc, 
                                  gPackageArc, gSkinArc, gLanguageArc };
  for (i = 0; i < 6; ++i) {
    // Remove all old values for the extension
    rv |= aTargetDataSource->GetTargets(extension, resources2[i], PR_FALSE,
                                        getter_AddRefs(e));
    do {
      PRBool hasMore;
      e->HasMoreElements(&hasMore);
      if (!hasMore)
        break;
      nsCOMPtr<nsIRDFNode> node;
      e->GetNext(getter_AddRefs(node));
      rv |= aTargetDataSource->Unassert(extension, resources2[i], node);
    }
    while (1);

    // Now add the new values
    rv |= aSourceDataSource->GetTargets(manifest, resources2[i], PR_FALSE,
                                  getter_AddRefs(e));
    do {
      PRBool hasMore;
      e->HasMoreElements(&hasMore);
      if (!hasMore)
        break;
      nsCOMPtr<nsIRDFNode> node;
      e->GetNext(getter_AddRefs(node));
      rv |= aTargetDataSource->Assert(extension, resources2[i], node, PR_FALSE);
    }
    while (1); 
  }

  // Now that we've migrated data from the extensions manifest we need
  // to set a flag to trigger chrome registration on the next restart.
  SET_PROPERTY(aTargetDataSource, extension, gToBeEnabledArc, gTrueValue)

  // Write out the extensions datasource
  nsCOMPtr<nsIRDFRemoteDataSource> rds(do_QueryInterface(aTargetDataSource));
  if (rds)
    rv |= rds->Flush();

  return rv;
}

nsresult
nsExtensionManager::SetExtensionProperty(const PRUnichar* aExtensionID, 
                                         nsIRDFResource* aPropertyArc, 
                                         nsIRDFNode* aPropertyValue)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIRDFResource> extension;
  gRDFService->GetUnicodeResource(nsDependentString(aExtensionID), 
                                  getter_AddRefs(extension));

  nsCOMPtr<nsIRDFNode> installLocation;
  rv |= mDataSource->GetTarget(extension, gInstallLocationArc, PR_TRUE, 
                               getter_AddRefs(installLocation));

  PRBool isProfile;
  installLocation->EqualsNode(gInstallProfile, &isProfile);
  nsCOMPtr<nsIRDFDataSource> ds = isProfile ? mProfileExtensions : mAppExtensions;

  nsCOMPtr<nsIRDFNode> oldNode;
  SET_PROPERTY(ds, extension, aPropertyArc, aPropertyValue)

  // Write out the extensions datasource
  nsCOMPtr<nsIRDFRemoteDataSource> rds(do_QueryInterface(ds));
  if (rds)
    rv |= rds->Flush();

  return rv;
}

NS_IMETHODIMP
nsExtensionManager::UninstallExtension(const PRUnichar* aExtensionID)
{
  return SetExtensionProperty(aExtensionID, gToBeUninstalledArc, gTrueValue);
}

NS_IMETHODIMP
nsExtensionManager::EnableExtension(const PRUnichar* aExtensionID)
{
  return SetExtensionProperty(aExtensionID, gToBeEnabledArc, gTrueValue);
}

NS_IMETHODIMP
nsExtensionManager::DisableExtension(const PRUnichar* aExtensionID)
{
  return SetExtensionProperty(aExtensionID, gToBeDisabledArc, gTrueValue);
}

NS_IMETHODIMP
nsExtensionManager::InstallTheme(const PRUnichar* aThemeID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExtensionManager::UninstallTheme(const PRUnichar* aThemeID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExtensionManager::EnableTheme(const PRUnichar* aThemeID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsExtensionManager::DisableTheme(const PRUnichar* aThemeID)
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

nsresult
nsExtensionManager::LoadExtensionsDataSource(nsIFile* aExtensionsDataSource,
                                             nsIRDFDataSource** aResult)
{
  if (!mDataSource)
    mDataSource = do_CreateInstance("@mozilla.org/rdf/datasource;1?name=composite-datasource");
 
  nsCOMPtr<nsIIOService> ioService(do_GetService("@mozilla.org/network/io-service;1"));
  nsCOMPtr<nsIProtocolHandler> ph;
  ioService->GetProtocolHandler("file", getter_AddRefs(ph));
  nsCOMPtr<nsIFileProtocolHandler> fph(do_QueryInterface(ph));

  nsCAutoString dsURL;
  fph->GetURLSpecFromFile(aExtensionsDataSource, dsURL);

  if (!gRDFService)
    CallGetService("@mozilla.org/rdf/rdf-service;1", &gRDFService);
  nsCOMPtr<nsIRDFDataSource> ds;
  gRDFService->GetDataSourceBlocking(dsURL.get(), getter_AddRefs(ds));

  *aResult = ds;
  NS_IF_ADDREF(*aResult);

  return mDataSource->AddDataSource(ds);
}

void
nsExtensionManager::InitLexicalResources()
{
  if (gInstallLocationArc && gInstallProfile && gInstallGlobal)
    return;

  // XXXben - this grammar is temporary and subject to formalization
  gRDFService->GetResource(NS_LITERAL_CSTRING(CHROME_RDF_URI "id"), 
                           &gIDArc);
  gRDFService->GetResource(NS_LITERAL_CSTRING(CHROME_RDF_URI "installLocation"), 
                           &gInstallLocationArc);
  gRDFService->GetLiteral(NS_LITERAL_STRING("profile").get(), &gInstallProfile);
  gRDFService->GetLiteral(NS_LITERAL_STRING("global").get(), &gInstallGlobal);
  gRDFService->GetLiteral(NS_LITERAL_STRING("true").get(), &gTrueValue);
  gRDFService->GetLiteral(NS_LITERAL_STRING("false").get(), &gFalseValue);
}

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

nsresult
nsExtensionManager::Init()
{
  // Register the profile extension launcher...
  nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1"));
  os->AddObserver(this, "profile-after-change", PR_FALSE);

  // Load global extensions
  printf("*** global extensions startup!\n");
  nsCOMPtr<nsIFile> globalExtensionsFile;
  NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(globalExtensionsFile));
  globalExtensionsFile->AppendNative(NS_LITERAL_CSTRING("extensions"));
  globalExtensionsFile->AppendNative(NS_LITERAL_CSTRING("extensions.rdf"));

  PRBool exists;
  globalExtensionsFile->Exists(&exists);
  if (exists && 
      NS_SUCCEEDED(LoadExtensionsDataSource(globalExtensionsFile, 
                                            getter_AddRefs(mAppExtensions))))
    StartExtensions(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsExtensionManager::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  if (nsCRT::strcmp(aTopic, "profile-after-change") == 0) {
    nsCOMPtr<nsIFile> profileExtensionsFile;
    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(profileExtensionsFile));
    profileExtensionsFile->AppendNative(NS_LITERAL_CSTRING("extensions"));
    profileExtensionsFile->AppendNative(NS_LITERAL_CSTRING("extensions.rdf"));

    PRBool exists;
    profileExtensionsFile->Exists(&exists);
    if (exists && 
        NS_SUCCEEDED(LoadExtensionsDataSource(profileExtensionsFile,
                                              getter_AddRefs(mProfileExtensions))))
      StartExtensions(PR_TRUE);
  }

  return NS_OK;
}