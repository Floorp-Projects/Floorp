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

#include "nsAppDirectoryServiceDefs.h"
#include "nsCRT.h"
#include "nsDirectoryServiceDefs.h"
#include "nsExtensionManagerDataSource.h"
#include "nsIFileProtocolHandler.h"
#include "nsIIOService.h"
#include "nsILocalFile.h"
#include "nsIProtocolHandler.h"
#include "nsIRDFContainer.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsLiteralString.h"
#include "rdf.h"

NS_IMPL_ISUPPORTS2(nsExtensionManagerDataSource, nsIRDFDataSource, nsIRDFRemoteDataSource)

static nsIRDFService* gRDFService = nsnull;

static nsIRDFResource* gIDArc = nsnull;
static nsIRDFResource* gNameArc = nsnull;
static nsIRDFResource* gDescriptionArc = nsnull;
static nsIRDFResource* gCreatorArc = nsnull;
static nsIRDFResource* gContributorArc = nsnull;
static nsIRDFResource* gDisabledArc = nsnull;
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
static nsIRDFResource* gExtensionsRoot = nsnull;
static nsIRDFResource* gThemesRoot = nsnull;

#define EM_RDF_URI "http://www.mozilla.org/2004/em-rdf#"

#define PREF_EM_DEFAULTUPDATEURL "update.url.extensions"
#define PREF_EM_APP_ID "app.id"

///////////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource

nsExtensionManagerDataSource::nsExtensionManagerDataSource()
{
  if (!gRDFService)
    CallGetService("@mozilla.org/rdf/rdf-service;1", &gRDFService);

  gRDFService->GetResource(NS_LITERAL_CSTRING(EM_RDF_URI "name"), &gNameArc);
  gRDFService->GetResource(NS_LITERAL_CSTRING(EM_RDF_URI "version"), &gVersionArc);
  gRDFService->GetResource(NS_LITERAL_CSTRING(EM_RDF_URI "iconURL"), &gIconURLArc);
  gRDFService->GetResource(NS_LITERAL_CSTRING(EM_RDF_URI "toBeEnabled"), &gToBeEnabledArc);
  gRDFService->GetResource(NS_LITERAL_CSTRING(EM_RDF_URI "toBeDisabled"), &gToBeDisabledArc);
  gRDFService->GetResource(NS_LITERAL_CSTRING(EM_RDF_URI "toBeUninstalled"), &gToBeUninstalledArc);
  gRDFService->GetResource(NS_LITERAL_CSTRING(EM_RDF_URI "disabled"), &gDisabledArc);
  gRDFService->GetResource(NS_LITERAL_CSTRING(EM_RDF_URI "installLocation"), &gInstallLocationArc);

  gRDFService->GetLiteral(NS_LITERAL_STRING("profile").get(), &gInstallProfile);
  gRDFService->GetLiteral(NS_LITERAL_STRING("global").get(), &gInstallGlobal);
  gRDFService->GetLiteral(NS_LITERAL_STRING("true").get(), &gTrueValue);
  gRDFService->GetLiteral(NS_LITERAL_STRING("false").get(), &gFalseValue);

  gRDFService->GetResource(NS_LITERAL_CSTRING("urn:mozilla:extension:root"), &gExtensionsRoot);
  gRDFService->GetResource(NS_LITERAL_CSTRING("urn:mozilla:themes:root"), &gThemesRoot);
}

nsExtensionManagerDataSource::~nsExtensionManagerDataSource()
{
  NS_IF_RELEASE(gRDFService);
  NS_IF_RELEASE(gIDArc);
  NS_IF_RELEASE(gNameArc);
  NS_IF_RELEASE(gDescriptionArc);
  NS_IF_RELEASE(gCreatorArc);
  NS_IF_RELEASE(gContributorArc);
  NS_IF_RELEASE(gDisabledArc);
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
  NS_IF_RELEASE(gExtensionsRoot);
  NS_IF_RELEASE(gThemesRoot);
}

