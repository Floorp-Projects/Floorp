/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Gagan Saksena <gagan@netscape.com>
 *   Benjamin Smedberg <bsmedberg@covad.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <string.h>
#include "nsArrayEnumerator.h"
#include "nsCOMPtr.h"
#include "nsIChromeRegistry.h"
#include "nsChromeRegistry.h"
#include "nsChromeUIDataSource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFXMLSink.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFResource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"
#include "nsNetUtil.h"
#include "nsIFileChannel.h"
#include "nsIXBLService.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMLocation.h"
#include "nsIWindowMediator.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIStyleSheet.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsIPresShell.h"
#include "nsIDocShell.h"
#include "nsISupportsArray.h"
#include "nsIDocumentObserver.h"
#include "nsIIOService.h"
#include "nsLayoutCID.h"
#include "nsIBindingManager.h"
#include "prio.h"
#include "nsInt64.h"
#include "nsIDirectoryService.h"
#include "nsILocalFile.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIObserverService.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindowCollection.h"
#include "nsIAtom.h"
#include "nsStaticAtom.h"
#include "nsNetCID.h"
#include "nsIJARURI.h"
#include "nsIFileURL.h"

static char kChromePrefix[] = "chrome://";
nsIAtom* nsChromeRegistry::sCPrefix; // atom for "c"

#define kChromeFileName           NS_LITERAL_CSTRING("chrome.rdf")
#define kInstalledChromeFileName  NS_LITERAL_CSTRING("installed-chrome.txt")

static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kCSSLoaderCID, NS_CSS_LOADER_CID);

class nsChromeRegistry;

nsIChromeRegistry* gChromeRegistry = nsnull;

#define CHROME_URI "http://www.mozilla.org/rdf/chrome#"

DEFINE_RDF_VOCAB(CHROME_URI, CHROME, selectedSkin);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, selectedLocale);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, baseURL);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, packages);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, package);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, name);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, image);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, locType);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, allowScripts);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, hasOverlays);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, hasStylesheets);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, skinVersion);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, localeVersion);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, packageVersion);
DEFINE_RDF_VOCAB(CHROME_URI, CHROME, disabled);

////////////////////////////////////////////////////////////////////////////////

nsChromeRegistry::nsChromeRegistry() : mRDFService(nsnull),
                                       mRDFContainerUtils(nsnull),
                                       mInstallInitialized(PR_FALSE),
                                       mProfileInitialized(PR_FALSE),
                                       mRuntimeProvider(PR_FALSE),
                                       mBatchInstallFlushes(PR_FALSE),
                                       mSearchedForOverride(PR_FALSE)
{
  mDataSourceTable = nsnull;
}


static PRBool PR_CALLBACK
DatasourceEnumerator(nsHashKey *aKey, void *aData, void* closure)
{
  if (!closure || !aData)
    return PR_FALSE;

  nsIRDFCompositeDataSource* compositeDS = (nsIRDFCompositeDataSource*) closure;

  nsCOMPtr<nsISupports> supports = (nsISupports*)aData;

  nsCOMPtr<nsIRDFDataSource> dataSource = do_QueryInterface(supports);
  if (!dataSource)
    return PR_FALSE;

#ifdef DEBUG
  nsresult rv =
#endif
  compositeDS->RemoveDataSource(dataSource);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to RemoveDataSource");
  return PR_TRUE;
}


nsChromeRegistry::~nsChromeRegistry()
{
  gChromeRegistry = nsnull;
  
  if (mDataSourceTable) {
      mDataSourceTable->Enumerate(DatasourceEnumerator, mChromeDataSource);
      delete mDataSourceTable;
  }

  if (mRDFService) {
    nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
    mRDFService = nsnull;
  }

  if (mRDFContainerUtils) {
    nsServiceManager::ReleaseService(kRDFContainerUtilsCID, mRDFContainerUtils);
    mRDFContainerUtils = nsnull;
  }

}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsChromeRegistry,
                              nsIChromeRegistry,
                              nsIXULChromeRegistry,
                              nsIXULOverlayProvider,
                              nsIObserver,
                              nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////////////
// nsIChromeRegistry methods:

nsresult
nsChromeRegistry::Init()
{
  // these atoms appear in almost every chrome registry manifest.rdf
  // in some form or another. making static atoms prevents the atoms
  // from constantly being created/destroyed during parsing
  
  static const nsStaticAtom atoms[] = {
    { "c",             &sCPrefix },
    { "chrome",        nsnull },
    { "NC",            nsnull },
    { "baseURL",       nsnull},
    { "allowScripts",  nsnull },
    { "skinVersion",   nsnull },
    { "package",       nsnull },
    { "packages",      nsnull },
    { "locType",       nsnull },
    { "displayName",   nsnull },
    { "author",        nsnull },
    { "localeVersion", nsnull },
    { "localeType",    nsnull },
    { "selectedLocale", nsnull },
    { "selectedSkin",  nsnull },
    { "hasOverlays",   nsnull },
    { "previewURL", nsnull },
  };

  NS_RegisterStaticAtoms(atoms, NS_ARRAY_LENGTH(atoms));
  
  gChromeRegistry = this;
  
  nsresult rv;
  rv = nsServiceManager::GetService(kRDFServiceCID,
                                    NS_GET_IID(nsIRDFService),
                                    (nsISupports**)&mRDFService);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
                                    NS_GET_IID(nsIRDFContainerUtils),
                                    (nsISupports**)&mRDFContainerUtils);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_selectedSkin),
                                getter_AddRefs(mSelectedSkin));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_selectedLocale),
                                getter_AddRefs(mSelectedLocale));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_baseURL),
                                getter_AddRefs(mBaseURL));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_packages),
                                getter_AddRefs(mPackages));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_package),
                                getter_AddRefs(mPackage));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_name),
                                getter_AddRefs(mName));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_image),
                                getter_AddRefs(mImage));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_locType),
                                getter_AddRefs(mLocType));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_allowScripts),
                                getter_AddRefs(mAllowScripts));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_hasOverlays),
                                getter_AddRefs(mHasOverlays));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_hasStylesheets),
                                getter_AddRefs(mHasStylesheets));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_skinVersion),
                                getter_AddRefs(mSkinVersion));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_localeVersion),
                                getter_AddRefs(mLocaleVersion));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_packageVersion),
                                getter_AddRefs(mPackageVersion));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  rv = mRDFService->GetResource(nsDependentCString(kURICHROME_disabled),
                                getter_AddRefs(mDisabled));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF resource");

  nsCOMPtr<nsIObserverService> observerService =
           do_GetService("@mozilla.org/observer-service;1", &rv);
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", PR_TRUE);
    observerService->AddObserver(this, "profile-after-change", PR_TRUE);
  }

  CheckForNewChrome();

  return NS_OK;
}


static nsresult
SplitURL(nsIURI *aChromeURI, nsCString& aPackage, nsCString& aProvider, nsCString& aFile,
         PRBool *aModified = nsnull)
{
  // Splits a "chrome:" URL into its package, provider, and file parts.
  // Here are the current portions of a
  // chrome: url that make up the chrome-
  //
  //     chrome://global/skin/foo?bar
  //     \------/ \----/\---/ \-----/
  //         |       |     |     |
  //         |       |     |     `-- RemainingPortion
  //         |       |     |
  //         |       |     `-- Provider
  //         |       |
  //         |       `-- Package
  //         |
  //         `-- Always "chrome://"
  //
  //

  nsresult rv;

  nsCAutoString str;
  rv = aChromeURI->GetSpec(str);
  if (NS_FAILED(rv)) return rv;

  // We only want to deal with "chrome:" URLs here. We could return
  // an error code if the URL isn't properly prefixed here...
  if (PL_strncmp(str.get(), kChromePrefix, sizeof(kChromePrefix) - 1) != 0)
    return NS_ERROR_INVALID_ARG;

  // Cull out the "package" string; e.g., "navigator"
  aPackage = str.get() + sizeof(kChromePrefix) - 1;

  PRInt32 idx;
  idx = aPackage.FindChar('/');
  if (idx < 0)
    return NS_OK;

  // Cull out the "provider" string; e.g., "content"
  aPackage.Right(aProvider, aPackage.Length() - (idx + 1));
  aPackage.Truncate(idx);

  idx = aProvider.FindChar('/');
  if (idx < 0) {
    // Force the provider to end with a '/'
    idx = aProvider.Length();
    aProvider.Append('/');
  }

  // Cull out the "file"; e.g., "navigator.xul"
  aProvider.Right(aFile, aProvider.Length() - (idx + 1));
  aProvider.Truncate(idx);

  PRBool nofile = aFile.IsEmpty();
  if (nofile) {
    // If there is no file, then construct the default file
    aFile = aPackage;

    if (aProvider.Equals("content")) {
      aFile += ".xul";
    }
    else if (aProvider.Equals("skin")) {
      aFile += ".css";
    }
    else if (aProvider.Equals("locale")) {
      aFile += ".dtd";
    }
    else {
      NS_ERROR("unknown provider");
      return NS_ERROR_FAILURE;
    }
  } else {
    // Protect against URIs containing .. that reach up out of the
    // chrome directory to grant chrome privileges to non-chrome files.
    int depth = 0;
    PRBool sawSlash = PR_TRUE;  // .. at the beginning is suspect as well as /..
    for (const char* p=aFile.get(); *p; p++) {
      if (sawSlash) {
        if (p[0] == '.' && p[1] == '.'){
          depth--;    // we have /.., decrement depth.
        } else {
          static const char escape[] = "%2E%2E";
          if (PL_strncasecmp(p, escape, sizeof(escape)-1) == 0)
            depth--;   // we have the HTML-escaped form of /.., decrement depth.
        }
      } else if (p[0] != '/') {
        depth++;        // we have /x for some x that is not /
      }
      sawSlash = (p[0] == '/');

      if (depth < 0) {
        return NS_ERROR_FAILURE;
      }
    }
  }
  if (aModified)
    *aModified = nofile;
  return NS_OK;
}

