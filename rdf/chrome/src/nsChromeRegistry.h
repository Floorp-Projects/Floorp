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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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

class nsIAtom;
class nsICSSStyleSheet;
class nsIRDFService;
class nsIRDFDataSource;
class nsIRDFResource;
class nsIRDFNode;
class nsISimpleEnumerator;
class nsSupportsHashtable;
class nsIRDFContainer;
class nsIRDFContainerUtils;
class nsIDOMWindowInternal;
class nsIDocument;

#include "nsIChromeRegistry.h"
#include "nsIChromeRegistrySea.h"
#include "nsIXULOverlayProvider.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsString.h"
#include "nsICSSLoader.h"
#include "nsIZipReader.h"
#include "nsCOMArray.h"
     
// for component registration
// {D8C7D8A2-E84C-11d2-BF87-00105A1B0627}
#define NS_CHROMEREGISTRY_CID \
{ 0xd8c7d8a2, 0xe84c, 0x11d2, { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }

class nsChromeRegistry : public nsIChromeRegistrySea,
                         public nsIXULOverlayProvider,
                         public nsIObserver,
                         public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS

  // nsIChromeRegistry methods:
  NS_DECL_NSICHROMEREGISTRY
  NS_DECL_NSIXULCHROMEREGISTRY
  NS_DECL_NSIXULOVERLAYPROVIDER
  NS_DECL_NSICHROMEREGISTRYSEA

  NS_DECL_NSIOBSERVER

  // nsChromeRegistry methods:
  nsChromeRegistry();
  virtual ~nsChromeRegistry();

  nsresult Init();

  // used by the protocol handler
  static nsresult Canonify(nsIURI* aChromeURL);

public:
  static nsresult FollowArc(nsIRDFDataSource *aDataSource,
                            nsACString& aResult,
                            nsIRDFResource* aChromeResource,
                            nsIRDFResource* aProperty);

  static nsresult UpdateArc(nsIRDFDataSource *aDataSource, nsIRDFResource* aSource, nsIRDFResource* aProperty, 
                            nsIRDFNode *aTarget, PRBool aRemove);

protected:
  nsresult GetDynamicDataSource(nsIURI *aChromeURL, PRBool aIsOverlay, PRBool aUseProfile, PRBool aCreateDS, nsIRDFDataSource **aResult);
  nsresult GetURIList(nsIRDFDataSource *aDS, nsIRDFResource *aResource, nsCOMArray<nsIURI>& aArray);
  nsresult GetDynamicInfo(nsIURI *aChromeURL, PRBool aIsOverlay, nsISimpleEnumerator **aResult);

  PRBool   IsOverlayAllowed(nsIURI *aChromeURI);

  nsresult GetResource(const nsACString& aChromeType, nsIRDFResource** aResult);
  
  nsresult UpdateDynamicDataSource(nsIRDFDataSource *aDataSource,
                                   nsIRDFResource *aResource,
                                   PRBool aIsOverlay, PRBool
                                   aUseProfile, PRBool aRemove);
  nsresult UpdateDynamicDataSources(nsIRDFDataSource *aDataSource,
                                    PRBool aIsOverlay,
                                    PRBool aUseProfile, PRBool
                                    aRemove);
  nsresult WriteInfoToDataSource(const char *aDocURI,
                                 const PRUnichar *aOverlayURI,
                                 PRBool aIsOverlay, PRBool
                                 aUseProfile, PRBool aRemove);
 
  nsresult LoadStyleSheetWithURL(nsIURI* aURL, PRBool aEnableUnsafeRules, nsICSSStyleSheet** aSheet);

  nsresult LoadInstallDataSource();
  nsresult LoadProfileDataSource();

  void FlushSkinCaches();
  void FlushAllCaches();

private:
  nsresult LoadDataSource(const nsACString &aFileName,
                          nsIRDFDataSource **aResult,
                          PRBool aUseProfileDirOnly = PR_FALSE,
                          const char *aProfilePath = nsnull);

  static nsresult GetProfileRoot(nsACString& aFileURL);
  static nsresult GetInstallRoot(nsIFile** aFileURL);

  nsresult RefreshWindow(nsIDOMWindowInternal* aWindow);

  nsresult GetArcs(nsIRDFDataSource* aDataSource,
                   const nsACString& aType,
                   nsISimpleEnumerator** aResult);

  nsresult AddToCompositeDataSource(PRBool aUseProfile);

  nsresult FlagXPCNativeWrappers();

  nsresult GetBaseURL(const nsACString& aPackage,
                      const nsACString& aProvider, 
                      nsACString& aBaseURL);

  nsresult InitOverrideJAR();
  nsresult GetOverrideURL(const nsACString& aPackage,
                          const nsACString& aProvider,
                          const nsACString& aPath,
                          nsACString& aResult);
  
  nsresult VerifyCompatibleProvider(nsIRDFResource* aPackageResource,
                                    nsIRDFResource* aProviderResource,
                                    nsIRDFResource* aArc,
                                    PRBool *aAcceptable);

  nsresult FindProvider(const nsACString& aPackage,
                        const nsACString& aProvider,
                        nsIRDFResource *aArc,
                        nsIRDFNode **aSelectedProvider);

  nsresult SelectPackageInProvider(nsIRDFResource *aPackageList,
                                   const nsACString& aPackage,
                                   const nsACString& aProvider,
                                   const nsACString& aProviderName,
                                   nsIRDFResource *aArc,
                                   nsIRDFNode **aSelectedProvider);

  nsresult SetProvider(const nsACString& aProvider,
                       nsIRDFResource* aSelectionArc,
                       const nsACString& aProviderName,
                       PRBool aAllUsers, 
                       const char *aProfilePath, 
                       PRBool aIsAdding);

  nsresult SetProviderForPackage(const nsACString& aProvider,
                                 nsIRDFResource* aPackageResource, 
                                 nsIRDFResource* aProviderPackageResource, 
                                 nsIRDFResource* aSelectionArc, 
                                 PRBool aAllUsers, const char *aProfilePath, 
                                 PRBool aIsAdding);

  nsresult SelectProviderForPackage(const nsACString& aProviderType,
                                    const nsACString& aProviderName, 
                                    const PRUnichar *aPackageName, 
                                    nsIRDFResource* aSelectionArc, 
                                    PRBool aUseProfile, PRBool aIsAdding);

  nsresult GetSelectedProvider(const nsACString& aPackage,
                               const nsACString& aProviderName,
                               nsIRDFResource* aSelectionArc,
                               nsACString& aResult);
  
  nsresult CheckProviderVersion (const nsACString& aProviderType,
                                 const nsACString& aProviderName,
                                 nsIRDFResource* aSelectionArc,
                                 PRBool *aCompatible);

  nsresult IsProviderSelected(const nsACString& aProvider,
                              const nsACString& aProviderName,
                              nsIRDFResource* aSelectionArc,
                              PRBool aUseProfile, PRInt32* aResult);
  
  nsresult IsProviderSelectedForPackage(const nsACString& aProviderType,
                                        const nsACString& aProviderName, 
                                        const PRUnichar *aPackageName, 
                                        nsIRDFResource* aSelectionArc, 
                                        PRBool aUseProfile, PRBool* aResult);
  nsresult IsProviderSetForPackage(const nsACString& aProvider,
                                     nsIRDFResource* aPackageResource, 
                                     nsIRDFResource* aProviderPackageResource, 
                                     nsIRDFResource* aSelectionArc, 
                                     PRBool aUseProfile, PRBool* aResult);

  nsresult InstallProvider(const nsACString& aProviderType,
                             const nsACString& aBaseURL,
                             PRBool aUseProfile, PRBool aAllowScripts, PRBool aRemove);
  nsresult UninstallProvider(const nsACString& aProviderType, const nsACString& aProviderName, PRBool aUseProfile);

  nsresult ProcessNewChromeBuffer(char *aBuffer, PRInt32 aLength);

  PRBool GetProviderCount(const nsACString& aProviderType, nsIRDFDataSource* aDataSource);

protected:
  nsCString mProfileRoot;
  nsCString mInstallRoot;

  nsCOMPtr<nsIRDFCompositeDataSource> mChromeDataSource;
  nsCOMPtr<nsIRDFDataSource> mInstallDirChromeDataSource;
  nsCOMPtr<nsIRDFDataSource> mUIDataSource;

  nsSupportsHashtable* mDataSourceTable;
  nsIRDFService* mRDFService;
  nsIRDFContainerUtils* mRDFContainerUtils;

  // Resources
  nsCOMPtr<nsIRDFResource> mSelectedSkin;
  nsCOMPtr<nsIRDFResource> mSelectedLocale;
  nsCOMPtr<nsIRDFResource> mBaseURL;
  nsCOMPtr<nsIRDFResource> mPackages;
  nsCOMPtr<nsIRDFResource> mPackage;
  nsCOMPtr<nsIRDFResource> mName;
  nsCOMPtr<nsIRDFResource> mImage;
  nsCOMPtr<nsIRDFResource> mLocType;
  nsCOMPtr<nsIRDFResource> mAllowScripts;
  nsCOMPtr<nsIRDFResource> mHasOverlays;
  nsCOMPtr<nsIRDFResource> mHasStylesheets;
  nsCOMPtr<nsIRDFResource> mSkinVersion;
  nsCOMPtr<nsIRDFResource> mLocaleVersion;
  nsCOMPtr<nsIRDFResource> mPackageVersion;
  nsCOMPtr<nsIRDFResource> mDisabled;
  nsCOMPtr<nsIRDFResource> mXPCNativeWrappers;

  nsCOMPtr<nsICSSLoader> mCSSLoader;
  nsCOMPtr<nsIZipReader> mOverrideJAR;
  nsCString              mOverrideJARURL;
  
  // useful atoms - these are static atoms, so don't use nsCOMPtr
  static nsIAtom* sCPrefix;            // "c"
  
  PRPackedBool mInstallInitialized;
  PRPackedBool mProfileInitialized;
  
  PRPackedBool mRuntimeProvider;

  // Boolean that indicates we should batch flushes of the main
  // chrome.rdf file.
  PRPackedBool mBatchInstallFlushes;

  // make sure we only look once for the JAR override
  PRPackedBool mSearchedForOverride;
  
  // if we find an old profile overlayinfo/ directory structure, use it
  // else use the new flat files
  PRPackedBool mLegacyOverlayinfo;
};