///////////////////////////////////////////////////////////////////////////////
//
nsresult
nsExtensionManagerDataSource::LoadExtensions(PRBool aProfile)
{
  nsCOMPtr<nsIFile> extensionsFile;
  NS_GetSpecialDirectory(aProfile ? NS_APP_USER_PROFILE_50_DIR : 
                                    NS_XPCOM_CURRENT_PROCESS_DIR, 
                         getter_AddRefs(extensionsFile));
  extensionsFile->AppendNative(NS_LITERAL_CSTRING("Extensions"));
  extensionsFile->AppendNative(NS_LITERAL_CSTRING("extensions.rdf"));

  PRBool exists;
  extensionsFile->Exists(&exists);
  if (!exists)
    return NS_OK;

  nsCOMPtr<nsIIOService> ioService(do_GetService("@mozilla.org/network/io-service;1"));
  nsCOMPtr<nsIProtocolHandler> ph;
  ioService->GetProtocolHandler("file", getter_AddRefs(ph));
  nsCOMPtr<nsIFileProtocolHandler> fph(do_QueryInterface(ph));

  nsCAutoString dsURL;
  fph->GetURLSpecFromFile(extensionsFile, dsURL);

  nsCOMPtr<nsIRDFDataSource> ds;
  gRDFService->GetDataSourceBlocking(dsURL.get(), getter_AddRefs(ds));

  if (aProfile) {
    mProfileExtensions = ds;

    if (!mComposite)
      mComposite = do_CreateInstance("@mozilla.org/rdf/datasource;1?name=composite-datasource");
  
    if (mAppExtensions)
      mComposite->RemoveDataSource(mAppExtensions);
    mComposite->AddDataSource(mProfileExtensions);
    if (mAppExtensions)
      mComposite->AddDataSource(mAppExtensions);
  }
  else {
    mAppExtensions = ds;

    if (!mComposite)
      mComposite = do_CreateInstance("@mozilla.org/rdf/datasource;1?name=composite-datasource");
    mComposite->AddDataSource(mAppExtensions);
  }

  return NS_OK;
}

#define SET_PROPERTY(datasource, source, property, value) \
    rv |= datasource->GetTarget(source, property, PR_FALSE, \
                                getter_AddRefs(oldNode)); \
    if (oldNode) \
      rv |= datasource->Change(source, property, oldNode, value); \
    else \
      rv |= datasource->Assert(source, property, value, PR_FALSE);

nsresult
nsExtensionManagerDataSource::InstallExtension(nsIRDFDataSource* aSourceDataSource,
                                               PRBool aProfile)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIRDFDataSource> targetDS = aProfile ? mProfileExtensions : mAppExtensions;

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
    SET_PROPERTY(targetDS, extension, resources[i], newNode)
  }

  nsCOMPtr<nsISimpleEnumerator> e;
  nsIRDFResource* resources2[] = { gCreatorArc, gContributorArc, gRequiresArc, 
                                  gPackageArc, gSkinArc, gLanguageArc };
  for (i = 0; i < 6; ++i) {
    // Remove all old values for the extension
    rv |= targetDS->GetTargets(extension, resources2[i], PR_FALSE,
                               getter_AddRefs(e));
    do {
      PRBool hasMore;
      e->HasMoreElements(&hasMore);
      if (!hasMore)
        break;
      nsCOMPtr<nsIRDFNode> node;
      e->GetNext(getter_AddRefs(node));
      rv |= targetDS->Unassert(extension, resources2[i], node);
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
      rv |= targetDS->Assert(extension, resources2[i], node, PR_FALSE);
    }
    while (1); 
  }

  // Now that we've migrated data from the extensions manifest we need
  // to set a flag to trigger chrome registration on the next restart.
  SET_PROPERTY(targetDS, extension, gToBeEnabledArc, gTrueValue)

  // Write out the extensions datasource
  rv |= Flush(aProfile);

  return rv;
}

