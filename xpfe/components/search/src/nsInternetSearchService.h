/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 
#ifndef nsinternetsearchdatasource__h____
#define nsinternetsearchdatasource__h____

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsISearchService.h"
#include "nsIRDFDataSource.h"
#include "nsIStreamListener.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIRDFService.h"
#include "nsITimer.h"
#include "nsIFileSpec.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsIPref.h"


class InternetSearchDataSource : public nsIInternetSearchService,
                                 public nsIRDFDataSource,
                                 public nsIStreamListener,
                                 public nsIObserver,
                                 public nsSupportsWeakReference
{
private:
  static PRInt32          gRefCnt;
  static PRBool           mEngineListBuilt;

  // pseudo-constants
  static nsIRDFResource    *kNC_SearchResult;
  static nsIRDFResource    *kNC_SearchEngineRoot;
  static nsIRDFResource    *kNC_LastSearchRoot;
  static nsIRDFResource    *kNC_LastSearchMode;
  static nsIRDFResource    *kNC_SearchCategoryRoot;
  static nsIRDFResource    *kNC_SearchResultsSitesRoot;
  static nsIRDFResource    *kNC_FilterSearchURLsRoot;
  static nsIRDFResource    *kNC_FilterSearchSitesRoot;
  static nsIRDFResource    *kNC_SearchType;
  static nsIRDFResource    *kNC_Ref;
  static nsIRDFResource    *kNC_Child;
  static nsIRDFResource    *kNC_Title;
  static nsIRDFResource    *kNC_Data;
  static nsIRDFResource    *kNC_Name;
  static nsIRDFResource    *kNC_Description;
  static nsIRDFResource    *kNC_Version;
  static nsIRDFResource    *kNC_actionButton;
  static nsIRDFResource    *kNC_actionBar;
  static nsIRDFResource    *kNC_LastText;
  static nsIRDFResource    *kNC_URL;
  static nsIRDFResource    *kRDF_InstanceOf;
  static nsIRDFResource    *kRDF_type;
  static nsIRDFResource    *kNC_loading;
  static nsIRDFResource    *kNC_HTML;
  static nsIRDFResource    *kNC_Icon;
  static nsIRDFResource    *kNC_StatusIcon;
  static nsIRDFResource    *kNC_Banner;
  static nsIRDFResource    *kNC_Site;
  static nsIRDFResource    *kNC_Relevance;
  static nsIRDFResource    *kNC_Date;
  static nsIRDFResource    *kNC_RelevanceSort;
  static nsIRDFResource    *kNC_PageRank;
  static nsIRDFResource    *kNC_Engine;
  static nsIRDFResource    *kNC_Price;
  static nsIRDFResource    *kNC_PriceSort;
  static nsIRDFResource    *kNC_Availability;
  static nsIRDFResource    *kNC_BookmarkSeparator;
  static nsIRDFResource    *kNC_Update;
  static nsIRDFResource    *kNC_UpdateIcon;
  static nsIRDFResource    *kNC_UpdateCheckDays;
  static nsIRDFResource    *kWEB_LastPingDate;
  static nsIRDFResource    *kWEB_LastPingModDate;
  static nsIRDFResource    *kWEB_LastPingContentLen;

  static nsIRDFResource    *kNC_SearchCommand_AddToBookmarks;
  static nsIRDFResource    *kNC_SearchCommand_FilterResult;
  static nsIRDFResource    *kNC_SearchCommand_FilterSite;
  static nsIRDFResource    *kNC_SearchCommand_ClearFilters;

  static nsIRDFLiteral     *kTrueLiteral;

protected:
  static nsIRDFDataSource           *mInner;
  static nsCOMPtr<nsIRDFDataSource>  mLocalstore;
  static nsCOMPtr<nsISupportsArray>  mUpdateArray;
  nsCOMPtr<nsITimer>                 mTimer;
  static nsCOMPtr<nsILoadGroup>      mBackgroundLoadGroup;
  static nsCOMPtr<nsILoadGroup>      mLoadGroup;
  static nsCOMPtr<nsIRDFDataSource>  categoryDataSource;
  static nsCOMPtr<nsIPref>           prefs;
  PRBool                             busySchedule;
  nsCOMPtr<nsIRDFResource>           busyResource;
  nsString                           mQueryEncodingStr;


friend  int  PR_CALLBACK  searchModePrefCallback(const char *pref, void *aClosure);

  // helper methods
  nsresult  GetSearchEngineToPing(nsIRDFResource **theResource, nsCString &updateURL);
  PRBool    isEngineURI(nsIRDFResource* aResource);
  PRBool    isSearchURI(nsIRDFResource* aResource);
  PRBool    isSearchCategoryURI(nsIRDFResource* aResource);
  PRBool    isSearchCategoryEngineURI(nsIRDFResource* aResource);
  PRBool    isSearchCategoryEngineBasenameURI(nsIRDFNode *aResource);
  PRBool    isSearchCommand(nsIRDFResource* aResource);
  nsresult  resolveSearchCategoryEngineURI(nsIRDFResource *source, nsIRDFResource **trueEngine);
  nsresult  BeginSearchRequest(nsIRDFResource *source, PRBool doNetworkRequest);
  nsresult  FindData(nsIRDFResource *engine, nsIRDFLiteral **data);
  nsresult  updateDataHintsInGraph(nsIRDFResource *engine, const PRUnichar *data);
  nsresult  updateAtom(nsIRDFDataSource *db, nsIRDFResource *src, nsIRDFResource *prop, nsIRDFNode *newValue, PRBool *dirtyFlag);
  nsresult  validateEngine(nsIRDFResource *engine);
  nsresult  DoSearch(nsIRDFResource *source, nsIRDFResource *engine, const nsString &fullURL, const nsString &text);
  nsresult  MapEncoding(const nsString &numericEncoding, nsString &stringEncoding);
  nsresult  SaveEngineInfoIntoGraph(nsIFile *file, nsIFile *icon, const PRUnichar *hint, const PRUnichar *data, PRBool checkMacFileType);
  nsresult  GetSearchEngineList(nsIFile *spec, PRBool checkMacFileType);
  nsresult  GetCategoryList();
  nsresult  GetSearchFolder(nsIFile **spec);
  nsresult  ReadFileContents(const nsFileSpec &baseFilename, nsString & sourceContents);
  nsresult  GetData(const PRUnichar *data, const char *sectionToFind, PRUint32 sectionNum, const char *attribToFind, nsString &value);
  nsresult  GetNumInterpretSections(const PRUnichar *data, PRUint32 &numInterpretSections);
  nsresult  GetInputs(const PRUnichar *data, nsString &userVar, const nsString &text, nsString &input);
  nsresult  GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
  nsresult  validateEngineNow(nsIRDFResource *engine);
  nsresult  webSearchFinalize(nsIChannel *channel, nsIInternetSearchContext *context);
  nsresult  ParseHTML(nsIURI *aURL, nsIRDFResource *mParent, nsIRDFResource *engine, const PRUnichar *htmlPage);
  nsresult  SetHint(nsIRDFResource *mParent, nsIRDFResource *hintRes);
  nsresult  ConvertEntities(nsString &str, PRBool removeHTMLFlag = PR_TRUE, PRBool removeCRLFsFlag = PR_TRUE, PRBool trimWhiteSpaceFlag = PR_TRUE);
  nsresult  saveContents(nsIChannel* channel, nsIInternetSearchContext *context, PRUint32 contextType);
  char *    getSearchURI(nsIRDFResource *src);
  nsresult  addToBookmarks(nsIRDFResource *src);
  nsresult  filterResult(nsIRDFResource *src);
  nsresult  filterSite(nsIRDFResource *src);
  nsresult  clearFilters(void);
  PRBool    isSearchResultFiltered(const nsString &href);

static  void    FireTimer(nsITimer* aTimer, void* aClosure);


public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERNETSEARCHSERVICE
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIRDFDATASOURCE
  NS_DECL_NSIOBSERVER

  InternetSearchDataSource(void);
  virtual    ~InternetSearchDataSource(void);
  NS_METHOD  Init();
  NS_METHOD  DeferredInit();
};

#endif // nsinternetsearchdatasource__h____
