/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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

class nsIRDFService;
class nsIRDFDataSource;
class nsIRDFResource;
class nsIRDFNode;
class nsICSSLoader;
class nsISimpleEnumerator;
class nsSupportsHashtable;
class nsIRDFContainer;
class nsIRDFContainerUtils;
class nsIDOMWindowInternal;
class nsIDocument;

#include "nsIRDFCompositeDataSource.h"
#include "nsICSSStyleSheet.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsString.h"

class nsChromeRegistry : public nsIChromeRegistry,
                         public nsIObserver,
                         public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS

  // nsIChromeRegistry methods:
  NS_DECL_NSICHROMEREGISTRY

  NS_DECL_NSIOBSERVER

  // nsChromeRegistry methods:
  nsChromeRegistry();
  virtual ~nsChromeRegistry();

  nsresult Init();

public:
  static nsresult FollowArc(nsIRDFDataSource *aDataSource,
                            nsCString& aResult, nsIRDFResource* aChromeResource,
                            nsIRDFResource* aProperty);

  static nsresult UpdateArc(nsIRDFDataSource *aDataSource, nsIRDFResource* aSource, nsIRDFResource* aProperty, 
                            nsIRDFNode *aTarget, PRBool aRemove);

protected:
  NS_IMETHOD GetDynamicDataSource(nsIURI *aChromeURL, PRBool aIsOverlay, PRBool aUseProfile, nsIRDFDataSource **aResult);
  NS_IMETHOD GetDynamicInfo(nsIURI *aChromeURL, PRBool aIsOverlay, nsISimpleEnumerator **aResult);

  nsresult GetResource(const nsCString& aChromeType, nsIRDFResource** aResult);
  
  NS_IMETHOD UpdateDynamicDataSource(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource, 
                                     PRBool aIsOverlay, PRBool aUseProfile, PRBool aRemove);
  NS_IMETHOD UpdateDynamicDataSources(nsIRDFDataSource *aDataSource, PRBool aIsOverlay, 
                                      PRBool aUseProfile, PRBool aRemove);
  NS_IMETHOD WriteInfoToDataSource(const char *aDocURI, const PRUnichar *aOverlayURI,
                                   PRBool aIsOverlay, PRBool aUseProfile, PRBool aRemove);
 
  nsresult LoadStyleSheet(nsICSSStyleSheet** aSheet, const nsCString & aURL);
  nsresult LoadStyleSheetWithURL(nsIURI* aURL, nsICSSStyleSheet** aSheet);
  
  nsresult GetUserSheetURL(PRBool aIsChrome, nsCString & aURL);
  nsresult GetFormSheetURL(nsCString& aURL);
  
  nsresult LoadInstallDataSource();
  nsresult LoadProfileDataSource();
  
  nsresult FlushCaches();

private:
  NS_IMETHOD LoadDataSource(const nsCString &aFileName, nsIRDFDataSource **aResult,
                            PRBool aUseProfileDirOnly = PR_FALSE, const char *aProfilePath = nsnull);

  NS_IMETHOD GetProfileRoot(nsCString& aFileURL);
  NS_IMETHOD GetInstallRoot(nsCString& aFileURL);

  NS_IMETHOD RefreshWindow(nsIDOMWindowInternal* aWindow);

  NS_IMETHOD GetArcs(nsIRDFDataSource* aDataSource,
                        const nsCString& aType,
                        nsISimpleEnumerator** aResult);

  NS_IMETHOD AddToCompositeDataSource(PRBool aUseProfile);
  
  NS_IMETHOD GetBaseURL(const nsCString& aPackage, const nsCString& aProvider, 
                             nsCString& aBaseURL);

  NS_IMETHOD FindProvider(const nsCString& aPackage,
                          const nsCString& aProvider,
                          nsIRDFResource *aArc,
                          nsIRDFNode **aSelectedProvider);

  NS_IMETHOD SelectPackageInProvider(nsIRDFResource *aPackageList,
                                   const nsCString& aPackage,
                                   const nsCString& aProvider,
                                   const nsCString& aProviderName,
                                   nsIRDFResource *aArc,
                                   nsIRDFNode **aSelectedProvider);

  NS_IMETHOD SetProvider(const nsCString& aProvider,
                         nsIRDFResource* aSelectionArc,
                         const PRUnichar* aProviderName,
                         PRBool aAllUsers, 
                         const char *aProfilePath, 
                         PRBool aIsAdding);

  NS_IMETHOD SetProviderForPackage(const nsCString& aProvider,
                                   nsIRDFResource* aPackageResource, 
                                   nsIRDFResource* aProviderPackageResource, 
                                   nsIRDFResource* aSelectionArc, 
                                   PRBool aAllUsers, const char *aProfilePath, 
                                   PRBool aIsAdding);

  NS_IMETHOD SelectProviderForPackage(const nsCString& aProviderType,
                                        const PRUnichar *aProviderName, 
                                        const PRUnichar *aPackageName, 
                                        nsIRDFResource* aSelectionArc, 
                                        PRBool aUseProfile, PRBool aIsAdding);

  NS_IMETHOD CheckProviderVersion (const nsCString& aProviderType,
                                      const PRUnichar* aProviderName,
                                      nsIRDFResource* aSelectionArc,
                                      PRBool *aCompatible);

  NS_IMETHOD IsProviderSelected(const nsCString& aProvider,
                                const PRUnichar* aProviderName,
                                nsIRDFResource* aSelectionArc,
                                PRBool aUseProfile, PRBool* aResult);
  NS_IMETHOD IsProviderSelectedForPackage(const nsCString& aProviderType,
                                          const PRUnichar *aProviderName, 
                                          const PRUnichar *aPackageName, 
                                          nsIRDFResource* aSelectionArc, 
                                          PRBool aUseProfile, PRBool* aResult);
  NS_IMETHOD IsProviderSetForPackage(const nsCString& aProvider,
                                     nsIRDFResource* aPackageResource, 
                                     nsIRDFResource* aProviderPackageResource, 
                                     nsIRDFResource* aSelectionArc, 
                                     PRBool aUseProfile, PRBool* aResult);

  NS_IMETHOD InstallProvider(const nsCString& aProviderType,
                             const nsCString& aBaseURL,
                             PRBool aUseProfile, PRBool aAllowScripts, PRBool aRemove);
  NS_IMETHOD UninstallProvider(const nsCString& aProviderType, const PRUnichar* aProviderName, PRBool aUseProfile);

  nsresult ProcessNewChromeBuffer(char *aBuffer, PRInt32 aLength);

  PRBool GetProviderCount(const nsCString& aProviderType, nsIRDFDataSource* aDataSource);

protected:
  PRBool mInstallInitialized;
  PRBool mProfileInitialized;
  
  PRBool mUseXBLForms;

  nsCString mProfileRoot;
  nsCString mInstallRoot;

  nsCOMPtr<nsIRDFCompositeDataSource> mChromeDataSource;
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
  nsCOMPtr<nsIRDFResource> mSkinVersion;
  nsCOMPtr<nsIRDFResource> mLocaleVersion;
  nsCOMPtr<nsIRDFResource> mPackageVersion;

  // Style Sheets
  nsCOMPtr<nsICSSStyleSheet> mScrollbarSheet;
  nsCOMPtr<nsICSSStyleSheet> mUserChromeSheet;
  nsCOMPtr<nsICSSStyleSheet> mUserContentSheet;
  nsCOMPtr<nsICSSStyleSheet> mFormSheet;
};