nsresult
nsExtensionManagerDataSource::SetExtensionProperty(const char* aExtensionID, 
                                                   nsIRDFResource* aPropertyArc, 
                                                   nsIRDFNode* aPropertyValue)
{
  nsresult rv = NS_OK;

  nsCString resourceURI = NS_LITERAL_CSTRING("urn:mozilla:extension:");
  resourceURI += aExtensionID;

  nsCOMPtr<nsIRDFResource> extension;
  gRDFService->GetResource(resourceURI, getter_AddRefs(extension));

  nsCOMPtr<nsIRDFNode> installLocation;
  rv |= GetTarget(extension, gInstallLocationArc, PR_TRUE, 
                  getter_AddRefs(installLocation));

  PRBool isProfile;
  installLocation->EqualsNode(gInstallProfile, &isProfile);
  nsCOMPtr<nsIRDFDataSource> ds = isProfile ? mProfileExtensions : mAppExtensions;

  nsCOMPtr<nsIRDFNode> oldNode;
  ds->GetTarget(extension, aPropertyArc, PR_FALSE, getter_AddRefs(oldNode));
  
  SET_PROPERTY(ds, extension, aPropertyArc, aPropertyValue)
  if (NS_FAILED(rv) && rv != NS_RDF_NO_VALUE)
    return rv;

  // Write out the extensions datasource
  return Flush(isProfile);
}

nsresult
nsExtensionManagerDataSource::EnableExtension(const char* aExtensionID)
{
  return SetExtensionProperty(aExtensionID, gToBeEnabledArc, gTrueValue) |
         SetExtensionProperty(aExtensionID, gDisabledArc, gFalseValue);
}

nsresult
nsExtensionManagerDataSource::DisableExtension(const char* aExtensionID)
{
  return SetExtensionProperty(aExtensionID, gToBeDisabledArc, gTrueValue) | 
         SetExtensionProperty(aExtensionID, gDisabledArc, gTrueValue);
}

nsresult
nsExtensionManagerDataSource::UninstallExtension(const char* aExtensionID)
{
  nsCOMPtr<nsIRDFContainer> ctr(do_CreateInstance("@mozilla.org/rdf/container;1"));
  ctr->Init(this, gExtensionsRoot);

  nsCString resourceURI = NS_LITERAL_CSTRING("urn:mozilla:extension:");
  resourceURI += aExtensionID;

  nsCOMPtr<nsIRDFResource> extension;
  gRDFService->GetResource(resourceURI, getter_AddRefs(extension));

  ctr->RemoveElement(extension, PR_TRUE);

  return SetExtensionProperty(aExtensionID, gToBeUninstalledArc, gTrueValue);
}

// This returns an array of URIs in the form:
// http://www.foo.com/software/ext1/update.cgi
// http://www.bar.com/update.php?ext=%7B0e4007ea-0b6b-4487-8e1b-479f65599e74%7D
// http://update.mozilla.org/genericUpdate.php?ext=%7B0e4007ea-0b6b-4487-8e1b-479f65599e74%7D%2C%7B0e4007ea-0b6b-4487-8e1b-479f65599e74%7D&app=%7Bec8030f7-c20a-464f-9b0e-13a3a9e97384%7D
// (specific URLs first, followed by a generic 
nsresult
nsExtensionManagerDataSource::GetConsolidatedUpdateURLs(nsISupportsArray* aURIs)
{
  nsCOMPtr<nsIRDFContainer> ctr(do_CreateInstance("@mozilla.org/rdf/container;1"));
  ctr->Init(this, gExtensionsRoot);

  nsCOMPtr<nsISimpleEnumerator> e;
  ctr->GetElements(getter_AddRefs(e));

  do {
    e->HasMoreElements(&hasMore);
    if (!hasMore)
      break;

    nsCOMPtr<nsIRDFResource> res;
    e->GetNext(getter_AddRefs(res));

    nsCAutoString url;
    GetUpdateURLForExtensionInternal(res, url);
    
    // If there's no specific Update URL specified for this extension, we want to
    // add this extension to the list that we'll pass to the generic update server.
    if (url.IsEmpty())  {
      
    }


  }
  while (PR_TRUE);
}

nsresult
nsExtensionManagerDataSource::GetUpdateURLForExtension(const char* aExtensionID, PRUnichar* aResult)
{
  nsCString resourceURI = NS_LITERAL_CSTRING("urn:mozilla:extension:");
  resourceURI += aExtensionID;

  nsCOMPtr<nsIRDFResource> extension;
  gRDFService->GetResource(resourceURI, getter_AddRefs(extension));

  nsCAutoString url;
  GetUpdateURLForExtensionInternal(extension, url);

}