static nsresult
GetBaseURLFile(const nsACString& aBaseURL, nsIFile** aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);
  *aFile = nsnull;

  nsresult rv;
  nsCOMPtr<nsIIOService> ioServ(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> uri;
  rv = ioServ->NewURI(aBaseURL, nsnull, nsnull, getter_AddRefs(uri));
  if (NS_FAILED(rv)) return rv;

  // Loop, jar: URIs can nest (e.g. jar:jar:A.jar!B.jar!C.xml).
  // Often, however, we have jar:resource:/chrome/A.jar!C.xml.
  nsCOMPtr<nsIJARURI> jarURI;
  while ((jarURI = do_QueryInterface(uri)) != nsnull)
    jarURI->GetJARFile(getter_AddRefs(uri));

  // Here we must have a URL of the form resource:/chrome/A.jar
  // or file:/some/path/to/A.jar.
  nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
  if (fileURL) {
    nsCOMPtr<nsIFile> file;
    fileURL->GetFile(getter_AddRefs(file));
    if (file) {
      NS_ADDREF(*aFile = file);
      return NS_OK;
    }
  }
  NS_ERROR("GetBaseURLFile() failed. Remote chrome?");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsChromeRegistry::Canonify(nsIURI* aChromeURI)
{
  // Canonicalize 'chrome:' URLs. We'll take any 'chrome:' URL
  // without a filename, and change it to a URL -with- a filename;
  // e.g., "chrome://navigator/content" to
  // "chrome://navigator/content/navigator.xul".
  if (! aChromeURI)
      return NS_ERROR_NULL_POINTER;

  PRBool modified = PR_TRUE; // default is we do canonification
  nsCAutoString package, provider, file;
  nsresult rv;
  rv = SplitURL(aChromeURI, package, provider, file, &modified);
  if (NS_FAILED(rv))
    return rv;

  if (!modified)
    return NS_OK;

  nsCAutoString canonical( kChromePrefix );
  canonical += package;
  canonical += "/";
  canonical += provider;
  canonical += "/";
  canonical += file;

  return aChromeURI->SetSpec(canonical);
}

NS_IMETHODIMP
nsChromeRegistry::ConvertChromeURL(nsIURI* aChromeURL, nsACString& aResult)
{
  nsresult rv = NS_OK;
  NS_ASSERTION(aChromeURL, "null url!");
  if (!aChromeURL)
      return NS_ERROR_NULL_POINTER;

  // No need to canonify as the SplitURL() that we
  // do is the equivalent of canonification without modifying
  // aChromeURL

  // Obtain the package, provider and remaining from the URL
  nsCAutoString package, provider, remaining;

  rv = SplitURL(aChromeURL, package, provider, remaining);
  if (NS_FAILED(rv)) return rv;

  // Try for the profile data source first because it
  // will load the install data source as well.
  if (!mProfileInitialized) {
    rv = LoadProfileDataSource();
    if (NS_FAILED(rv)) return rv;
  }
  if (!mInstallInitialized) {
    rv = LoadInstallDataSource();
    if (NS_FAILED(rv)) return rv;
  }

  nsCAutoString finalURL;

  rv = GetOverrideURL(package, provider, remaining, finalURL);
  if (NS_SUCCEEDED(rv))
    return NS_OK;
  
  rv = GetBaseURL(package, provider, finalURL);
#ifdef DEBUG
  if (NS_FAILED(rv)) {
    nsCAutoString msg("chrome: failed to get base url");
    nsCAutoString url;
    rv = aChromeURL->GetSpec(url);
    if (NS_SUCCEEDED(rv)) {
      msg += " for ";
      msg += url.get();
    }
    msg += " -- using wacky default";
    NS_WARNING(msg.get());
  }
#endif
  if (finalURL.IsEmpty()) {
    // hard-coded fallback
    if (provider.Equals("skin")) {
      finalURL = "resource:/chrome/skins/classic/";
    }
    else if (provider.Equals("locale")) {
      finalURL = "resource:/chrome/locales/en-US/";
    }
    else if (package.Equals("aim")) {
      finalURL = "resource:/chrome/packages/aim/";
    }
    else if (package.Equals("messenger")) {
      finalURL = "resource:/chrome/packages/messenger/";
    }
    else if (package.Equals("global")) {
      finalURL = "resource:/chrome/packages/widget-toolkit/";
    }
    else {
      finalURL = "resource:/chrome/packages/core/";
    }
  }

  aResult = finalURL + remaining;

  return NS_OK;
}

nsresult
nsChromeRegistry::GetBaseURL(const nsACString& aPackage,
                             const nsACString& aProvider,
                             nsACString& aBaseURL)
{
  nsCOMPtr<nsIRDFResource> resource;

  nsCAutoString resourceStr("urn:mozilla:package:");
  resourceStr += aPackage;

  // Obtain the resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> packageResource;
  rv = GetResource(resourceStr, getter_AddRefs(packageResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }

  // Follow the "selectedSkin" or "selectedLocale" arc.
  nsCOMPtr<nsIRDFResource> arc;
  if (aProvider.EqualsLiteral("skin")) {
    arc = mSelectedSkin;
  }
  else if (aProvider.EqualsLiteral("locale")) {
    arc = mSelectedLocale;
  }
  else
    // We're a package.
    resource = packageResource;

  if (arc) {

    nsCOMPtr<nsIRDFNode> selectedProvider;
    if (NS_FAILED(rv = mChromeDataSource->GetTarget(packageResource, arc, PR_TRUE, getter_AddRefs(selectedProvider)))) {
      NS_ERROR("Unable to obtain the provider.");
      return rv;
    }

    resource = do_QueryInterface(selectedProvider);

    if (resource) {
      PRBool providerOK;
      rv = VerifyCompatibleProvider(packageResource, resource, arc, &providerOK);
      if (NS_FAILED(rv)) return rv;
      if (!providerOK) {
        // We had a selection but it was incompatible or not present.
        // That selection may have come from the profile, so check only
        // the part of the datasource in the install.  (If this succeeds,
        // we won't remember the choice, either, in case the user
        // switches back to a build where this theme does work.)
        if (NS_FAILED(rv = mInstallDirChromeDataSource->GetTarget(packageResource, arc, PR_TRUE, getter_AddRefs(selectedProvider)))) {
          NS_ERROR("Unable to obtain the provider.");
          return rv;
        }
        resource = do_QueryInterface(selectedProvider);
        if (resource) {
          rv = VerifyCompatibleProvider(packageResource, resource, arc, &providerOK);
          if (NS_FAILED(rv)) return rv;
          if (!providerOK) 
            selectedProvider = nsnull;
        }
      }
    }

    if (!selectedProvider) {
      // FindProvider will attempt to auto-select a version-compatible provider (skin).  If none
      // exist it will return nsnull in the selectedProvider variable.
      FindProvider(aPackage, aProvider, arc, getter_AddRefs(selectedProvider));
      resource = do_QueryInterface(selectedProvider);
    }

    if (!selectedProvider)
      return rv;

    if (!resource)
      return NS_ERROR_FAILURE;
  }

  // From this resource, follow the "baseURL" arc.
  return FollowArc(mChromeDataSource, aBaseURL, resource, mBaseURL);
}

nsresult
nsChromeRegistry::GetOverrideURL(const nsACString& aPackage,
                                 const nsACString& aProvider,
                                 const nsACString& aPath,
                                 nsACString& aResult)
{
  nsresult rv = InitOverrideJAR();
  if (NS_FAILED(rv)) return rv;

  // ok, if we get here, we have an override JAR

  aResult.SetCapacity(mOverrideJARURL.Length() +
                      aPackage.Length() +
                      aProvider.Length() +
                      aPath.Length() + 2);
  
  aResult = mOverrideJARURL;
  aResult += aPackage;
  aResult += '/';
  aResult += aProvider;
  aResult += '/';

  // skins and locales get their name tacked on, like
  // skin/modern/foo.css or
  // locale/en-US/navigator.properties
  if (aProvider.EqualsLiteral("skin") ||
      aProvider.EqualsLiteral("locale")) {

    // little hack here to get the right arc
    nsIRDFResource* providerArc;
    if (aProvider.Equals("skin"))
      providerArc = mSelectedSkin;
    else
      providerArc = mSelectedLocale;
    
    nsCAutoString selectedProvider;
    rv = GetSelectedProvider(aPackage, aProvider, providerArc, selectedProvider);

    if (NS_SUCCEEDED(rv)) {
      aResult += selectedProvider;
      aResult += '/';
    }
  }
  
  aResult += aPath;

  nsCOMPtr<nsIZipEntry> zipEntry;
  rv = mOverrideJAR->GetEntry(PromiseFlatCString(aResult).get(),
                              getter_AddRefs(zipEntry));
  if (NS_FAILED(rv)) {
    aResult.Truncate();
    return rv;
  }

  return NS_OK;
}

nsresult
nsChromeRegistry::InitOverrideJAR()
{
  // generic failure if we know there's no override
  if (mSearchedForOverride && !mOverrideJAR)
    return NS_ERROR_FAILURE;

  mSearchedForOverride = PR_TRUE;

  nsresult rv;
  //
  // look for custom.jar
  //
  nsCOMPtr<nsIFile> overrideFile;
  rv = GetInstallRoot(getter_AddRefs(overrideFile));
  if (NS_FAILED(rv)) return rv;

  rv = overrideFile->AppendNative(NS_LITERAL_CSTRING("custom.jar"));
  if (NS_FAILED(rv)) return rv;

  PRBool exists;
  rv = overrideFile->Exists(&exists);
  if (NS_FAILED(rv)) return rv;

  // ok, if the file doesn't exist, its just a generic failure
  if (!exists)
    return NS_ERROR_FAILURE;

  //
  // cache the url so we can later append
  //
  mOverrideJARURL.Assign("jar:");
  nsCAutoString jarURL;
  rv = NS_GetURLSpecFromFile(overrideFile, jarURL);
  if (NS_FAILED(rv)) return rv;

  mOverrideJARURL.Append(jarURL);
  mOverrideJARURL.Append("!/");
  if (NS_FAILED(rv)) return rv;

  //
  // also cache the zip file itself
  //
  nsCOMPtr<nsIZipReaderCache> readerCache =
    do_CreateInstance("@mozilla.org/libjar/zip-reader-cache;1", &rv);
  if (NS_FAILED(rv)) return rv;

  rv = readerCache->Init(32);
  
  rv = readerCache->GetZip(overrideFile, getter_AddRefs(mOverrideJAR));
  if (NS_FAILED(rv)) {
    mOverrideJARURL.Truncate();
    return rv;
  }
  
  return NS_OK;
}

nsresult
nsChromeRegistry::VerifyCompatibleProvider(nsIRDFResource* aPackageResource,
                                           nsIRDFResource* aProviderResource,
                                           nsIRDFResource* aArc,
                                           PRBool *aAcceptable)
{
  // We found a selected provider, but now we need to verify that the version
  // specified by the package and the version specified by the provider are
  // one and the same.  If they aren't, then we cannot use this provider.
  nsCOMPtr<nsIRDFResource> versionArc;
  if (aArc == mSelectedSkin)
    versionArc = mSkinVersion;
  else // Locale arc
    versionArc = mLocaleVersion;

  nsCAutoString packageVersion;
  nsChromeRegistry::FollowArc(mChromeDataSource, packageVersion,
                              aPackageResource, versionArc);
  if (!packageVersion.IsEmpty()) {
    // The package only wants providers (skins) that say they can work
    // with it.  Let's find out if our provider (skin) can work with it.
    nsCAutoString providerVersion;
    nsChromeRegistry::FollowArc(mChromeDataSource, providerVersion,
                                aProviderResource, versionArc);
    if (!providerVersion.Equals(packageVersion)) {
      *aAcceptable = PR_FALSE;
      return NS_OK;
    }
  }
  
  // Ensure that the provider actually exists.
  // XXX This will have to change if we handle remote chrome.
  nsCAutoString providerBaseURL;
  nsresult rv = FollowArc(mChromeDataSource, providerBaseURL,
                          aProviderResource, mBaseURL);
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIFile> baseURLFile;
  rv = GetBaseURLFile(providerBaseURL, getter_AddRefs(baseURLFile));
  if (NS_FAILED(rv))
    return rv;
  rv = baseURLFile->Exists(aAcceptable);
#if DEBUG
  if (NS_FAILED(rv) || !*aAcceptable)
    printf("BaseURL %s cannot be found.\n",
           PromiseFlatCString(providerBaseURL).get());
#endif
  return rv;
}

// locate
nsresult
nsChromeRegistry::FindProvider(const nsACString& aPackage,
                               const nsACString& aProvider,
                               nsIRDFResource *aArc,
                               nsIRDFNode **aSelectedProvider)
{
  *aSelectedProvider = nsnull;

  nsCAutoString rootStr("urn:mozilla:");
  nsresult rv = NS_OK;

  rootStr += aProvider;
  rootStr += ":root";

  // obtain the provider root resource
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(rootStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the provider root resource.");
    return rv;
  }

  // wrap it in a container
  nsCOMPtr<nsIRDFContainer> container;
  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv)) return rv;

  rv = container->Init(mChromeDataSource, resource);
  if (NS_FAILED(rv)) return rv;

  // step through its (seq) arcs
  nsCOMPtr<nsISimpleEnumerator> arcs;
  rv = container->GetElements(getter_AddRefs(arcs));
  if (NS_FAILED(rv)) return rv;

  // XXX This needs to be something other than random.  See bug 191957.
  PRBool moreElements;
  rv = arcs->HasMoreElements(&moreElements);
  if (NS_FAILED(rv)) return rv;
  for ( ; moreElements; arcs->HasMoreElements(&moreElements)) {

    // get next arc resource
    nsCOMPtr<nsISupports> supports;
    rv = arcs->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIRDFResource> kid = do_QueryInterface(supports);

    if (kid) {
      // get its name
      nsCAutoString providerName;
      rv = FollowArc(mChromeDataSource, providerName, kid, mName);
      if (NS_FAILED(rv)) return rv;

      // get its package list
      nsCOMPtr<nsIRDFNode> packageNode;
      nsCOMPtr<nsIRDFResource> packageList;
      rv = mChromeDataSource->GetTarget(kid, mPackages, PR_TRUE, getter_AddRefs(packageNode));
      if (NS_SUCCEEDED(rv))
        packageList = do_QueryInterface(packageNode);
      if (!packageList)
        continue;

      // if aPackage is named in kid's package list, select it and we're done
      rv = SelectPackageInProvider(packageList, aPackage, aProvider, providerName,
                                   aArc, aSelectedProvider);
      if (NS_FAILED(rv))
        continue; // Don't let this be disastrous.  We may find another acceptable match.

      if (*aSelectedProvider)
        return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsChromeRegistry::SelectPackageInProvider(nsIRDFResource *aPackageList,
                                          const nsACString& aPackage,
                                          const nsACString& aProvider,
                                          const nsACString& aProviderName,
                                          nsIRDFResource *aArc,
                                          nsIRDFNode **aSelectedProvider)
{
  *aSelectedProvider = nsnull;

  nsresult rv = NS_OK;

  // wrap aPackageList in a container
  nsCOMPtr<nsIRDFContainer> container;
  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_SUCCEEDED(rv))
    rv = container->Init(mChromeDataSource, aPackageList);
  if (NS_FAILED(rv))
    return rv;

  // step through its (seq) arcs
  nsCOMPtr<nsISimpleEnumerator> arcs;
  rv = container->GetElements(getter_AddRefs(arcs));
  if (NS_FAILED(rv)) return rv;

  PRBool moreElements;
  rv = arcs->HasMoreElements(&moreElements);
  if (NS_FAILED(rv)) return rv;
  for ( ; moreElements; arcs->HasMoreElements(&moreElements)) {

    // get next arc resource
    nsCOMPtr<nsISupports> supports;
    rv = arcs->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIRDFResource> kid = do_QueryInterface(supports);

    if (kid) {
      // get its package resource
      nsCOMPtr<nsIRDFNode> packageNode;
      nsCOMPtr<nsIRDFResource> package;
      rv = mChromeDataSource->GetTarget(kid, mPackage, PR_TRUE, getter_AddRefs(packageNode));
      if (NS_SUCCEEDED(rv))
        package = do_QueryInterface(packageNode);
      if (!package)
        continue;

      // get its name
      nsCAutoString packageName;
      rv = FollowArc(mChromeDataSource, packageName, package, mName);
      if (NS_FAILED(rv))
        continue;       // don't fail if package has not yet been installed

      if (packageName.Equals(aPackage)) {
        PRBool useProfile = !mProfileRoot.IsEmpty();
        // XXXldb Do we really want to do this?  We risk crossing skins.
        if (packageName.Equals("global") || packageName.Equals("communicator"))
          useProfile = PR_FALSE; // Always force the auto-selection to be in the
                                 // install dir for the packages required to bring up the profile UI.
        rv = SelectProviderForPackage(aProvider,
                                      aProviderName,
                                      NS_ConvertASCIItoUCS2(packageName).get(),
                                      aArc, useProfile, PR_TRUE);
        if (NS_FAILED(rv))
          return NS_ERROR_FAILURE;

        *aSelectedProvider = kid;
        NS_ADDREF(*aSelectedProvider);
        return NS_OK;
      }
    }
  }
  return NS_OK;
}

nsresult
nsChromeRegistry::GetDynamicDataSource(nsIURI *aChromeURL,
                                       PRBool aIsOverlay, PRBool aUseProfile,
                                       PRBool aCreateDS,
                                       nsIRDFDataSource **aResult)
{
  *aResult = nsnull;

  nsresult rv;

  if (!mDataSourceTable)
    return NS_OK;

  // Obtain the package, provider and remaining from the URL
  nsCAutoString package, provider, remaining;

  rv = SplitURL(aChromeURL, package, provider, remaining);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aCreateDS) {
    // We are not supposed to create the data source, which means
    // we should first check our chrome.rdf file to see if this
    // package even has dynamic data.  Only if it claims to have
    // dynamic data are we willing to hand back a datasource.
    nsDependentCString dataSourceStr(kChromeFileName);
    nsCOMPtr<nsIRDFDataSource> mainDataSource;
    rv = LoadDataSource(dataSourceStr, getter_AddRefs(mainDataSource), aUseProfile, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // Now that we have the appropriate chrome.rdf file, we
    // must check the package resource for stylesheets or overlays.
    nsCOMPtr<nsIRDFResource> hasDynamicDataArc;
    if (aIsOverlay)
      hasDynamicDataArc = mHasOverlays;
    else
      hasDynamicDataArc = mHasStylesheets;
    
    // Obtain the resource for the package.
    nsCAutoString packageResourceStr("urn:mozilla:package:");
    packageResourceStr += package;
    nsCOMPtr<nsIRDFResource> packageResource;
    GetResource(packageResourceStr, getter_AddRefs(packageResource));
    
    // Follow the dynamic data arc to see if we should continue.
    // Only if it claims to have dynamic data do we even bother.
    nsCAutoString hasDynamicDS;
    nsChromeRegistry::FollowArc(mainDataSource, hasDynamicDS, 
                                packageResource, hasDynamicDataArc);
    if (hasDynamicDS.IsEmpty())
      return NS_OK; // No data source exists.
  }

  // Retrieve the mInner data source.
  nsCAutoString overlayFile( "overlayinfo/" );
  overlayFile += package;
  overlayFile += "/";

  if (aIsOverlay)
    overlayFile += "content/overlays.rdf";
  else overlayFile += "skin/stylesheets.rdf";

  return LoadDataSource(overlayFile, aResult, aUseProfile, nsnull);
}

NS_IMETHODIMP
nsChromeRegistry::GetStyleOverlays(nsIURI *aChromeURL,
                                   nsISimpleEnumerator **aResult)
{
  return GetDynamicInfo(aChromeURL, PR_FALSE, aResult);
}

NS_IMETHODIMP
nsChromeRegistry::GetXULOverlays(nsIURI *aChromeURL, nsISimpleEnumerator **aResult)
{
  return GetDynamicInfo(aChromeURL, PR_TRUE, aResult);
}

nsresult
nsChromeRegistry::GetURIList(nsIRDFDataSource *aSource,
                             nsIRDFResource *aResource,
                             nsCOMArray<nsIURI>& aArray)
{
  nsresult rv;
  nsCOMPtr<nsISimpleEnumerator> arcs;
  nsCOMPtr<nsIRDFContainer> container =
    do_CreateInstance("@mozilla.org/rdf/container;1", &rv);
  if (NS_FAILED(rv)) goto end_GetURIList;

  rv = container->Init(aSource, aResource);
  if (NS_FAILED(rv)) {
    rv = NS_OK;
    goto end_GetURIList;
  }

  rv = container->GetElements(getter_AddRefs(arcs));
  if (NS_FAILED(rv)) goto end_GetURIList;

  {
    nsCOMPtr<nsISupports> supports;
    nsCOMPtr<nsIRDFLiteral> value;
    nsCOMPtr<nsIURI> uri;
    PRBool hasMore;

    while (NS_SUCCEEDED(rv = arcs->HasMoreElements(&hasMore)) && hasMore) {
      rv = arcs->GetNext(getter_AddRefs(supports));
      if (NS_FAILED(rv)) break;

      value = do_QueryInterface(supports, &rv);
      if (NS_FAILED(rv)) continue;

      const PRUnichar* valueStr;
      rv = value->GetValueConst(&valueStr);
      if (NS_FAILED(rv)) continue;

      rv = NS_NewURI(getter_AddRefs(uri), NS_ConvertUTF16toUTF8(valueStr));
      if (NS_FAILED(rv)) continue;

      if (IsOverlayAllowed(uri)) {
        if (!aArray.AppendObject(uri)) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
      }
    }
  }

end_GetURIList:
  return rv;
}

nsresult
nsChromeRegistry::GetDynamicInfo(nsIURI *aChromeURL, PRBool aIsOverlay,
                                 nsISimpleEnumerator **aResult)
{
  *aResult = nsnull;

  nsresult rv;

  if (!mDataSourceTable)
    return NS_OK;

  nsCOMPtr<nsIRDFDataSource> installSource;
  rv = GetDynamicDataSource(aChromeURL, aIsOverlay, PR_FALSE, PR_FALSE,
                            getter_AddRefs(installSource));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFDataSource> profileSource;
  if (mProfileInitialized) {
    rv = GetDynamicDataSource(aChromeURL, aIsOverlay, PR_TRUE, PR_FALSE,
                              getter_AddRefs(profileSource));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCAutoString lookup;
  rv = aChromeURL->GetSpec(lookup);
  NS_ENSURE_SUCCESS(rv, rv);
   
  // Get the chromeResource from this lookup string
  nsCOMPtr<nsIRDFResource> chromeResource;
  rv = GetResource(lookup, getter_AddRefs(chromeResource));
  if (NS_FAILED(rv)) {
      NS_ERROR("Unable to retrieve the resource corresponding to the chrome skin or content.");
      return rv;
  }

  nsCOMArray<nsIURI> overlayURIs;

  if (installSource) {
    GetURIList(installSource, chromeResource, overlayURIs);
  }
  if (profileSource) {
    GetURIList(profileSource, chromeResource, overlayURIs);
  }

  return NS_NewArrayEnumerator(aResult, overlayURIs);
}

nsresult
nsChromeRegistry::LoadDataSource(const nsACString &aFileName,
                                 nsIRDFDataSource **aResult,
                                 PRBool aUseProfileDir,
                                 const char *aProfilePath)
{
  // Init the data source to null.
  *aResult = nsnull;

  nsCAutoString key;

  // Try the profile root first.
  if (aUseProfileDir) {
    // use given profile path if non-null
    if (aProfilePath) {
      key = aProfilePath;
      key += "chrome/";
    }
    else
      key = mProfileRoot;

    key += aFileName;
  }
  else {
    key = mInstallRoot;
    key += aFileName;
  }

  if (mDataSourceTable)
  {
    nsCStringKey skey(key);
    nsCOMPtr<nsISupports> supports =
      getter_AddRefs(NS_STATIC_CAST(nsISupports*, mDataSourceTable->Get(&skey)));

    if (supports)
    {
      nsCOMPtr<nsIRDFDataSource> dataSource = do_QueryInterface(supports);
      if (dataSource)
      {
        *aResult = dataSource;
        NS_ADDREF(*aResult);
        return NS_OK;
      }
      return NS_ERROR_FAILURE;
    }
  }

  nsresult rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                                     nsnull,
                                                     NS_GET_IID(nsIRDFDataSource),
                                                     (void**) aResult);
  if (NS_FAILED(rv)) return rv;

  // Seed the datasource with the ``chrome'' namespace
  nsCOMPtr<nsIRDFXMLSink> sink = do_QueryInterface(*aResult);
  if (sink)
    sink->AddNameSpace(sCPrefix, NS_ConvertASCIItoUCS2(CHROME_URI));

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(*aResult);
  if (! remote)
      return NS_ERROR_UNEXPECTED;

  if (!mDataSourceTable)
    mDataSourceTable = new nsSupportsHashtable;

  // We need to read this synchronously.
  rv = remote->Init(key.get());
  if (NS_SUCCEEDED(rv))
    rv = remote->Refresh(PR_TRUE);

  nsCOMPtr<nsISupports> supports = do_QueryInterface(remote);
  nsCStringKey skey(key);
  mDataSourceTable->Put(&skey, supports.get());

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsChromeRegistry::GetResource(const nsACString& aURL,
                              nsIRDFResource** aResult)
{
  nsresult rv = NS_OK;
  if (NS_FAILED(rv = mRDFService->GetResource(aURL, aResult))) {
    NS_ERROR("Unable to retrieve a resource for this URL.");
    *aResult = nsnull;
    return rv;
  }
  return NS_OK;
}

nsresult
nsChromeRegistry::FollowArc(nsIRDFDataSource *aDataSource,
                            nsACString& aResult,
                            nsIRDFResource* aChromeResource,
                            nsIRDFResource* aProperty)
{
  if (!aDataSource)
    return NS_ERROR_FAILURE;

  nsresult rv;

  nsCOMPtr<nsIRDFNode> chromeBase;
  rv = aDataSource->GetTarget(aChromeResource, aProperty, PR_TRUE, getter_AddRefs(chromeBase));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain a base resource.");
    return rv;
  }

  if (chromeBase == nsnull)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFResource> resource(do_QueryInterface(chromeBase));

  if (resource) {
    nsXPIDLCString uri;
    rv = resource->GetValue(getter_Copies(uri));
    if (NS_FAILED(rv)) return rv;
    aResult.Assign(uri);
    return NS_OK;
  }

  nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(chromeBase));
  if (literal) {
    const PRUnichar *s;
    rv = literal->GetValueConst(&s);
    if (NS_FAILED(rv)) return rv;
    CopyUTF16toUTF8(s, aResult);
  }
  else {
    // This should _never_ happen.
    NS_ERROR("uh, this isn't a resource or a literal!");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult
nsChromeRegistry::UpdateArc(nsIRDFDataSource *aDataSource, nsIRDFResource* aSource,
                            nsIRDFResource* aProperty,
                            nsIRDFNode *aTarget, PRBool aRemove)
{
  nsresult rv;
  // Get the old targets
  nsCOMPtr<nsIRDFNode> retVal;
  rv = aDataSource->GetTarget(aSource, aProperty, PR_TRUE, getter_AddRefs(retVal));
  if (NS_FAILED(rv)) return rv;

  if (retVal) {
    if (!aRemove)
      aDataSource->Change(aSource, aProperty, retVal, aTarget);
    else
      aDataSource->Unassert(aSource, aProperty, aTarget);
  }
  else if (!aRemove)
    aDataSource->Assert(aSource, aProperty, aTarget, PR_TRUE);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

// theme stuff


static void FlushSkinBindingsForWindow(nsIDOMWindowInternal* aWindow)
{
  // Get the DOM document.
  nsCOMPtr<nsIDOMDocument> domDocument;
  aWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument)
    return;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  if (!document)
    return;

  // Annihilate all XBL bindings.
  document->GetBindingManager()->FlushSkinBindings();
}

// XXXbsmedberg: move this to nsIWindowMediator
NS_IMETHODIMP nsChromeRegistry::RefreshSkins()
{
  nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
  if (!windowMediator)
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
  PRBool more;
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> protoWindow;
    windowEnumerator->GetNext(getter_AddRefs(protoWindow));
    if (protoWindow) {
      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(protoWindow);
      if (domWindow)
        FlushSkinBindingsForWindow(domWindow);
    }
    windowEnumerator->HasMoreElements(&more);
  }

  FlushSkinCaches();
  
  windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> protoWindow;
    windowEnumerator->GetNext(getter_AddRefs(protoWindow));
    if (protoWindow) {
      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(protoWindow);
      if (domWindow)
        RefreshWindow(domWindow);
    }
    windowEnumerator->HasMoreElements(&more);
  }
   
  return NS_OK;
}


void
nsChromeRegistry::FlushSkinCaches()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ASSERTION(obsSvc, "Couldn't get observer service.");

  obsSvc->NotifyObservers((nsIChromeRegistry*) this,
                          NS_CHROME_FLUSH_SKINS_TOPIC, nsnull);
}

