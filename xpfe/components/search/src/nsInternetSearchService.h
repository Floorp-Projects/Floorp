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
 
#ifndef nsinternetsearchdatasource__h____
#define nsinternetsearchdatasource__h____

#include "nsCOMPtr.h"
#include "nsStringAPI.h"
#include "nsISearchService.h"
#include "nsIRDFDataSource.h"
#include "nsIStreamListener.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIRDFService.h"
#include "nsIRDFContainerUtils.h"
#include "nsITimer.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsIPref.h"
#include "nsCycleCollectionParticipant.h"

class InternetSearchDataSource : public nsIInternetSearchService,
                                 public nsIRDFDataSource,
                                 public nsIStreamListener,
                                 public nsIObserver,
                                 public nsSupportsWeakReference
{
private:
  PRInt32 mBrowserSearchMode;
  PRBool  mEngineListBuilt;

#ifdef MOZ_PHOENIX
  PRBool mReorderedEngineList;
#endif

  // pseudo-constants
  nsCOMPtr<nsIRDFResource> mNC_SearchResult;
  nsCOMPtr<nsIRDFResource> mNC_SearchEngineRoot;
  nsCOMPtr<nsIRDFResource> mNC_LastSearchRoot;
  nsCOMPtr<nsIRDFResource> mNC_LastSearchMode;
  nsCOMPtr<nsIRDFResource> mNC_SearchCategoryRoot;
  nsCOMPtr<nsIRDFResource> mNC_SearchResultsSitesRoot;
  nsCOMPtr<nsIRDFResource> mNC_FilterSearchURLsRoot;
  nsCOMPtr<nsIRDFResource> mNC_FilterSearchSitesRoot;
  nsCOMPtr<nsIRDFResource> mNC_SearchType;
  nsCOMPtr<nsIRDFResource> mNC_Ref;
  nsCOMPtr<nsIRDFResource> mNC_Child;
  nsCOMPtr<nsIRDFResource> mNC_Title;
  nsCOMPtr<nsIRDFResource> mNC_Data;
  nsCOMPtr<nsIRDFResource> mNC_Name;
  nsCOMPtr<nsIRDFResource> mNC_Description;
  nsCOMPtr<nsIRDFResource> mNC_Version;
  nsCOMPtr<nsIRDFResource> mNC_actionButton;
  nsCOMPtr<nsIRDFResource> mNC_actionBar;
  nsCOMPtr<nsIRDFResource> mNC_searchForm;
  nsCOMPtr<nsIRDFResource> mNC_LastText;
  nsCOMPtr<nsIRDFResource> mNC_URL;
  nsCOMPtr<nsIRDFResource> mRDF_InstanceOf;
  nsCOMPtr<nsIRDFResource> mRDF_type;
  nsCOMPtr<nsIRDFResource> mNC_loading;
  nsCOMPtr<nsIRDFResource> mNC_HTML;
  nsCOMPtr<nsIRDFResource> mNC_Icon;
  nsCOMPtr<nsIRDFResource> mNC_StatusIcon;
  nsCOMPtr<nsIRDFResource> mNC_Banner;
  nsCOMPtr<nsIRDFResource> mNC_Site;
  nsCOMPtr<nsIRDFResource> mNC_Relevance;
  nsCOMPtr<nsIRDFResource> mNC_Date;
  nsCOMPtr<nsIRDFResource> mNC_RelevanceSort;
  nsCOMPtr<nsIRDFResource> mNC_PageRank;
  nsCOMPtr<nsIRDFResource> mNC_Engine;
  nsCOMPtr<nsIRDFResource> mNC_Price;
  nsCOMPtr<nsIRDFResource> mNC_PriceSort;
  nsCOMPtr<nsIRDFResource> mNC_Availability;
  nsCOMPtr<nsIRDFResource> mNC_BookmarkSeparator;
  nsCOMPtr<nsIRDFResource> mNC_Update;
  nsCOMPtr<nsIRDFResource> mNC_UpdateIcon;
  nsCOMPtr<nsIRDFResource> mNC_UpdateCheckDays;
  nsCOMPtr<nsIRDFResource> mWEB_LastPingDate;
  nsCOMPtr<nsIRDFResource> mWEB_LastPingModDate;
  nsCOMPtr<nsIRDFResource> mWEB_LastPingContentLen;

  nsCOMPtr<nsIRDFResource> mNC_SearchCommand_AddToBookmarks;
  nsCOMPtr<nsIRDFResource> mNC_SearchCommand_AddQueryToBookmarks;
  nsCOMPtr<nsIRDFResource> mNC_SearchCommand_FilterResult;
  nsCOMPtr<nsIRDFResource> mNC_SearchCommand_FilterSite;
  nsCOMPtr<nsIRDFResource> mNC_SearchCommand_ClearFilters;

  nsCOMPtr<nsIRDFLiteral>  mTrueLiteral;

protected:
  nsCOMPtr<nsIRDFService>     mRDFService;
  nsCOMPtr<nsIRDFContainerUtils> mRDFC;
  nsCOMPtr<nsIRDFDataSource>  mInner;
  nsCOMPtr<nsIRDFDataSource>  mLocalstore;
  nsCOMPtr<nsISupportsArray>  mUpdateArray;
  nsCOMPtr<nsITimer>          mTimer;
  nsCOMPtr<nsILoadGroup>      mBackgroundLoadGroup;
  nsCOMPtr<nsILoadGroup>      mLoadGroup;
  nsCOMPtr<nsIRDFDataSource>  categoryDataSource;
  PRBool                      busySchedule;
  nsCOMPtr<nsIRDFResource>    busyResource;
  nsString                    mQueryEncodingStr;

  friend  int  PR_CALLBACK  searchModePrefCallback(const char *pref, void *aClosure);

  // helper methods
  nsresult  GetSearchEngineToPing(nsIRDFResource **theResource, nsCString &updateURL);
  PRBool    isEngineURI(nsIRDFResource* aResource);
  PRBool    isSearchURI(nsIRDFResource* aResource);
  PRBool    isSearchCategoryURI(nsIRDFResource* aResource);
  PRBool    isSearchCategoryEngineURI(nsIRDFResource* aResource);
  PRBool    isSearchCommand(nsIRDFResource* aResource);
  nsresult  resolveSearchCategoryEngineURI(nsIRDFResource *source, nsIRDFResource **trueEngine);
  nsresult  BeginSearchRequest(nsIRDFResource *source, PRBool doNetworkRequest);
  nsresult  FindData(nsIRDFResource *engine, nsIRDFLiteral **data);
  nsresult  EngineFileFromResource(nsIRDFResource *aEngineResource,
                                   nsILocalFile **aResult);
  nsresult  updateDataHintsInGraph(nsIRDFResource *engine, const PRUnichar *data);
  nsresult  updateAtom(nsIRDFDataSource *db, nsIRDFResource *src, nsIRDFResource *prop, nsIRDFNode *newValue, PRBool *dirtyFlag);
  nsresult  validateEngine(nsIRDFResource *engine);
  nsresult  DoSearch(nsIRDFResource *source, nsIRDFResource *engine, const nsString &fullURL, const nsString &text);
  nsresult  MapEncoding(const nsString &numericEncoding, 
                        nsString &stringEncoding);
  const char * const MapScriptCodeToCharsetName(PRUint32 aScriptCode);
  nsresult  SaveEngineInfoIntoGraph(nsIFile *file, nsIFile *icon, const PRUnichar *hint, const PRUnichar *data, PRBool isSystemSearchFile);
  nsresult  GetSearchEngineList(nsIFile *spec, PRBool isSystemSearchFile);
  nsresult  GetCategoryList();
  nsresult  ReadFileContents(nsILocalFile *baseFilename, nsString & sourceContents);
  nsresult  DecodeData(const char *aCharset, const PRUnichar *aInString, PRUnichar **aOutString);
  nsresult  GetData(const PRUnichar *data, const char *sectionToFind, PRUint32 sectionNum, const char *attribToFind, nsString &value);
  nsresult  GetNumInterpretSections(const PRUnichar *data, PRUint32 &numInterpretSections);
  nsresult  GetInputs(const PRUnichar *data, nsString &engineName, nsString &userVar, const nsString &text, nsString &input, PRInt16 direction, PRUint16 pageNumber, PRUint16 *whichButtons);
  PRInt32   computeIndex(nsAutoString &factor, PRUint16 page, PRInt16 direction);
  nsresult  GetURL(nsIRDFResource *source, nsIRDFLiteral** aResult);
  nsresult  validateEngineNow(nsIRDFResource *engine);
  nsresult  webSearchFinalize(nsIChannel *channel, nsIInternetSearchContext *context);
  nsresult  ParseHTML(nsIURI *aURL, nsIRDFResource *mParent, nsIRDFResource *engine, const PRUnichar *htmlPage, PRInt32 htmlPageSize);
  nsresult  SetHint(nsIRDFResource *mParent, nsIRDFResource *hintRes);
  nsresult  ConvertEntities(nsString &str, PRBool removeHTMLFlag = PR_TRUE, PRBool removeCRLFsFlag = PR_TRUE, PRBool trimWhiteSpaceFlag = PR_TRUE);
  nsresult  saveContents(nsIChannel* channel, nsIInternetSearchContext *context, PRUint32 contextType);
  PRBool    getSearchURI(nsIRDFResource *src, nsAString &_retval);  // returns true on success
  nsresult  addToBookmarks(nsIRDFResource *src);
  nsresult  addQueryToBookmarks(nsIRDFResource *src);
  nsresult  filterResult(nsIRDFResource *src);
  nsresult  filterSite(nsIRDFResource *src);
  nsresult  clearFilters(void);
  PRBool    isSearchResultFiltered(const nsString &href);
#ifdef MOZ_PHOENIX
  void      ReorderEngineList();
#endif

static  void    FireTimer(nsITimer* aTimer, void* aClosure);

  // Just like AddSearchEngine, but with an extra parameter to include the
  // nsIRDFResource of an existing engines.  If this is a new search engine
  // aOldEngineResource should be null.
  nsresult AddSearchEngineInternal(const char *engineURL,
                                   const char *iconURL,
                                   const PRUnichar *suggestedTitle,
                                   const PRUnichar *suggestedCategory,
                                   nsIRDFResource *aOldEngineResource);

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(InternetSearchDataSource,
                                           nsIInternetSearchService)
  NS_DECL_NSIINTERNETSEARCHSERVICE
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIRDFDATASOURCE
  NS_DECL_NSIOBSERVER

  InternetSearchDataSource(void);
  virtual    ~InternetSearchDataSource(void);
  NS_METHOD  Init();
  void DeferredInit();
};

#endif // nsinternetsearchdatasource__h____