void
nsExtensionManagerDataSource::GetUpdateURLForExtensionInternal(nsIRDFResource* aResource, nsACString& aResult)
{
  PRBool hasUpdateURLArc;
  mDataSource->HasArcOut(extension, gUpdateURLArc, &hasUpdateURLArc);
  if (hasUpdateURLArc) {
    nsCOMPtr<nsIRDFNode> updateURL;
    mDataSource->GetTarget(extension, gUpdateURLArc, PR_TRUE, getter_AddRefs(updateURL));

    nsCOMPtr<nsIRDFResource> updateURLResource(do_QueryInterface(updateURL));

    if (updateURLResource) {
      nsXPIDLCString value;
      updateURLResource->GetValueConst(getter_Shares(value));

      nsXPIDLCString app;
      nsCOMPtr<nsIPrefBranch> prefSvc(do_GetService(NS_PREFSERVICE_CONTRACTID));
      prefSvc->GetCharPref(PREF_EM_APP_ID, getter_Copies(app));
      
      nsXPIDLCString id;
      extension->GetValue(getter_Copies(id));
      const nsACString& extensionPrefix = NS_LITERAL_CSTRING("urn:mozilla:extension:");
      PRInt32 prefixLength = extensionPrefix.Length();
      id.Cut(prefixLength, id.Length() - prefixLength);

      ReplaceAll(value, NS_LITERAL_CSTRING("%APP%"), app);
      ReplaceAll(value, NS_LITERAL_CSTRING("%ITEM%"), id);

      aResult = value;
    }
  }
}

void
nsExtensionManagerDataSource::ReplaceAll(nsACString& aString, const nsACString& aKey, const nsACString& aReplace)
{
  nsACString::const_iterator startString, startSubstring, end;
  aString.BeginReading(startString);
  aString.BeginReading(startSubstring);
  aString.EndReading(end);

  while (FindInReadable(aKey, startSubstring, end))
    aString.Replace(Distance(startString, startSubstring), aKey.Length(), aReplace);
}

///////////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource

NS_IMETHODIMP 
nsExtensionManagerDataSource::GetURI(char** aURI)
{
  *aURI = nsCRT::strdup("rdf:extensions");
	if (!*aURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::GetSource(nsIRDFResource* aProperty, 
                                        nsIRDFNode* aTarget, 
                                        PRBool aTruthValue, 
                                        nsIRDFResource** aResult)
{
  return mComposite->GetSource(aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::GetSources(nsIRDFResource* aProperty, 
                                         nsIRDFNode* aTarget, 
                                         PRBool aTruthValue, 
                                         nsISimpleEnumerator** aResult)
{
  return mComposite->GetSources(aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::GetTarget(nsIRDFResource* aSource, 
                                        nsIRDFResource* aProperty, 
                                        PRBool aTruthValue, 
                                        nsIRDFNode** aResult)
{
  nsresult rv = NS_OK;

  if (aProperty == gIconURLArc) {
    PRBool hasIconURLArc;
    rv = mComposite->HasArcOut(aSource, aProperty, &hasIconURLArc);
    if (NS_FAILED(rv)) return rv;

    // If the download entry doesn't have a IconURL property, use a
    // generic icon URL instead.
    if (!hasIconURLArc) {
      nsCOMPtr<nsIRDFResource> res;
      gRDFService->GetResource(NS_LITERAL_CSTRING("chrome://mozapps/skin/xpinstall/xpinstallItemGeneric.png"),
                               getter_AddRefs(res));

      *aResult = res;
      NS_IF_ADDREF(*aResult);
    }
  }
  else if (aProperty == gInstallLocationArc) {
    PRBool hasNameArc, hasVersionArc;
    rv |= mProfileExtensions->HasArcOut(aSource, gNameArc, &hasNameArc);
    rv |= mProfileExtensions->HasArcOut(aSource, gVersionArc, &hasVersionArc);

    *aResult = (hasNameArc && hasVersionArc) ? gInstallProfile : gInstallGlobal;
    NS_IF_ADDREF(*aResult);
  }

  if (!*aResult)
    rv |= mComposite->GetTarget(aSource, aProperty, aTruthValue, aResult);

  return rv;
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::GetTargets(nsIRDFResource* aSource, 
                                         nsIRDFResource* aProperty, 
                                         PRBool aTruthValue, 
                                         nsISimpleEnumerator** aResult)
{
  return mComposite->GetTargets(aSource, aProperty, aTruthValue, aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::Assert(nsIRDFResource* aSource, 
                                     nsIRDFResource* aProperty, 
                                     nsIRDFNode* aTarget, 
                                     PRBool aTruthValue)
{
  return mComposite->Assert(aSource, aProperty, aTarget, aTruthValue);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::Unassert(nsIRDFResource* aSource, 
                                       nsIRDFResource* aProperty, 
                                       nsIRDFNode* aTarget)
{
  return mComposite->Unassert(aSource, aProperty, aTarget);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::Change(nsIRDFResource* aSource, 
                                     nsIRDFResource* aProperty, 
                                     nsIRDFNode* aOldTarget, 
                                     nsIRDFNode* aNewTarget)
{
  return mComposite->Change(aSource, aProperty, aOldTarget, aNewTarget);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::Move(nsIRDFResource* aOldSource, 
                                   nsIRDFResource* aNewSource, 
                                   nsIRDFResource* aProperty, 
                                   nsIRDFNode* aTarget)
{
  return mComposite->Move(aOldSource, aNewSource, aProperty, aTarget);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::HasAssertion(nsIRDFResource* aSource, 
                                           nsIRDFResource* aProperty, 
                                           nsIRDFNode* aTarget, 
                                           PRBool aTruthValue, 
                                           PRBool* aResult)
{
  return mComposite->HasAssertion(aSource, aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::AddObserver(nsIRDFObserver* aObserver)
{
  return mComposite->AddObserver(aObserver);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::RemoveObserver(nsIRDFObserver* aObserver)
{
  return mComposite->RemoveObserver(aObserver);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::ArcLabelsIn(nsIRDFNode* aNode, 
                                          nsISimpleEnumerator** aResult)
{
  return mComposite->ArcLabelsIn(aNode, aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::ArcLabelsOut(nsIRDFResource* aSource, 
                                           nsISimpleEnumerator** aResult)
{
  return mComposite->ArcLabelsOut(aSource, aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::GetAllResources(nsISimpleEnumerator** aResult)
{
  return mComposite->GetAllResources(aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::IsCommandEnabled(nsISupportsArray* aSources, 
                                               nsIRDFResource* aCommand, 
                                               nsISupportsArray* aArguments, 
                                               PRBool* aResult)
{
  return mComposite->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::DoCommand(nsISupportsArray* aSources, 
                                        nsIRDFResource* aCommand, 
                                        nsISupportsArray* aArguments)
{
  return mComposite->DoCommand(aSources, aCommand, aArguments);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::GetAllCmds(nsIRDFResource* aSource, 
                                         nsISimpleEnumerator** aResult)
{
  return mComposite->GetAllCmds(aSource, aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::HasArcIn(nsIRDFNode* aNode, 
                                       nsIRDFResource* aArc, 
                                       PRBool* aResult)
{
  return mComposite->HasArcIn(aNode, aArc, aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::HasArcOut(nsIRDFResource* aSource, 
                                        nsIRDFResource* aArc, 
                                        PRBool* aResult)
{
  return mComposite->HasArcOut(aSource, aArc, aResult);
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::BeginUpdateBatch()
{
  return mComposite->BeginUpdateBatch();
}

NS_IMETHODIMP 
nsExtensionManagerDataSource::EndUpdateBatch()
{
  return mComposite->EndUpdateBatch();
}

///////////////////////////////////////////////////////////////////////////////
// nsIRDFRemoteDataSource

NS_IMETHODIMP
nsExtensionManagerDataSource::GetLoaded(PRBool* aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsExtensionManagerDataSource::Init(const char* aURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsExtensionManagerDataSource::Refresh(PRBool aBlocking)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsExtensionManagerDataSource::Flush()
{
  return Flush(PR_FALSE) | Flush(PR_TRUE);
}

NS_IMETHODIMP
nsExtensionManagerDataSource::FlushTo(const char* aURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsExtensionManagerDataSource::Flush(PRBool aIsProfile)
{
  nsCOMPtr<nsIRDFRemoteDataSource> rds(do_QueryInterface(aIsProfile ? mProfileExtensions : mAppExtensions));
  return rds ? rds->Flush() : NS_OK;
}