static PRBool IsChromeURI(nsIURI* aURI)
{
    PRBool isChrome=PR_FALSE;
    if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && isChrome)
        return PR_TRUE;
    return PR_FALSE;
}

// XXXbsmedberg: move this to windowmediator
nsresult nsChromeRegistry::RefreshWindow(nsIDOMWindowInternal* aWindow)
{
  // Deal with our subframes first.
  nsCOMPtr<nsIDOMWindowCollection> frames;
  aWindow->GetFrames(getter_AddRefs(frames));
  PRUint32 length;
  frames->GetLength(&length);
  PRUint32 j;
  for (j = 0; j < length; j++) {
    nsCOMPtr<nsIDOMWindow> childWin;
    frames->Item(j, getter_AddRefs(childWin));
    nsCOMPtr<nsIDOMWindowInternal> childInt(do_QueryInterface(childWin));
    RefreshWindow(childInt);
  }

  nsresult rv;
  // Get the DOM document.
  nsCOMPtr<nsIDOMDocument> domDocument;
  aWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument)
    return NS_OK;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  if (!document)
    return NS_OK;

  // Deal with the agent sheets first.  Have to do all the style sets by hand.
  PRUint32 shellCount = document->GetNumberOfShells();
  for (PRUint32 k = 0; k < shellCount; k++) {
    nsIPresShell *shell = document->GetShellAt(k);

    // Reload only the chrome URL agent style sheets.
    nsCOMArray<nsIStyleSheet> agentSheets;
    rv = shell->GetAgentStyleSheets(agentSheets);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMArray<nsIStyleSheet> newAgentSheets;
    for (PRInt32 l = 0; l < agentSheets.Count(); ++l) {
      nsIStyleSheet *sheet = agentSheets[l];

      nsCOMPtr<nsIURI> uri;
      rv = sheet->GetURL(*getter_AddRefs(uri));
      if (NS_FAILED(rv)) return rv;

      if (IsChromeURI(uri)) {
        // Reload the sheet.
        nsCOMPtr<nsICSSStyleSheet> newSheet;
        rv = LoadStyleSheetWithURL(uri, getter_AddRefs(newSheet));
        if (NS_FAILED(rv)) return rv;
        if (newSheet) {
          rv = newAgentSheets.AppendObject(newSheet) ? NS_OK : NS_ERROR_FAILURE;
          if (NS_FAILED(rv)) return rv;
        }
      }
      else {  // Just use the same sheet.
        rv = newAgentSheets.AppendObject(sheet) ? NS_OK : NS_ERROR_FAILURE;
        if (NS_FAILED(rv)) return rv;
      }
    }

    rv = shell->SetAgentStyleSheets(newAgentSheets);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Build an array of nsIURIs of style sheets we need to load.
  nsCOMArray<nsIStyleSheet> oldSheets;
  nsCOMArray<nsIStyleSheet> newSheets;

  PRInt32 count = document->GetNumberOfStyleSheets(PR_FALSE);

  // Iterate over the style sheets.
  PRInt32 i;
  for (i = 0; i < count; i++) {
    // Get the style sheet
    nsIStyleSheet *styleSheet = document->GetStyleSheetAt(i, PR_FALSE);
    
    if (!oldSheets.AppendObject(styleSheet)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Iterate over our old sheets and kick off a sync load of the new 
  // sheet if and only if it's a chrome URL.
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIStyleSheet> sheet = oldSheets[i];
    nsCOMPtr<nsIURI> uri;
    rv = sheet->GetURL(*getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    if (IsChromeURI(uri)) {
      // Reload the sheet.
#ifdef DEBUG
      nsCOMPtr<nsICSSStyleSheet> oldCSSSheet = do_QueryInterface(sheet);
      NS_ASSERTION(oldCSSSheet, "Don't know how to reload a non-CSS sheet");
#endif
      nsCOMPtr<nsICSSStyleSheet> newSheet;
      // XXX what about chrome sheets that have a title or are disabled?  This
      // only works by sheer dumb luck.
      // XXXbz this should really use the document's CSSLoader!
      LoadStyleSheetWithURL(uri, getter_AddRefs(newSheet));
      // Even if it's null, we put in in there.
      newSheets.AppendObject(newSheet);
    }
    else {
      // Just use the same sheet.
      newSheets.AppendObject(sheet);
    }
  }

  // Now notify the document that multiple sheets have been added and removed.
  document->UpdateStyleSheets(oldSheets, newSheets);
  return NS_OK;
}

nsresult
nsChromeRegistry::WriteInfoToDataSource(const char *aDocURI,
                                        const PRUnichar *aOverlayURI,
                                        PRBool aIsOverlay,
                                        PRBool aUseProfile,
                                        PRBool aRemove)
{
  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  nsCAutoString str(aDocURI);
  rv = NS_NewURI(getter_AddRefs(uri), str);
  if (NS_FAILED(rv)) return rv;

  if (!aRemove) {
    // We are installing a dynamic overlay or package.  
    // We must split the doc URI and obtain our package or skin.
    // We then annotate the chrome.rdf datasource in the appropriate
    // install/profile dir (based off aUseProfile) with the knowledge
    // that we have overlays or stylesheets.
    nsCAutoString package, provider, file;
    rv = SplitURL(uri, package, provider, file);
    if (NS_FAILED(rv)) return NS_OK;
    
    // Obtain our chrome data source.
    nsDependentCString dataSourceStr(kChromeFileName);
    nsCOMPtr<nsIRDFDataSource> mainDataSource;
    rv = LoadDataSource(dataSourceStr, getter_AddRefs(mainDataSource), aUseProfile, nsnull);
    if (NS_FAILED(rv)) return rv;
    
    // Now that we have the appropriate chrome.rdf file, we 
    // must annotate the package resource with the knowledge of
    // whether or not we have stylesheets or overlays.
    nsCOMPtr<nsIRDFResource> hasDynamicDataArc;
    if (aIsOverlay)
      hasDynamicDataArc = mHasOverlays;
    else
      hasDynamicDataArc = mHasStylesheets;
    
    // Obtain the resource for the package.
    nsCAutoString packageResourceStr("urn:mozilla:package:");
    packageResourceStr += package;
    nsCOMPtr<nsIRDFResource> packageResource;
    GetResource(packageResourceStr, getter_AddRefs(packageResource));
    
    // Now add the arc to the package.
    nsCOMPtr<nsIRDFLiteral> trueLiteral;
    mRDFService->GetLiteral(NS_LITERAL_STRING("true").get(), getter_AddRefs(trueLiteral));
    nsChromeRegistry::UpdateArc(mainDataSource, packageResource, 
                                hasDynamicDataArc, 
                                trueLiteral, PR_FALSE);
  }

  nsCOMPtr<nsIRDFDataSource> dataSource;
  rv = GetDynamicDataSource(uri, aIsOverlay, aUseProfile, PR_TRUE, getter_AddRefs(dataSource));
  if (NS_FAILED(rv)) return rv;

  if (!dataSource)
    return NS_OK;

  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(str, getter_AddRefs(resource));

  if (NS_FAILED(rv))
    return NS_OK;

  nsCOMPtr<nsIRDFContainer> container;
  rv = mRDFContainerUtils->MakeSeq(dataSource, resource, getter_AddRefs(container));
  if (NS_FAILED(rv)) return rv;
  if (!container) {
    // Already exists. Create a container instead.
    rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                      nsnull,
                                      NS_GET_IID(nsIRDFContainer),
                                      getter_AddRefs(container));
    if (NS_FAILED(rv)) return rv;
    rv = container->Init(dataSource, resource);
    if (NS_FAILED(rv)) return rv;
  }

  nsAutoString unistr(aOverlayURI);
  nsCOMPtr<nsIRDFLiteral> literal;
  rv = mRDFService->GetLiteral(unistr.get(), getter_AddRefs(literal));
  if (NS_FAILED(rv)) return rv;

  if (aRemove) {
    rv = container->RemoveElement(literal, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    PRInt32 index;
    rv = container->IndexOf(literal, &index);
    if (NS_FAILED(rv)) return rv;
    if (index == -1) {
      rv = container->AppendElement(literal);
      if (NS_FAILED(rv)) return rv;
    }
  }

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(dataSource, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = remote->Flush();
  }

  return rv;
}

nsresult
nsChromeRegistry::UpdateDynamicDataSource(nsIRDFDataSource *aDataSource,
                                          nsIRDFResource *aResource,
                                          PRBool aIsOverlay,
                                          PRBool aUseProfile, PRBool aRemove)
{
  nsCOMPtr<nsIRDFContainer> container;
  nsresult rv;

  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv)) return rv;

  rv = container->Init(aDataSource, aResource);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  rv = container->GetElements(getter_AddRefs(arcs));
  if (NS_FAILED(rv)) return rv;

  PRBool moreElements;
  rv = arcs->HasMoreElements(&moreElements);
  if (NS_FAILED(rv)) return rv;

  const char *value;
  rv = aResource->GetValueConst(&value);
  if (NS_FAILED(rv)) return rv;

  while (moreElements)
  {
    nsCOMPtr<nsISupports> supports;
    rv = arcs->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(supports, &rv);

    if (NS_SUCCEEDED(rv))
    {
      const PRUnichar* valueStr;
      rv = literal->GetValueConst(&valueStr);
      if (NS_FAILED(rv)) return rv;

      rv = WriteInfoToDataSource(value, valueStr, aIsOverlay, aUseProfile, aRemove);
      if (NS_FAILED(rv)) return rv;
    }
    rv = arcs->HasMoreElements(&moreElements);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}


nsresult
nsChromeRegistry::UpdateDynamicDataSources(nsIRDFDataSource *aDataSource,
                                           PRBool aIsOverlay,
                                           PRBool aUseProfile, PRBool aRemove)
{
  nsresult rv;
  nsCOMPtr<nsIRDFResource> resource;
  nsCAutoString root;
  if (aIsOverlay)
    root.Assign("urn:mozilla:overlays");
  else root.Assign("urn:mozilla:stylesheets");

  rv = GetResource(root, getter_AddRefs(resource));

  if (!resource)
    return NS_OK;

  nsCOMPtr<nsIRDFContainer> container(do_CreateInstance("@mozilla.org/rdf/container;1"));
  if (!container)
    return NS_OK;

  if (NS_FAILED(container->Init(aDataSource, resource)))
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  if (NS_FAILED(container->GetElements(getter_AddRefs(arcs))))
    return NS_OK;

  PRBool moreElements;
  rv = arcs->HasMoreElements(&moreElements);
  if (NS_FAILED(rv)) return rv;

  while (moreElements)
  {
    nsCOMPtr<nsISupports> supports;
    rv = arcs->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> resource2 = do_QueryInterface(supports, &rv);

    if (NS_SUCCEEDED(rv))
    {
      rv = UpdateDynamicDataSource(aDataSource, resource2, aIsOverlay, aUseProfile, aRemove);
      if (NS_FAILED(rv)) return rv;
    }

    rv = arcs->HasMoreElements(&moreElements);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::SelectSkin(const nsACString& aSkin,
                                        PRBool aUseProfile)
{
  nsresult rv = SetProvider(NS_LITERAL_CSTRING("skin"), mSelectedSkin, aSkin, aUseProfile, nsnull, PR_TRUE);
  FlushSkinCaches();
  return rv;
}

NS_IMETHODIMP nsChromeRegistry::SelectLocale(const nsACString& aLocale,
                                          PRBool aUseProfile)
{
  return SetProvider(NS_LITERAL_CSTRING("locale"), mSelectedLocale, aLocale, aUseProfile, nsnull, PR_TRUE);
}

NS_IMETHODIMP nsChromeRegistry::SelectLocaleForProfile(const nsACString& aLocale,
                                                       const PRUnichar *aProfilePath)
{
  // to be changed to use given path
  return SetProvider(NS_LITERAL_CSTRING("locale"), mSelectedLocale, aLocale, PR_TRUE, NS_ConvertUCS2toUTF8(aProfilePath).get(), PR_TRUE);
}

NS_IMETHODIMP nsChromeRegistry::SelectSkinForProfile(const nsACString& aSkin,
                                                     const PRUnichar *aProfilePath)
{
  return SetProvider(NS_LITERAL_CSTRING("skin"), mSelectedSkin, aSkin, PR_TRUE, NS_ConvertUCS2toUTF8(aProfilePath).get(), PR_TRUE);
}

/* void setRuntimeProvider (in boolean runtimeProvider); */
// should we inline this one?
NS_IMETHODIMP nsChromeRegistry::SetRuntimeProvider(PRBool runtimeProvider)
{
  mRuntimeProvider = runtimeProvider;
  return NS_OK;
}


/* ACString getSelectedLocale (ACString packageName); */
NS_IMETHODIMP
nsChromeRegistry::GetSelectedLocale(const nsACString& aPackageName,
                                    nsACString& aResult)
{
  return GetSelectedProvider(aPackageName,
                             NS_LITERAL_CSTRING("locale"), mSelectedLocale,
                             aResult);
}

NS_IMETHODIMP
nsChromeRegistry::GetSelectedSkin(const nsACString& aPackageName,
                                  nsACString& aResult)
{
  return GetSelectedProvider(aPackageName,
                             NS_LITERAL_CSTRING("skin"), mSelectedSkin,
                             aResult);
}

nsresult
nsChromeRegistry::GetSelectedProvider(const nsACString& aPackageName,
                                      const nsACString& aProvider,
                                      nsIRDFResource* aSelectionArc,
                                      nsACString& _retval)
{
  // check if mChromeDataSource is null; do we need to apply this to every instance?
  // is there a better way to test if the data source is ready?
  if (!mChromeDataSource) {
    return NS_ERROR_FAILURE;
  }

  nsCAutoString resourceStr("urn:mozilla:package:");
  resourceStr += aPackageName;

  // Obtain the resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(resourceStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }

  if (mChromeDataSource == nsnull)
    return NS_ERROR_NULL_POINTER;

  // Follow the "selectedLocale" arc.
  nsCOMPtr<nsIRDFNode> selectedProvider;
  if (NS_FAILED(rv = mChromeDataSource->GetTarget(resource, aSelectionArc, PR_TRUE, getter_AddRefs(selectedProvider)))) {
    NS_ERROR("Unable to obtain the provider.");
    return rv;
  }

  if (!selectedProvider) {
    rv = FindProvider(aPackageName, aProvider, aSelectionArc, getter_AddRefs(selectedProvider));
    if (!selectedProvider)
      return rv;
  }

  resource = do_QueryInterface(selectedProvider);
  if (!resource)
    return NS_ERROR_FAILURE;

  // selectedProvider.mURI now looks like "urn:mozilla:locale:ja-JP:navigator"
  const char *uri;
  if (NS_FAILED(rv = resource->GetValueConst(&uri)))
    return rv;

  // trim down to "urn:mozilla:locale:ja-JP"
  nsCAutoString packageStr(":");
  packageStr += aPackageName;

  nsCAutoString ustr(uri);
  PRInt32 pos = ustr.RFind(packageStr);
  nsCAutoString urn;
  ustr.Left(urn, pos);

  rv = GetResource(urn, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the provider resource.");
    return rv;
  }

  // From this resource, follow the "name" arc.
  return FollowArc(mChromeDataSource, _retval, resource, mName);
}

NS_IMETHODIMP nsChromeRegistry::DeselectSkin(const nsACString& aSkin,
                                        PRBool aUseProfile)
{
  nsresult rv = SetProvider(NS_LITERAL_CSTRING("skin"), mSelectedSkin, aSkin, aUseProfile, nsnull, PR_FALSE);
  FlushSkinCaches();
  return rv;
}

NS_IMETHODIMP nsChromeRegistry::DeselectLocale(const nsACString& aLocale,
                                          PRBool aUseProfile)
{
  return SetProvider(NS_LITERAL_CSTRING("locale"), mSelectedLocale, aLocale, aUseProfile, nsnull, PR_FALSE);
}

nsresult
nsChromeRegistry::SetProvider(const nsACString& aProvider,
                              nsIRDFResource* aSelectionArc,
                              const nsACString& aProviderName,
                              PRBool aUseProfile, const char *aProfilePath,
                              PRBool aIsAdding)
{
  // Build the provider resource str.
  // e.g., urn:mozilla:skin:aqua/1.0
  nsCAutoString resourceStr( "urn:mozilla:" );
  resourceStr += aProvider;
  resourceStr += ":";
  resourceStr += aProviderName;

  // Obtain the provider resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(resourceStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }
  NS_ASSERTION(resource, "failed to GetResource");

  // Follow the packages arc to the package resources.
  nsCOMPtr<nsIRDFNode> packageList;
  rv = mChromeDataSource->GetTarget(resource, mPackages, PR_TRUE, getter_AddRefs(packageList));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the SEQ for the package list.");
    return rv;
  }
  // ok for packageList to be null here -- it just means that we haven't encountered that package yet

  nsCOMPtr<nsIRDFResource> packageSeq(do_QueryInterface(packageList, &rv));
  if (NS_FAILED(rv)) return rv;

  // Build an RDF container to wrap the SEQ
  nsCOMPtr<nsIRDFContainer> container;
  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv))
    return NS_OK;

  if (NS_FAILED(container->Init(mChromeDataSource, packageSeq)))
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  if (NS_FAILED(container->GetElements(getter_AddRefs(arcs))))
    return NS_OK;

  // For each skin/package entry, follow the arcs to the real package
  // resource.
  PRBool more;
  rv = arcs->HasMoreElements(&more);
  if (NS_FAILED(rv)) return rv;
  while (more) {
    nsCOMPtr<nsISupports> packageSkinEntry;
    rv = arcs->GetNext(getter_AddRefs(packageSkinEntry));
    if (NS_SUCCEEDED(rv) && packageSkinEntry) {
      nsCOMPtr<nsIRDFResource> entry = do_QueryInterface(packageSkinEntry);
      if (entry) {
         // Obtain the real package resource.
         nsCOMPtr<nsIRDFNode> packageNode;
         rv = mChromeDataSource->GetTarget(entry, mPackage, PR_TRUE, getter_AddRefs(packageNode));
         if (NS_FAILED(rv)) {
           NS_ERROR("Unable to obtain the package resource.");
           return rv;
         }

         // Select the skin for this package resource.
         nsCOMPtr<nsIRDFResource> packageResource(do_QueryInterface(packageNode));
         if (packageResource) {
           rv = SetProviderForPackage(aProvider, packageResource, entry, aSelectionArc, aUseProfile, aProfilePath, aIsAdding);
           if (NS_FAILED(rv))
             continue; // Well, let's set as many sub-packages as we can...
         }
      }
    }
    rv = arcs->HasMoreElements(&more);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

nsresult
nsChromeRegistry::SetProviderForPackage(const nsACString& aProvider,
                                        nsIRDFResource* aPackageResource,
                                        nsIRDFResource* aProviderPackageResource,
                                        nsIRDFResource* aSelectionArc,
                                        PRBool aUseProfile, const char *aProfilePath,
                                        PRBool aIsAdding)
{
  nsresult rv;
  
  // Figure out which file we're needing to modify, e.g., is it the install
  // dir or the profile dir, and get the right datasource.
  nsCOMPtr<nsIRDFDataSource> dataSource;
  rv = LoadDataSource(kChromeFileName, getter_AddRefs(dataSource), aUseProfile, aProfilePath);
  if (NS_FAILED(rv)) return rv;

  rv = nsChromeRegistry::UpdateArc(dataSource, aPackageResource, aSelectionArc, aProviderPackageResource, !aIsAdding);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(dataSource, &rv);
  if (NS_FAILED(rv)) return rv;

  // add one more check: 
  //   assert the data source only when we are not setting runtime-only provider
  if (!mBatchInstallFlushes && !mRuntimeProvider)
    rv = remote->Flush();

  // always reset the flag
  mRuntimeProvider = PR_FALSE;

  return rv;
}

NS_IMETHODIMP nsChromeRegistry::SelectSkinForPackage(const nsACString& aSkin,
                                                     const PRUnichar *aPackageName,
                                                  PRBool aUseProfile)
{
  return SelectProviderForPackage(NS_LITERAL_CSTRING("skin"), aSkin, aPackageName, mSelectedSkin, aUseProfile, PR_TRUE);
}

NS_IMETHODIMP nsChromeRegistry::SelectLocaleForPackage(const nsACString& aLocale,
                                                       const PRUnichar *aPackageName,
                                                    PRBool aUseProfile)
{
  return SelectProviderForPackage(NS_LITERAL_CSTRING("locale"), aLocale, aPackageName, mSelectedLocale, aUseProfile, PR_TRUE);
}

NS_IMETHODIMP nsChromeRegistry::DeselectSkinForPackage(const nsACString& aSkin,
                                                  const PRUnichar *aPackageName,
                                                  PRBool aUseProfile)
{
  return SelectProviderForPackage(NS_LITERAL_CSTRING("skin"), aSkin, aPackageName, mSelectedSkin, aUseProfile, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::DeselectLocaleForPackage(const nsACString& aLocale,
                                                    const PRUnichar *aPackageName,
                                                    PRBool aUseProfile)
{
  return SelectProviderForPackage(NS_LITERAL_CSTRING("locale"), aLocale, aPackageName, mSelectedLocale, aUseProfile, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::IsSkinSelectedForPackage(const nsACString& aSkin,
                                                  const PRUnichar *aPackageName,
                                                  PRBool aUseProfile, PRBool* aResult)
{
  return IsProviderSelectedForPackage(NS_LITERAL_CSTRING("skin"), aSkin, aPackageName, mSelectedSkin, aUseProfile, aResult);
}

NS_IMETHODIMP nsChromeRegistry::IsLocaleSelectedForPackage(const nsACString& aLocale,
                                                    const PRUnichar *aPackageName,
                                                    PRBool aUseProfile, PRBool* aResult)
{
  return IsProviderSelectedForPackage(NS_LITERAL_CSTRING("locale"), aLocale, aPackageName, mSelectedLocale, aUseProfile, aResult);
}

nsresult
nsChromeRegistry::SelectProviderForPackage(const nsACString& aProviderType,
                                           const nsACString& aProviderName,
                                           const PRUnichar *aPackageName,
                                           nsIRDFResource* aSelectionArc,
                                           PRBool aUseProfile, PRBool aIsAdding)
{
  nsCAutoString package( "urn:mozilla:package:" );
  package.AppendWithConversion(aPackageName);

  nsCAutoString provider( "urn:mozilla:" );
  provider += aProviderType;
  provider += ":";
  provider += aProviderName;
  provider += ":";
  provider.AppendWithConversion(aPackageName);

  // Obtain the package resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> packageResource;
  rv = GetResource(package, getter_AddRefs(packageResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }
  NS_ASSERTION(packageResource, "failed to get packageResource");

  // Obtain the provider resource.
  nsCOMPtr<nsIRDFResource> providerResource;
  rv = GetResource(provider, getter_AddRefs(providerResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the provider resource.");
    return rv;
  }
  NS_ASSERTION(providerResource, "failed to get providerResource");

  // Version-check before selecting.  If this skin isn't a compatible version, then
  // don't allow the selection.
  PRBool acceptable;
  rv = VerifyCompatibleProvider(packageResource, providerResource,
                                aSelectionArc, &acceptable);
  if (NS_FAILED(rv))
    return rv;
  if (!acceptable)
    return NS_ERROR_FAILURE;

  return SetProviderForPackage(aProviderType, packageResource, providerResource, aSelectionArc,
                               aUseProfile, nsnull, aIsAdding);
}

NS_IMETHODIMP nsChromeRegistry::IsSkinSelected(const nsACString& aSkin,
                                               PRBool aUseProfile, PRInt32* aResult)
{
  return IsProviderSelected(NS_LITERAL_CSTRING("skin"), aSkin, mSelectedSkin, aUseProfile, aResult);
}

NS_IMETHODIMP nsChromeRegistry::IsLocaleSelected(const nsACString& aLocale,
                                                 PRBool aUseProfile, PRInt32* aResult)
{
  return IsProviderSelected(NS_LITERAL_CSTRING("locale"), aLocale, mSelectedLocale, aUseProfile, aResult);
}

nsresult
nsChromeRegistry::IsProviderSelected(const nsACString& aProvider,
                                     const nsACString& aProviderName,
                                     nsIRDFResource* aSelectionArc,
                                     PRBool aUseProfile, PRInt32* aResult)
{
  // Build the provider resource str.
  // e.g., urn:mozilla:skin:aqua/1.0
  *aResult = NONE;
  nsCAutoString resourceStr( "urn:mozilla:" );
  resourceStr += aProvider;
  resourceStr += ":";
  resourceStr += aProviderName;
  // Obtain the provider resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(resourceStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }
  NS_ASSERTION(resource, "failed to GetResource");

  // Follow the packages arc to the package resources.
  nsCOMPtr<nsIRDFNode> packageList;
  rv = mChromeDataSource->GetTarget(resource, mPackages, PR_TRUE, getter_AddRefs(packageList));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the SEQ for the package list.");
    return rv;
  }
  // ok for packageList to be null here -- it just means that we haven't encountered that package yet

  nsCOMPtr<nsIRDFResource> packageSeq(do_QueryInterface(packageList, &rv));
  if (NS_FAILED(rv)) return rv;

  // Build an RDF container to wrap the SEQ
  nsCOMPtr<nsIRDFContainer> container(do_CreateInstance("@mozilla.org/rdf/container;1"));
  if (NS_FAILED(container->Init(mChromeDataSource, packageSeq)))
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  container->GetElements(getter_AddRefs(arcs));

  // For each skin/package entry, follow the arcs to the real package
  // resource.
  PRBool more;
  PRInt32 numSet = 0;
  PRInt32 numPackages = 0;
  rv = arcs->HasMoreElements(&more);
  if (NS_FAILED(rv)) return rv;
  while (more) {
    nsCOMPtr<nsISupports> packageSkinEntry;
    rv = arcs->GetNext(getter_AddRefs(packageSkinEntry));
    if (NS_SUCCEEDED(rv) && packageSkinEntry) {
      nsCOMPtr<nsIRDFResource> entry = do_QueryInterface(packageSkinEntry);
      if (entry) {
         // Obtain the real package resource.
         nsCOMPtr<nsIRDFNode> packageNode;
         rv = mChromeDataSource->GetTarget(entry, mPackage, PR_TRUE, getter_AddRefs(packageNode));
         if (NS_FAILED(rv)) {
           NS_ERROR("Unable to obtain the package resource.");
           return rv;
         }

         // Select the skin for this package resource.
         nsCOMPtr<nsIRDFResource> packageResource(do_QueryInterface(packageNode));
         if (packageResource) {
           PRBool isSet = PR_FALSE;
           rv = IsProviderSetForPackage(aProvider, packageResource, entry, aSelectionArc, aUseProfile, &isSet);
           if (NS_FAILED(rv)) {
             NS_ERROR("Unable to set provider for package resource.");
             return rv;
           }
           ++numPackages;
           if (isSet)
             ++numSet;
         }
      }
    }
    rv = arcs->HasMoreElements(&more);
    if (NS_FAILED(rv)) return rv;
  }
  if (numPackages == numSet)
    *aResult = FULL;
  else if (numSet)
    *aResult = PARTIAL;
  return NS_OK;
}

nsresult
nsChromeRegistry::IsProviderSelectedForPackage(const nsACString& aProviderType,
                                               const nsACString& aProviderName,
                                               const PRUnichar *aPackageName,
                                               nsIRDFResource* aSelectionArc,
                                               PRBool aUseProfile, PRBool* aResult)
{
  *aResult = PR_FALSE;
  nsCAutoString package( "urn:mozilla:package:" );
  package.AppendWithConversion(aPackageName);

  nsCAutoString provider( "urn:mozilla:" );
  provider += aProviderType;
  provider += ":";
  provider += aProviderName;
  provider += ":";
  provider.AppendWithConversion(aPackageName);

  // Obtain the package resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> packageResource;
  rv = GetResource(package, getter_AddRefs(packageResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }
  NS_ASSERTION(packageResource, "failed to get packageResource");

  // Obtain the provider resource.
  nsCOMPtr<nsIRDFResource> providerResource;
  rv = GetResource(provider, getter_AddRefs(providerResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the provider resource.");
    return rv;
  }
  NS_ASSERTION(providerResource, "failed to get providerResource");

  return IsProviderSetForPackage(aProviderType, packageResource, providerResource, aSelectionArc,
                                 aUseProfile, aResult);
}

nsresult
nsChromeRegistry::IsProviderSetForPackage(const nsACString& aProvider,
                                          nsIRDFResource* aPackageResource,
                                          nsIRDFResource* aProviderPackageResource,
                                          nsIRDFResource* aSelectionArc,
                                          PRBool aUseProfile, PRBool* aResult)
{
  nsresult rv;
  // Figure out which file we're needing to modify, e.g., is it the install
  // dir or the profile dir, and get the right datasource.
  
  nsCOMPtr<nsIRDFDataSource> dataSource;
  rv = LoadDataSource(kChromeFileName, getter_AddRefs(dataSource), aUseProfile, nsnull);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFNode> retVal;
  dataSource->GetTarget(aPackageResource, aSelectionArc, PR_TRUE, getter_AddRefs(retVal));
  if (retVal) {
    nsCOMPtr<nsIRDFNode> node(do_QueryInterface(aProviderPackageResource));
    if (node == retVal)
      *aResult = PR_TRUE;
  }

  return NS_OK;
}

nsresult
nsChromeRegistry::InstallProvider(const nsACString& aProviderType,
                                  const nsACString& aBaseURL,
                                  PRBool aUseProfile, PRBool aAllowScripts,
                                  PRBool aRemove)
{
  // XXX don't allow local chrome overrides of install chrome!
#ifdef DEBUG
  printf("*** Chrome Registration of %-7s: Checking for contents.rdf at %s\n", PromiseFlatCString(aProviderType).get(), PromiseFlatCString(aBaseURL).get());
#endif

  // Load the data source found at the base URL.
  nsCOMPtr<nsIRDFDataSource> dataSource;
  nsresult rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                                   nsnull,
                                                   NS_GET_IID(nsIRDFDataSource),
                                                   (void**) getter_AddRefs(dataSource));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(dataSource, &rv);
  if (NS_FAILED(rv)) return rv;

  // We need to read this synchronously.
  nsCAutoString key(aBaseURL);
  key += "contents.rdf";
  remote->Init(key.get());
  remote->Refresh(PR_TRUE);

  PRBool skinCount = GetProviderCount(NS_LITERAL_CSTRING("skin"), dataSource);
  PRBool localeCount = GetProviderCount(NS_LITERAL_CSTRING("locale"), dataSource);
  PRBool packageCount = GetProviderCount(NS_LITERAL_CSTRING("package"), dataSource);

  PRBool appendPackage = PR_FALSE;
  PRBool appendProvider = PR_FALSE;
  PRBool appendProviderName = PR_FALSE;

  if (skinCount == 0 && localeCount == 0 && packageCount == 0) {
    // Try the old-style manifest.rdf instead
    key = aBaseURL;
    key += "manifest.rdf";
    (void)remote->Init(key.get());      // ignore failure here
    rv = remote->Refresh(PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    appendPackage = PR_TRUE;
    appendProvider = PR_TRUE;
    NS_WARNING("Trying old-style manifest.rdf. Please update to contents.rdf.");
  }
  else {
    if ((skinCount > 1 && aProviderType.Equals("skin")) ||
        (localeCount > 1 && aProviderType.Equals("locale")))
      appendProviderName = PR_TRUE;

    if (!appendProviderName && packageCount > 1) {
      appendPackage = PR_TRUE;
    }

    if (aProviderType.Equals("skin")) {
      if (!appendProviderName && (localeCount == 1 || packageCount != 0))
        appendProvider = PR_TRUE;
    }
    else if (aProviderType.Equals("locale")) {
      if (!appendProviderName && (skinCount == 1 || packageCount != 0))
        appendProvider = PR_TRUE;
    }
    else {
      // Package install.
      if (localeCount == 1 || skinCount == 1)
        appendProvider = PR_TRUE;
    }
  }

  // Load the install data source that we wish to manipulate.
  nsCOMPtr<nsIRDFDataSource> installSource;
  rv = LoadDataSource(kChromeFileName, getter_AddRefs(installSource), aUseProfile, nsnull);
  if (NS_FAILED(rv)) return rv;
  NS_ASSERTION(installSource, "failed to get installSource");

  // install our dynamic overlays
  if (aProviderType.Equals("package"))
    rv = UpdateDynamicDataSources(dataSource, PR_TRUE, aUseProfile, aRemove);
  else if (aProviderType.Equals("skin"))
    rv = UpdateDynamicDataSources(dataSource, PR_FALSE, aUseProfile, aRemove);
  if (NS_FAILED(rv)) return rv;

  // Get the literal for our loc type.
  nsAutoString locstr;
  if (aUseProfile)
    locstr.Assign(NS_LITERAL_STRING("profile"));
  else locstr.Assign(NS_LITERAL_STRING("install"));
  nsCOMPtr<nsIRDFLiteral> locLiteral;
  rv = mRDFService->GetLiteral(locstr.get(), getter_AddRefs(locLiteral));
  if (NS_FAILED(rv)) return rv;

  // Get the literal for our script access.
  nsCOMPtr<nsIRDFLiteral> scriptLiteral;
  rv = mRDFService->GetLiteral(NS_LITERAL_STRING("false").get(),
                               getter_AddRefs(scriptLiteral));
  if (NS_FAILED(rv)) return rv;

  // Build the prefix string. Only resources with this prefix string will have their
  // assertions copied.
  nsCAutoString prefix( "urn:mozilla:" );
  prefix += aProviderType;
  prefix += ":";

  // Get all the resources
  nsCOMPtr<nsISimpleEnumerator> resources;
  rv = dataSource->GetAllResources(getter_AddRefs(resources));
  if (NS_FAILED(rv)) return rv;

  // For each resource
  PRBool moreElements;
  rv = resources->HasMoreElements(&moreElements);
  if (NS_FAILED(rv)) return rv;

  while (moreElements) {
    nsCOMPtr<nsISupports> supports;
    rv = resources->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(supports);

    // Check against the prefix string
    const char* value;
    rv = resource->GetValueConst(&value);
    if (NS_FAILED(rv)) return rv;
    nsCAutoString val(value);
    if (val.Find(prefix) == 0) {
      // It's valid.

      if (aProviderType.Equals("package") && !val.Equals("urn:mozilla:package:root")) {
        // Add arcs for the base url and loctype
        // Get the value of the base literal.
        nsCAutoString baseURL(aBaseURL);

        // Peel off the package.
        const char* val2;
        rv = resource->GetValueConst(&val2);
        if (NS_FAILED(rv)) return rv;
        nsCAutoString value2(val2);
        PRInt32 index = value2.RFind(":");
        nsCAutoString packageName;
        value2.Right(packageName, value2.Length() - index - 1);

        if (appendPackage) {
          baseURL += packageName;
          baseURL += "/";
        }
        if (appendProvider) {
          baseURL += "content/";
        }

        nsCOMPtr<nsIRDFLiteral> baseLiteral;
        mRDFService->GetLiteral(NS_ConvertASCIItoUCS2(baseURL).get(), getter_AddRefs(baseLiteral));

        rv = nsChromeRegistry::UpdateArc(installSource, resource, mBaseURL, baseLiteral, aRemove);
        if (NS_FAILED(rv)) return rv;
        rv = nsChromeRegistry::UpdateArc(installSource, resource, mLocType, locLiteral, aRemove);
        if (NS_FAILED(rv)) return rv;
      }

      nsCOMPtr<nsIRDFContainer> container;
      rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                            nsnull,
                                            NS_GET_IID(nsIRDFContainer),
                                            getter_AddRefs(container));
      if (NS_FAILED(rv)) return rv;
      rv = container->Init(dataSource, resource);
      if (NS_SUCCEEDED(rv)) {
        // XXX Deal with BAGS and ALTs? Aww, to hell with it. Who cares? I certainly don't.
        // We're a SEQ. Different rules apply. Do an AppendElement instead.
        // First do the decoration in the install data source.
        nsCOMPtr<nsIRDFContainer> installContainer;
        rv = mRDFContainerUtils->MakeSeq(installSource, resource, getter_AddRefs(installContainer));
        if (NS_FAILED(rv)) return rv;
        if (!installContainer) {
          // Already exists. Create a container instead.
          rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                            nsnull,
                                            NS_GET_IID(nsIRDFContainer),
                                            getter_AddRefs(installContainer));
          if (NS_FAILED(rv)) return rv;
          rv = installContainer->Init(installSource, resource);
          if (NS_FAILED(rv)) return rv;
        }

        // Put all our elements into the install container.
        nsCOMPtr<nsISimpleEnumerator> seqKids;
        rv = container->GetElements(getter_AddRefs(seqKids));
        if (NS_FAILED(rv)) return rv;
        PRBool moreKids;
        rv = seqKids->HasMoreElements(&moreKids);
        if (NS_FAILED(rv)) return rv;
        while (moreKids) {
          nsCOMPtr<nsISupports> supp;
          rv = seqKids->GetNext(getter_AddRefs(supp));
          if (NS_FAILED(rv)) return rv;
          nsCOMPtr<nsIRDFNode> kid = do_QueryInterface(supp);
          if (aRemove) {
            rv = installContainer->RemoveElement(kid, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
          }
          else {
            PRInt32 index;
            rv = installContainer->IndexOf(kid, &index);
            if (NS_FAILED(rv)) return rv;
            if (index == -1) {
              rv = installContainer->AppendElement(kid);
              if (NS_FAILED(rv)) return rv;
            }
            rv = seqKids->HasMoreElements(&moreKids);
            if (NS_FAILED(rv)) return rv;
          }
        }

        // See if we're a packages seq in a skin/locale.  If so, we need to set up the baseURL, allowScripts
        // and package arcs.
        if (val.Find(":packages") != -1 && !aProviderType.EqualsLiteral("package")) {
          PRBool doAppendPackage = appendPackage;
          PRInt32 perProviderPackageCount;
          container->GetCount(&perProviderPackageCount);
          if (perProviderPackageCount > 1)
            doAppendPackage = PR_TRUE;

          // Iterate over our kids a second time.
          nsCOMPtr<nsISimpleEnumerator> seqKids2;
          rv = container->GetElements(getter_AddRefs(seqKids2));
          if (NS_FAILED(rv)) return rv;
          PRBool moreKids2;
          rv = seqKids2->HasMoreElements(&moreKids2);
          if (NS_FAILED(rv)) return rv;
          while (moreKids2) {
            nsCOMPtr<nsISupports> supp;
            rv = seqKids2->GetNext(getter_AddRefs(supp));
            if (NS_FAILED(rv)) return rv;
            nsCOMPtr<nsIRDFResource> entry(do_QueryInterface(supp));
            if (entry) {
              // Get the value of the base literal.
              nsCAutoString baseURL(aBaseURL);

              // Peel off the package and the provider.
              const char* val2;
              rv = entry->GetValueConst(&val2);
              if (NS_FAILED(rv)) return rv;
              nsCAutoString value2(val2);
              PRInt32 index = value2.RFind(":");
              nsCAutoString packageName;
              value2.Right(packageName, value2.Length() - index - 1);
              nsCAutoString remainder;
              value2.Left(remainder, index);

              nsCAutoString providerName;
              index = remainder.RFind(":");
              remainder.Right(providerName, remainder.Length() - index - 1);

              // Append them to the base literal and tack on a final slash.
              if (appendProviderName) {
                baseURL += providerName;
                baseURL += "/";
              }
              if (doAppendPackage) {
                baseURL += packageName;
                baseURL += "/";
              }
              if (appendProvider) {
                baseURL += aProviderType;
                baseURL += "/";
              }

              nsCOMPtr<nsIRDFLiteral> baseLiteral;
              mRDFService->GetLiteral(NS_ConvertASCIItoUCS2(baseURL).get(), getter_AddRefs(baseLiteral));

              rv = nsChromeRegistry::UpdateArc(installSource, entry, mBaseURL, baseLiteral, aRemove);
              if (NS_FAILED(rv)) return rv;
              if (aProviderType.EqualsLiteral("skin") && !aAllowScripts) {
                rv = nsChromeRegistry::UpdateArc(installSource, entry, mAllowScripts, scriptLiteral, aRemove);
                if (NS_FAILED(rv)) return rv;
              }

              // Now set up the package arc.
              if (index != -1) {
                // Peel off the package name.

                nsCAutoString resourceName("urn:mozilla:package:");
                resourceName += packageName;
                nsCOMPtr<nsIRDFResource> packageResource;
                rv = GetResource(resourceName, getter_AddRefs(packageResource));
                if (NS_FAILED(rv)) return rv;
                if (packageResource) {
                  rv = nsChromeRegistry::UpdateArc(installSource, entry, mPackage, packageResource, aRemove);
                  if (NS_FAILED(rv)) return rv;
                }
              }
            }

            rv = seqKids2->HasMoreElements(&moreKids2);
            if (NS_FAILED(rv)) return rv;
          }
        }
      }
      else {
        // We're not a seq. Get all of the arcs that go out.
        nsCOMPtr<nsISimpleEnumerator> arcs;
        rv = dataSource->ArcLabelsOut(resource, getter_AddRefs(arcs));
        if (NS_FAILED(rv)) return rv;

        PRBool moreArcs;
        rv = arcs->HasMoreElements(&moreArcs);
        if (NS_FAILED(rv)) return rv;
        while (moreArcs) {
          nsCOMPtr<nsISupports> supp;
          rv = arcs->GetNext(getter_AddRefs(supp));
          if (NS_FAILED(rv)) return rv;
          nsCOMPtr<nsIRDFResource> arc = do_QueryInterface(supp);

          if (arc == mPackages) {
            // We are the main entry for a skin/locale.
            // Set up our loctype and our script access
            rv = nsChromeRegistry::UpdateArc(installSource, resource, mLocType, locLiteral, aRemove);
            if (NS_FAILED(rv)) return rv;
          }

          nsCOMPtr<nsIRDFNode> newTarget;
          rv = dataSource->GetTarget(resource, arc, PR_TRUE, getter_AddRefs(newTarget));
          if (NS_FAILED(rv)) return rv;

          if (arc == mImage) {
            // We are an image URL.  Check to see if we're a relative URL.
            nsCOMPtr<nsIRDFLiteral> literal(do_QueryInterface(newTarget));
            if (literal) {
              const PRUnichar* valueStr;
              literal->GetValueConst(&valueStr);
              nsAutoString imageURL(valueStr);
              if (imageURL.FindChar(':') == -1) {
                // We're relative. Prepend the base URL of the
                // package.
                NS_ConvertUTF8toUCS2 fullURL(aBaseURL);
                fullURL += imageURL;
                mRDFService->GetLiteral(fullURL.get(), getter_AddRefs(literal));
                newTarget = do_QueryInterface(literal);
              }
            }
          }

          rv = nsChromeRegistry::UpdateArc(installSource, resource, arc, newTarget, aRemove);
          if (NS_FAILED(rv)) return rv;

          rv = arcs->HasMoreElements(&moreArcs);
          if (NS_FAILED(rv)) return rv;
        }
      }
    }
    rv = resources->HasMoreElements(&moreElements);
    if (NS_FAILED(rv)) return rv;
  }

  // Flush the install source
  nsCOMPtr<nsIRDFRemoteDataSource> remoteInstall = do_QueryInterface(installSource, &rv);
  if (NS_FAILED(rv))
    return NS_OK;

  if (!mBatchInstallFlushes)
    rv = remoteInstall->Flush();

  // XXX Handle the installation of overlays.

  return rv;
}

NS_IMETHODIMP nsChromeRegistry::SetAllowOverlaysForPackage(const PRUnichar *aPackageName, PRBool allowOverlays)
{
  nsCAutoString package("urn:mozilla:package:");
  package.AppendWithConversion(aPackageName);

  // Obtain the package resource.
  nsCOMPtr<nsIRDFResource> packageResource;
  nsresult rv = GetResource(package, getter_AddRefs(packageResource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }
  NS_ASSERTION(packageResource, "failed to get packageResource");

  nsCOMPtr<nsIRDFDataSource> dataSource;
  rv = LoadDataSource(kChromeFileName, getter_AddRefs(dataSource), PR_TRUE, nsnull);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFLiteral> trueLiteral;
  mRDFService->GetLiteral(NS_LITERAL_STRING("true").get(), getter_AddRefs(trueLiteral));
  nsChromeRegistry::UpdateArc(dataSource, packageResource, 
                              mDisabled, 
                              trueLiteral, allowOverlays);

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(dataSource, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = remote->Flush();
  return rv;
}

PRBool nsChromeRegistry::IsOverlayAllowed(nsIURI *aChromeURL)
{
  nsCAutoString package, provider, file;
  nsresult rv = SplitURL(aChromeURL, package, provider, file);
  if (NS_FAILED(rv)) return PR_FALSE;

  // Get the chrome resource for the package.
  nsCAutoString rdfpackage( "urn:mozilla:package:" );
  rdfpackage.Append(package);

  // Obtain the package resource.
  nsCOMPtr<nsIRDFResource> packageResource;
  rv = GetResource(rdfpackage, getter_AddRefs(packageResource));
  if (NS_FAILED(rv) || !packageResource) {
    NS_ERROR("Unable to obtain the package resource.");
    return PR_FALSE;
  }

  // See if the disabled arc is set for the package.
  nsCAutoString disabled;
  nsChromeRegistry::FollowArc(mChromeDataSource, disabled, packageResource, mDisabled);
  return disabled.IsEmpty();
}

NS_IMETHODIMP nsChromeRegistry::InstallSkin(const char* aBaseURL, PRBool aUseProfile, PRBool aAllowScripts)
{
  return InstallProvider(NS_LITERAL_CSTRING("skin"),
                         nsDependentCString(aBaseURL),
                         aUseProfile, aAllowScripts, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::InstallLocale(const char* aBaseURL, PRBool aUseProfile)
{
  return InstallProvider(NS_LITERAL_CSTRING("locale"),
                         nsDependentCString(aBaseURL),
                         aUseProfile, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::InstallPackage(const char* aBaseURL, PRBool aUseProfile)
{
  return InstallProvider(NS_LITERAL_CSTRING("package"),
                         nsDependentCString(aBaseURL),
                         aUseProfile, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP nsChromeRegistry::UninstallSkin(const nsACString& aSkinName, PRBool aUseProfile)
{
  // The skin must first be deselected.
  DeselectSkin(aSkinName, aUseProfile);

  // Now uninstall it.
  return UninstallProvider(NS_LITERAL_CSTRING("skin"), aSkinName, aUseProfile);
}

NS_IMETHODIMP nsChromeRegistry::UninstallLocale(const nsACString& aLocaleName, PRBool aUseProfile)
{
  // The locale must first be deselected.
  DeselectLocale(aLocaleName, aUseProfile);

  return UninstallProvider(NS_LITERAL_CSTRING("locale"), aLocaleName, aUseProfile);
}

NS_IMETHODIMP nsChromeRegistry::UninstallPackage(const nsACString& aPackageName, PRBool aUseProfile)
{
  NS_ERROR("XXX Write me!\n");
  return NS_ERROR_FAILURE;
}

nsresult
nsChromeRegistry::UninstallProvider(const nsACString& aProviderType,
                                    const nsACString& aProviderName,
                                    PRBool aUseProfile)
{
  // XXX We are going to simply do a snip of the arc from the seq ROOT to
  // the associated package.  waterson is going to provide the ability to name
  // roots in a datasource, and only resources that are reachable from the
  // root will be saved.
  nsresult rv = NS_OK;
  nsCAutoString prefix( "urn:mozilla:" );
  prefix += aProviderType;
  prefix += ":";

  // Obtain the root.
  nsCAutoString providerRoot(prefix);
  providerRoot += "root";

  // Obtain the child we wish to remove.
  nsCAutoString specificChild(prefix);
  specificChild += aProviderName;

  // Instantiate the data source we wish to modify.
  nsCOMPtr<nsIRDFDataSource> installSource;
  rv = LoadDataSource(kChromeFileName, getter_AddRefs(installSource), aUseProfile, nsnull);
  if (NS_FAILED(rv)) return rv;
  NS_ASSERTION(installSource, "failed to get installSource");

  // Now make a container out of the root seq.
  nsCOMPtr<nsIRDFContainer> container(do_CreateInstance("@mozilla.org/rdf/container;1"));

  // Get the resource for the root.
  nsCOMPtr<nsIRDFResource> chromeResource;
  if (NS_FAILED(rv = GetResource(providerRoot, getter_AddRefs(chromeResource)))) {
    NS_ERROR("Unable to retrieve the resource corresponding to the skin/locale root.");
    return rv;
  }

  if (NS_FAILED(container->Init(installSource, chromeResource)))
    return NS_ERROR_FAILURE;

  // Get the resource for the child.
  nsCOMPtr<nsIRDFResource> childResource;
  if (NS_FAILED(rv = GetResource(specificChild, getter_AddRefs(childResource)))) {
    NS_ERROR("Unable to retrieve the resource corresponding to the skin/locale child being removed.");
    return rv;
  }

  // Remove the child from the container.
  container->RemoveElement(childResource, PR_TRUE);

  // Now flush the datasource.
  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(installSource);
  remote->Flush();

  return NS_OK;
}

nsresult
nsChromeRegistry::GetProfileRoot(nsACString& aFileURL)
{
   nsresult rv;
   nsCOMPtr<nsIFile> userChromeDir;

   // Build a fileSpec that points to the destination
   // (profile dir + chrome)
   rv = NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR, getter_AddRefs(userChromeDir));
   if (NS_FAILED(rv) || !userChromeDir)
     return NS_ERROR_FAILURE;

   PRBool exists;
   rv = userChromeDir->Exists(&exists);
   if (NS_SUCCEEDED(rv) && !exists) {
     rv = userChromeDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
     if (NS_SUCCEEDED(rv)) {
       // now we need to put the userContent.css and userChrome.css
       // stubs into place

       // first get the locations of the defaults
       nsCOMPtr<nsIFile> defaultUserContentFile;
       nsCOMPtr<nsIFile> defaultUserChromeFile;
       rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR,
                                   getter_AddRefs(defaultUserContentFile));
       if (NS_FAILED(rv))
         rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR,
                                     getter_AddRefs(defaultUserContentFile));
       if (NS_FAILED(rv))
         return(rv);
       rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_50_DIR,
                                   getter_AddRefs(defaultUserChromeFile));
       if (NS_FAILED(rv))
         rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DEFAULTS_NLOC_50_DIR,
                                     getter_AddRefs(defaultUserChromeFile));
       if (NS_FAILED(rv))
         return(rv);
       defaultUserContentFile->AppendNative(NS_LITERAL_CSTRING("chrome"));
       defaultUserContentFile->AppendNative(NS_LITERAL_CSTRING("userContent-example.css"));
       defaultUserChromeFile->AppendNative(NS_LITERAL_CSTRING("chrome"));
       defaultUserChromeFile->AppendNative(NS_LITERAL_CSTRING("userChrome-example.css"));

       const nsAFlatCString& empty = EmptyCString();

       // copy along
       // It aint an error if these files dont exist
       defaultUserContentFile->CopyToNative(userChromeDir, empty);
       defaultUserChromeFile->CopyToNative(userChromeDir, empty);
     }
   }
   if (NS_FAILED(rv))
     return rv;

   return NS_GetURLSpecFromFile(userChromeDir, aFileURL);
}

nsresult
nsChromeRegistry::GetInstallRoot(nsIFile** aFileURL)
{
  return NS_GetSpecialDirectory(NS_APP_CHROME_DIR, aFileURL);
}

void
nsChromeRegistry::FlushAllCaches()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ASSERTION(obsSvc, "Couldn't get observer service.");

  obsSvc->NotifyObservers((nsIChromeRegistry*) this,
                          NS_CHROME_FLUSH_TOPIC, nsnull);

}  

// xxxbsmedberg Move me to nsIWindowMediator
NS_IMETHODIMP
nsChromeRegistry::ReloadChrome()
{
  FlushAllCaches();
  // Do a reload of all top level windows.
  nsresult rv = NS_OK;

  // Get the window mediator
  nsCOMPtr<nsIWindowMediator> windowMediator = do_GetService(kWindowMediatorCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

    rv = windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator));
    if (NS_SUCCEEDED(rv)) {
      // Get each dom window
      PRBool more;
      rv = windowEnumerator->HasMoreElements(&more);
      if (NS_FAILED(rv)) return rv;
      while (more) {
        nsCOMPtr<nsISupports> protoWindow;
        rv = windowEnumerator->GetNext(getter_AddRefs(protoWindow));
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIDOMWindowInternal> domWindow =
            do_QueryInterface(protoWindow);
          if (domWindow) {
            nsCOMPtr<nsIDOMLocation> location;
            domWindow->GetLocation(getter_AddRefs(location));
            if (location) {
              rv = location->Reload(PR_FALSE);
              if (NS_FAILED(rv)) return rv;
            }
          }
        }
        rv = windowEnumerator->HasMoreElements(&more);
        if (NS_FAILED(rv)) return rv;
      }
    }
  }
  return rv;
}

nsresult
nsChromeRegistry::GetArcs(nsIRDFDataSource* aDataSource,
                          const nsACString& aType,
                          nsISimpleEnumerator** aResult)
{
  nsCOMPtr<nsIRDFContainer> container;
  nsresult rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv))
    return NS_OK;

  nsCAutoString lookup("chrome:");
  lookup += aType;

  // Get the chromeResource from this lookup string
  nsCOMPtr<nsIRDFResource> chromeResource;
  if (NS_FAILED(rv = GetResource(lookup, getter_AddRefs(chromeResource)))) {
    NS_ERROR("Unable to retrieve the resource corresponding to the chrome skin or content.");
    return rv;
  }

  if (NS_FAILED(container->Init(aDataSource, chromeResource)))
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  if (NS_FAILED(container->GetElements(getter_AddRefs(arcs))))
    return NS_OK;

  *aResult = arcs;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

nsresult
nsChromeRegistry::AddToCompositeDataSource(PRBool aUseProfile)
{
  nsresult rv = NS_OK;
  if (!mChromeDataSource) {
    rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/datasource;1?name=composite-datasource",
                                            nsnull,
                                            NS_GET_IID(nsIRDFCompositeDataSource),
                                            getter_AddRefs(mChromeDataSource));
    if (NS_FAILED(rv))
      return rv;

    // Also create and hold on to our UI data source.
    rv = NS_NewChromeUIDataSource(mChromeDataSource, getter_AddRefs(mUIDataSource));
    if (NS_FAILED(rv)) return rv;
  }

  if (aUseProfile) {
    // Profiles take precedence.  Load them first.
    nsCOMPtr<nsIRDFDataSource> dataSource;
    LoadDataSource(kChromeFileName, getter_AddRefs(dataSource), PR_TRUE, nsnull);
    mChromeDataSource->AddDataSource(dataSource);
  }

  // Always load the install dir datasources
  LoadDataSource(kChromeFileName, getter_AddRefs(mInstallDirChromeDataSource), PR_FALSE, nsnull);
  mChromeDataSource->AddDataSource(mInstallDirChromeDataSource);
  
  return NS_OK;
}

nsresult nsChromeRegistry::LoadStyleSheetWithURL(nsIURI* aURL, nsICSSStyleSheet** aSheet)
{
  *aSheet = nsnull;

  nsCOMPtr<nsICSSLoader> cssLoader = do_GetService(kCSSLoaderCID);
  if (!cssLoader) return NS_ERROR_FAILURE;

  return cssLoader->LoadAgentSheet(aURL, aSheet);
}

nsresult nsChromeRegistry::LoadInstallDataSource()
{
  nsCOMPtr<nsIFile> installRootFile;
  
  nsresult rv = GetInstallRoot(getter_AddRefs(installRootFile));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = NS_GetURLSpecFromFile(installRootFile, mInstallRoot);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mInstallInitialized = PR_TRUE;
  return AddToCompositeDataSource(PR_FALSE);
}

nsresult nsChromeRegistry::LoadProfileDataSource()
{
  nsresult rv = GetProfileRoot(mProfileRoot);
  if (NS_SUCCEEDED(rv)) {
    // Load the profile search path for skins, content, and locales
    // Prepend them to our list of substitutions.
    mProfileInitialized = mInstallInitialized = PR_TRUE;
    mChromeDataSource = nsnull;
    rv = AddToCompositeDataSource(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    // XXX this sucks ASS. This is a temporary hack until we get
    // around to fixing the skin switching bugs.
    // Select and Remove skins based on a pref set in a previous session.
    nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefBranch) {
      nsXPIDLCString skinToSelect;
      rv = prefBranch->GetCharPref("general.skins.selectedSkin", getter_Copies(skinToSelect));
      if (NS_SUCCEEDED(rv)) {
        rv = SelectSkin(skinToSelect, PR_TRUE);
        if (NS_SUCCEEDED(rv))
          prefBranch->DeleteBranch("general.skins.selectedSkin");
      }
    }

    // We have to flush the chrome skin cache...
    FlushSkinCaches();
  }
  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::AllowScriptsForSkin(nsIURI* aChromeURI, PRBool *aResult)
{
  *aResult = PR_TRUE;

  // split the url
  nsCAutoString package, provider, file;
  nsresult rv;
  rv = SplitURL(aChromeURI, package, provider, file);
  if (NS_FAILED(rv)) return NS_OK;

  // verify it's a skin url
  if (!provider.Equals("skin"))
    return NS_OK;

  // XXX could factor this with selectproviderforpackage
  // get the selected skin resource for the package
  nsCOMPtr<nsIRDFNode> selectedProvider;

  nsCAutoString resourceStr("urn:mozilla:package:");
  resourceStr += package;

  // Obtain the resource.
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(resourceStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }

  if (NS_FAILED(rv = mChromeDataSource->GetTarget(resource, mSelectedSkin, PR_TRUE, getter_AddRefs(selectedProvider))))
    return NS_OK;

  if (!selectedProvider) {
    rv = FindProvider(package, provider, mSelectedSkin, getter_AddRefs(selectedProvider));
    if (NS_FAILED(rv)) return rv;
  }
  if (!selectedProvider)
    return NS_OK;

  resource = do_QueryInterface(selectedProvider, &rv);
  if (NS_SUCCEEDED(rv)) {
    // get its script access
    nsCAutoString scriptAccess;
    rv = nsChromeRegistry::FollowArc(mChromeDataSource,
                                     scriptAccess,
                                     resource,
                                     mAllowScripts);
    if (NS_FAILED(rv)) return rv;

    if (!scriptAccess.IsEmpty())
      *aResult = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::CheckForNewChrome()
{
  nsresult rv;

  rv = LoadInstallDataSource();
  if (NS_FAILED(rv)) return rv;

  // open the installed-chrome file
  nsCOMPtr<nsILocalFile> listFile;
  nsCOMPtr<nsIProperties> directoryService =
           do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;
  rv = directoryService->Get(NS_APP_CHROME_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(listFile));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIFile> chromeFile;
  rv = listFile->Clone(getter_AddRefs(chromeFile));
  if (NS_FAILED(rv)) return rv;
  rv = chromeFile->AppendNative(kChromeFileName);
  if (NS_FAILED(rv)) return rv;
  // XXXldb For the case where the file is nonexistent, we're depending
  // on the fact that the nsInt64 constructor initializes to 0 and
  // |GetLastModifiedTime| doesn't touch the out parameter.
  nsInt64 chromeDate;
  (void)chromeFile->GetLastModifiedTime(&chromeDate.mValue);

  rv = listFile->AppendRelativeNativePath(kInstalledChromeFileName);
  if (NS_FAILED(rv)) return rv;
  nsInt64 listFileDate;
  (void)listFile->GetLastModifiedTime(&listFileDate.mValue);

  if (listFileDate < chromeDate)
    return NS_OK;

  PRFileDesc *file;
  rv = listFile->OpenNSPRFileDesc(PR_RDONLY, 0, &file);
  if (NS_FAILED(rv)) return rv;

  // file is open.

  PRFileInfo finfo;

  if (PR_GetOpenFileInfo(file, &finfo) == PR_SUCCESS) {
    char *dataBuffer = new char[finfo.size+1];
    if (dataBuffer) {
      PRInt32 bufferSize = PR_Read(file, dataBuffer, finfo.size);
      if (bufferSize > 0) {
        rv = ProcessNewChromeBuffer(dataBuffer, bufferSize);
      }
      delete [] dataBuffer;
    }
  }
  PR_Close(file);
  // listFile->Remove(PR_FALSE);

  return rv;
}

// flaming unthreadsafe function
nsresult
nsChromeRegistry::ProcessNewChromeBuffer(char *aBuffer, PRInt32 aLength)
{
  nsresult rv = NS_OK;
  char   *bufferEnd = aBuffer + aLength;
  char   *chromeType,      // "content", "locale" or "skin"
         *chromeProfile,   // "install" or "profile"
         *chromeLocType,   // type of location (local path or URL)
         *chromeLocation;  // base location of chrome (jar file)
  PRBool isProfile;
  PRBool isSelection;

  NS_NAMED_LITERAL_CSTRING(content, "content");
  NS_NAMED_LITERAL_CSTRING(locale, "locale");
  NS_NAMED_LITERAL_CSTRING(skin, "skin");
  NS_NAMED_LITERAL_CSTRING(profile, "profile");
  NS_NAMED_LITERAL_CSTRING(select, "select");
  NS_NAMED_LITERAL_CSTRING(path, "path");
  nsCAutoString fileURL;
  nsCAutoString chromeURL;

  mBatchInstallFlushes = PR_TRUE;

  // process chromeType, chromeProfile, chromeLocType, chromeLocation
  while (aBuffer < bufferEnd) {
    // parse one line of installed-chrome.txt
    chromeType = aBuffer;
    while (aBuffer < bufferEnd && *aBuffer != ',')
      ++aBuffer;
    *aBuffer = '\0';

    chromeProfile = ++aBuffer;
    if (aBuffer >= bufferEnd)
      break;

    while (aBuffer < bufferEnd && *aBuffer != ',')
      ++aBuffer;
    *aBuffer = '\0';

    chromeLocType = ++aBuffer;
    if (aBuffer >= bufferEnd)
      break;

    while (aBuffer < bufferEnd && *aBuffer != ',')
      ++aBuffer;
    *aBuffer = '\0';

    chromeLocation = ++aBuffer;
    if (aBuffer >= bufferEnd)
      break;

    while (aBuffer < bufferEnd &&
           (*aBuffer != '\r' && *aBuffer != '\n' && *aBuffer != ' '))
      ++aBuffer;
    *aBuffer = '\0';

    // process the parsed line
    isSelection = select.Equals(chromeLocType);
    isProfile = profile.Equals(chromeProfile);
    if (isProfile && !mProfileInitialized) 
    { // load profile chrome.rdf only if needed
      rv = LoadProfileDataSource();
      if (NS_FAILED(rv)) 
        return rv;
    }

    if (path.Equals(chromeLocType)) {
      // location is a (full) path. convert it to an URL.

      /* this is some convoluted shit... this creates a file, inits it with
       * the path parsed above (chromeLocation), makes a url, and inits it
       * with the file created. the purpose of this is just to have the
       * canonical url of the stupid thing.
       */
      nsCOMPtr<nsILocalFile> chromeFile(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
      if (NS_FAILED(rv))
        return rv;
      // assuming chromeLocation is given in the charset of the current locale
      rv = chromeFile->InitWithNativePath(nsDependentCString(chromeLocation));
      if (NS_FAILED(rv))
        return rv;

      /* 
       * all we want here is the canonical url
       */
      rv = NS_GetURLSpecFromFile(chromeFile, chromeURL);
      if (NS_FAILED(rv)) return rv;

      /* if we're a file, we must be a jar file. do appropriate string munging.
       * otherwise, the string we got from GetSpec is fine.
       */
      PRBool isFile;
      rv = chromeFile->IsFile(&isFile);
      if (NS_FAILED(rv))
        return rv;

      if (isFile) {
        fileURL = "jar:";
        fileURL += chromeURL;
        fileURL += "!/";
        chromeURL = fileURL;
      }
    }
    else {
      // not "path". assume "url".
      chromeURL = chromeLocation;
    }

    // process the line
    if (skin.Equals(chromeType)) {
      if (isSelection) {

        rv = SelectSkin(nsDependentCString(chromeLocation), isProfile);
#ifdef DEBUG
        printf("***** Chrome Registration: Selecting skin %s as default\n", (const char*)chromeLocation);
#endif
      }
      else
        rv = InstallSkin(chromeURL.get(), isProfile, PR_FALSE);
    }
    else if (content.Equals(chromeType))
      rv = InstallPackage(chromeURL.get(), isProfile);
    else if (locale.Equals(chromeType)) {
      if (isSelection) {

        rv = SelectLocale(nsDependentCString(chromeLocation), isProfile);
#ifdef DEBUG
        printf("***** Chrome Registration: Selecting locale %s as default\n", (const char*)chromeLocation);
#endif
      }
      else
        rv = InstallLocale(chromeURL.get(), isProfile);
    }
    
    while (aBuffer < bufferEnd && (*aBuffer == '\0' || *aBuffer == ' ' || *aBuffer == '\r' || *aBuffer == '\n'))
      ++aBuffer;
  }

  mBatchInstallFlushes = PR_FALSE;
  nsCOMPtr<nsIRDFDataSource> dataSource;
  LoadDataSource(kChromeFileName, getter_AddRefs(dataSource), PR_FALSE, nsnull);
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(dataSource));
  remote->Flush();
  return NS_OK;
}

PRBool
nsChromeRegistry::GetProviderCount(const nsACString& aProviderType, nsIRDFDataSource* aDataSource)
{
  nsresult rv;

  nsCAutoString rootStr("urn:mozilla:");
  rootStr += aProviderType;
  rootStr += ":root";

  // obtain the provider root resource
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(rootStr, getter_AddRefs(resource));
  if (NS_FAILED(rv))
    return 0;

  // wrap it in a container
  nsCOMPtr<nsIRDFContainer> container;
  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv)) return 0;

  rv = container->Init(aDataSource, resource);
  if (NS_FAILED(rv)) return 0;

  PRInt32 count;
  container->GetCount(&count);
  return count;
}


NS_IMETHODIMP nsChromeRegistry::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!strcmp("profile-before-change", aTopic)) {

    mChromeDataSource = nsnull;
    mInstallInitialized = mProfileInitialized = PR_FALSE;

    if (!strcmp("shutdown-cleanse", NS_ConvertUCS2toUTF8(someData).get())) {
      nsCOMPtr<nsIFile> userChromeDir;
      rv = NS_GetSpecialDirectory(NS_APP_USER_CHROME_DIR, getter_AddRefs(userChromeDir));
      if (NS_SUCCEEDED(rv) && userChromeDir)
        rv = userChromeDir->Remove(PR_TRUE);
    }
  }
  else if (!strcmp("profile-after-change", aTopic)) {
    if (!mProfileInitialized) {
      rv = LoadProfileDataSource();
    }
  }

  return rv;
}

NS_IMETHODIMP nsChromeRegistry::CheckThemeVersion(const nsACString& aSkin,
                                                  PRBool* aResult)
{
  return CheckProviderVersion(NS_LITERAL_CSTRING("skin"), aSkin, mSkinVersion, aResult);
}


NS_IMETHODIMP nsChromeRegistry::CheckLocaleVersion(const nsACString& aLocale,
                                                   PRBool* aResult)
{
  nsCAutoString provider("locale");
  return CheckProviderVersion(NS_LITERAL_CSTRING("skin"), aLocale, mLocaleVersion, aResult);
}


nsresult
nsChromeRegistry::CheckProviderVersion (const nsACString& aProviderType,
                                        const nsACString& aProviderName,
                                        nsIRDFResource* aSelectionArc,
                                        PRBool *aCompatible)
{
  *aCompatible = PR_TRUE;

  // Build the provider resource str.
  // e.g., urn:mozilla:skin:aqua/1.0
  nsCAutoString resourceStr( "urn:mozilla:" );
  resourceStr += aProviderType;
  resourceStr += ":";
  resourceStr += aProviderName;

  // Obtain the provider resource.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFResource> resource;
  rv = GetResource(resourceStr, getter_AddRefs(resource));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the package resource.");
    return rv;
  }

  // Follow the packages arc to the package resources.
  nsCOMPtr<nsIRDFNode> packageList;
  rv = mChromeDataSource->GetTarget(resource, mPackages, PR_TRUE, getter_AddRefs(packageList));
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to obtain the SEQ for the package list.");
    return rv;
  }
  // ok for packageList to be null here -- it just means that we haven't encountered that package yet

  nsCOMPtr<nsIRDFResource> packageSeq(do_QueryInterface(packageList, &rv));
  if (NS_FAILED(rv)) return rv;

  // Build an RDF container to wrap the SEQ
  nsCOMPtr<nsIRDFContainer> container;
  rv = nsComponentManager::CreateInstance("@mozilla.org/rdf/container;1",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv))
    return rv;

  rv = container->Init(mChromeDataSource, packageSeq);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISimpleEnumerator> arcs;

  rv = container->GetElements(getter_AddRefs(arcs));
  if (NS_FAILED(rv))
    return NS_OK;

  // For each skin-package entry, follow the arcs to the real package
  // resource.
  PRBool more;
  rv = arcs->HasMoreElements(&more);
  if (NS_FAILED(rv)) return rv;
  while (more) {
    nsCOMPtr<nsISupports> packageSkinEntry;
    rv = arcs->GetNext(getter_AddRefs(packageSkinEntry));
    if (NS_SUCCEEDED(rv) && packageSkinEntry) {
      nsCOMPtr<nsIRDFResource> entry = do_QueryInterface(packageSkinEntry);
      if (entry) {

        nsCAutoString themePackageVersion;
        nsChromeRegistry::FollowArc(mChromeDataSource, themePackageVersion, entry, aSelectionArc);

        // Obtain the real package resource.
        nsCOMPtr<nsIRDFNode> packageNode;
        rv = mChromeDataSource->GetTarget(entry, mPackage, PR_TRUE, getter_AddRefs(packageNode));
        if (NS_FAILED(rv)) {
          NS_ERROR("Unable to obtain the package resource.");
          return rv;
        }

        nsCOMPtr<nsIRDFResource> packageResource(do_QueryInterface(packageNode));
        if (packageResource) {

          nsCAutoString packageVersion;
          nsChromeRegistry::FollowArc(mChromeDataSource, packageVersion, packageResource, aSelectionArc);

          nsCAutoString packageName;
          nsChromeRegistry::FollowArc(mChromeDataSource, packageName, packageResource, mName);

          if (packageName.IsEmpty())
            // the package is not represented for current version, so ignore it
            *aCompatible = PR_TRUE;
          else {
            if (packageVersion.IsEmpty() && themePackageVersion.IsEmpty())
              *aCompatible = PR_TRUE;
            else {
              if (!packageVersion.IsEmpty() && !themePackageVersion.IsEmpty())
                *aCompatible = ( themePackageVersion.Equals(packageVersion));
              else
                *aCompatible = PR_FALSE;
            }
          }

           // if just one theme package is NOT compatible, the theme will be disabled
           if (!(*aCompatible))
             return NS_OK;

         }
      }
    }
    rv = arcs->HasMoreElements(&more);
    if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}

