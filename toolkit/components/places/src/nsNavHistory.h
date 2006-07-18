/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
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


#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageValueArray.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsINavHistory.h"
#include "nsIAutoCompleteSearch.h"
#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteResultTypes.h"
#include "nsIAutoCompleteSimpleResult.h"
#include "nsIBrowserHistory.h"
#include "nsICollation.h"
#include "nsIGlobalHistory.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsIStringBundle.h"
#include "nsITimer.h"
#include "nsITreeSelection.h"
#include "nsITreeView.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsWeakReference.h"

// Number of prefixes used in the autocomplete sort comparison function
#define AUTOCOMPLETE_PREFIX_LIST_COUNT 6
// Size of visit count boost to give to urls which are sites or paths
#define AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST 5

class mozIAnnotationService;


// nsNavHistoryQuery
//
//    This class encapsulates the parameters for basic history queries for
//    building UI, trees, lists, etc.

class nsNavHistoryQuery : public nsINavHistoryQuery
{
public:
  nsNavHistoryQuery();

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERY

private:
  ~nsNavHistoryQuery() {}

protected:

  PRTime mBeginTime;
  PRTime mEndTime;
  nsString mSearchTerms;
  PRBool mOnlyBookmarked;
  PRBool mDomainIsHost;
  nsString mDomain;
  PRInt32 mGroupingMode;
  PRInt32 mSortingMode;
  PRBool mAsVisits;
};


// nsNavHistoryResultNode

class nsNavHistoryResultNode : public nsINavHistoryResultNode
{
public:
  nsNavHistoryResultNode();

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYRESULTNODE

private:
  ~nsNavHistoryResultNode() {}

protected:
  // parent of this element, NULL if no parent. Filled in by FillAllElements
  // in the result set.
  nsNavHistoryResultNode* mParent;

  // these are all the basic row info, filled in by nsNavHistory::RowToResult
  PRInt64 mID; // keep this for comparing dups, not exposed in interface
               // might be 0, indicating some kind of parent node
  PRInt32 mType;
  nsCString mUrl;
  nsString mTitle;
  PRInt32 mAccessCount;
  PRTime mTime;
  nsString mHost;

  // Filled in by the result type generator in nsNavHistory
  nsCOMArray<nsNavHistoryResultNode> mChildren;

  // filled in by FillAllElements in the result set.
  PRInt32 mIndentLevel;

  // Index of this element into the flat mAllElements array in the result set.
  // Filled in by FillAllElements. DANGER, this does not necessarily mean that
  // mAllElements[mFlatIndex] = this, because we could be collapsed.
  // mAllElements[mFlatIndex] could be a duplicate of us.
  PRInt32 mFlatIndex;

  // index of this element into the mVisibleElements array in the result set
  PRInt32 mVisibleIndex;

  // when there are children, this stores the open state in the tree
  // this is set to the default in the constructor
  PRBool mExpanded;

  friend class nsNavHistory;
  friend class nsNavHistoryResult;
};

class nsIDateTimeFormat;
class nsNavHistory;

// nsNavHistoryResult
//
//    nsNavHistory creates this object and fills in mTopLevel (by getting
//    it through GetTopLevel(). Then FilledAllResults() is called to finish
//    object initialization.

class nsNavHistoryResult : public nsINavHistoryResult,
                           public nsITreeView
{
public:
  nsNavHistoryResult(nsNavHistory* aHistoryService,
                     nsIStringBundle* aHistoryBundle);

  // Two-stage init, MUST BE CALLED BEFORE ANYTHING ELSE
  nsresult Init();

  nsCOMArray<nsNavHistoryResultNode>* GetTopLevel() { return &mTopLevelElements; }
  void ApplyTreeState(
      const nsDataHashtable<nsStringHashKey, int>& aExpanded);
  void FilledAllResults();

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYRESULT
  NS_DECL_NSITREEVIEW

private:
  ~nsNavHistoryResult();

protected:
  nsCOMPtr<nsIStringBundle> mBundle;
  nsCOMPtr<nsITreeBoxObject> mTree; // may be null if no tree has bound itself
  nsCOMPtr<nsITreeSelection> mSelection; // may be null

  // This is a COM ptr that MUST BE AddRef'ed and Release'd MANUALLY.
  // nsNavHistory has nsISupports as an ambiguous base class, so nsCOMPtr
  // won't work.
  nsRefPtr<nsNavHistory> mHistoryService;

  PRBool mCollapseDuplicates;

  // for locale-specific date formatting and string sorting
  nsCOMPtr<nsILocale> mLocale;
  nsCOMPtr<nsICollation> mCollation;
  nsCOMPtr<nsIDateTimeFormat> mDateFormatter;
  PRBool mTimesIncludeDates;

  // these are the roots of the hierarchy. This array is filled in externally
  // (use GetTopLevel). This contains the owning references. Everything else
  // uses void arrays to avoid AddRef overhead and to get better functionality.
  nsCOMArray<nsNavHistoryResultNode> mTopLevelElements;

  // this is the flattened version of the hierarchy containing everything
  nsVoidArray mAllElements;
  nsNavHistoryResultNode* AllElementAt(int index)
  {
    return (nsNavHistoryResultNode*)mAllElements[index];
  }

  nsVoidArray mVisibleElements;
  nsNavHistoryResultNode* VisibleElementAt(int index)
  {
    return (nsNavHistoryResultNode*)mVisibleElements[index];
  }

  // keep track of sorting state
  PRUint32 mCurrentSort;

  void FillTreeStats(nsNavHistoryResultNode* aResult, int aLevel);
  void InitializeVisibleList();
  void RebuildList();
  void RebuildAllListRecurse(const nsCOMArray<nsNavHistoryResultNode>& aSource);
  void BuildVisibleSection(const nsCOMArray<nsNavHistoryResultNode>& aSources,
                           nsVoidArray* aVisible);
  void InsertVisibleSection(const nsVoidArray& aAddition, int aInsertHere);
  int DeleteVisibleChildrenOf(int aIndex);

  void RecursiveSortArray(nsCOMArray<nsNavHistoryResultNode>& aSources,
                          PRUint32 aSortingMode);
  void SetTreeSortingIndicator();

  //void RecursiveCopyTreeStateFrom(nsCOMArray<nsNavHistoryResultNode>& aSource,
  //                                nsCOMArray<nsNavHistoryResultNode>& aDest);
  void RecursiveApplyTreeState(
      nsCOMArray<nsNavHistoryResultNode>& aList,
      const nsDataHashtable<nsStringHashKey, int>& aExpanded);
  void RecursiveExpandCollapse(nsCOMArray<nsNavHistoryResultNode>& aList,
                               PRBool aExpand);

  enum ColumnType { Column_Unknown = -1, Column_Title, Column_URL, Column_Date, Column_VisitCount };
  ColumnType GetColumnType(nsITreeColumn* col);
  ColumnType SortTypeToColumnType(PRUint32 aSortType,
                                  PRBool* aDescending = nsnull);

  PR_STATIC_CALLBACK(int) SortComparison_TitleLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_TitleGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_DateLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_DateGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_URLLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_URLGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_VisitCountLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_VisitCountGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
};


class AutoCompleteIntermediateResultSet;

// nsNavHistory

class nsNavHistory : nsSupportsWeakReference,
                     public nsINavHistory,
                     public nsIObserver,
                     public nsIBrowserHistory,
                     public nsIAutoCompleteSearch
{
  friend class AutoCompleteIntermediateResultSet;
public:
  nsNavHistory();

  NS_DECL_ISUPPORTS

  NS_DECL_NSINAVHISTORY
  NS_DECL_NSIGLOBALHISTORY2
  NS_DECL_NSIBROWSERHISTORY
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIAUTOCOMPLETESEARCH

  nsresult Init();

  /**
   * Used by other components in the places directory such as the annotation
   * service to get a reference to this history object. Returns a pointer to
   * the service if it exists. Otherwise creates one. Returns NULL on error.
   */
  static nsNavHistory* GetHistoryService()
  {
    if (gHistoryService)
      return gHistoryService;

    // don't want the return value, since that's the interface. We want the
    // pointer to the implementation.
    do_GetService("@mozilla.org/browser/nav-history;1");

    // our constructor should have set the static variable. If it didn't,
    // something is wrong.
    NS_ASSERTION(gHistoryService, "History service creation failed");
    return gHistoryService;
  }

  nsresult GetUrlIdFor(nsIURI* aURI, PRInt64* aEntryID,
                       PRBool aAutoCreate);

  /**
   * Returns a pointer to the storage connection used by history. This connection
   * object is also used by the annotation service and bookmarks, so that
   * things can be grouped into transactions across these components.
   *
   * This connection can only be used in the thread that created it the
   * history service!
   */
  mozIStorageConnection* GetStorageConnection()
  {
    return mDBConn;
  }

  // remember tree state

  void SaveExpandItem(const nsAString& aTitle);
  void SaveCollapseItem(const nsAString& aTitle);

private:
  ~nsNavHistory();

  // used by GetHistoryService
  static nsNavHistory* gHistoryService;

protected:

  //
  // Constants
  //
  nsCOMPtr<nsIPrefService> gPrefService;
  nsCOMPtr<nsIPrefBranch> gPrefBranch;
  nsCOMPtr<nsIObserverService> gObserverService;
  nsDataHashtable<nsStringHashKey, int> gExpandedItems;

  //
  // Database stuff
  //
  nsCOMPtr<mozIStorageService> mDBService;
  nsCOMPtr<mozIStorageConnection> mDBConn;

  nsCOMPtr<mozIStorageStatement> mDBGetVisitPageInfo; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBGetURLPageInfo;   // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBFullAutoComplete; // kAutoCompleteIndex_* results, 1 arg (max # results)
  static const int kGetInfoIndex_PageID;
  static const int kGetInfoIndex_URL;
  static const int kGetInfoIndex_Title;
  static const int kGetInfoIndex_VisitCount;
  static const int kGetInfoIndex_VisitDate;
  static const int kGetInfoIndex_RevHost;
  static const int kAutoCompleteIndex_URL;
  static const int kAutoCompleteIndex_Title;
  static const int kAutoCompleteIndex_VisitCount;
  static const int kAutoCompleteIndex_Typed;

  nsresult InitDB();

  // this is the cache DB in memory used for storing visited URLs
  nsCOMPtr<mozIStorageConnection> mMemDBConn;
  nsCOMPtr<mozIStorageStatement> mMemDBAddPage;
  nsCOMPtr<mozIStorageStatement> mMemDBGetPage;

  nsresult InitMemDB();

  nsresult InternalAdd(nsIURI* aURI, PRUint32 aSessionID,
                       PRUint32 aTransitionType, const PRUnichar* aTitle,
                       PRTime aVisitDate, PRBool aRedirect,
                       PRBool aToplevel, PRInt64* aPageID);
  nsresult InternalAddNewPage(nsIURI* aURI, const PRUnichar* aTitle,
                              PRBool aHidden, PRBool aTyped,
                              int aVisitCount, PRInt64* aPageID);
  nsresult AddVisit(PRInt64 aFromStep, PRInt64 aPageID, PRTime aTime,
                    PRInt32 aTransitionType, PRInt64 aSessionID);
  PRBool IsURIStringVisited(const nsACString& url);
  nsresult VacuumDB();
  nsresult LoadPrefs();

  // Current time optimization
  PRTime mLastNow;
  PRBool mNowValid;
  nsCOMPtr<nsITimer> mExpireNowTimer;
  PRTime GetNow();
  static void expireNowTimerCallback(nsITimer* aTimer, void* aClosure);

  nsresult QueryToSelectClause(nsINavHistoryQuery* aQuery,
                               int aStartParameter,
                               nsCString* aClause,
                               int* aParamCount);
  nsresult BindQueryClauseParameters(mozIStorageStatement* statement,
                                     int aStartParameter,
                                     nsINavHistoryQuery* aQuery,
                                     int* aParamCount);

  nsresult ResultsAsList(mozIStorageStatement* statement, PRBool aAsVisits,
                         nsCOMArray<nsNavHistoryResultNode>* aResults);

  nsresult RowToResult(mozIStorageValueArray* aRow, PRBool aAsVisits,
                       nsNavHistoryResultNode** aResult);
  void TitleForDomain(const nsString& domain, nsAString& aTitle);

  nsresult RecursiveGroup(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                          const PRInt32* aGroupingMode, PRUint32 aGroupCount,
                          nsCOMArray<nsNavHistoryResultNode>* aDest);
  nsresult GroupByDay(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                      nsCOMArray<nsNavHistoryResultNode>* aDest);
  nsresult GroupByHost(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                       nsCOMArray<nsNavHistoryResultNode>* aDest);
  nsresult GroupByDomain(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                         nsCOMArray<nsNavHistoryResultNode>* aDest);

  nsresult FilterResultSet(const nsCOMArray<nsNavHistoryResultNode>& aSet,
                           nsCOMArray<nsNavHistoryResultNode>* aFiltered,
                           const nsString& aSearch);

  // observers
  nsCOMArray<nsINavHistoryObserver> mObservers;
  PRInt32 mBatchesInProgress;

  // string bundles
  nsCOMPtr<nsIStringBundle> mBundle;

  // annotation service : MAY BE NULL!
  //nsCOMPtr<mozIAnnotationService> mAnnotationService;

  //
  // AutoComplete stuff
  //
  nsStringArray mIgnoreSchemes;
  nsStringArray mIgnoreHostnames;

  PRInt32 mAutoCompleteMaxCount;
  PRInt32 mExpireDays;
  PRBool mAutoCompleteOnlyTyped;

  // Used to describe what prefixes shouldn't be cut from
  // history urls when doing an autocomplete url comparison.
  struct AutoCompleteExclude {
    // these are indices into mIgnoreSchemes and mIgnoreHostnames, or -1
    PRInt32 schemePrefix;
    PRInt32 hostnamePrefix;

    // this is the offset of the character immediately after the prefix
    PRInt32 postPrefixOffset;
  };

  nsresult AutoCompleteTypedSearch(nsIAutoCompleteSimpleResult* result);
  nsresult AutoCompleteFullHistorySearch(const nsAString& aSearchString,
                                         nsIAutoCompleteSimpleResult* result);
  nsresult AutoCompleteRefineHistorySearch(const nsAString& aSearchString,
                                   nsIAutoCompleteResult* aPreviousResult,
                                   nsIAutoCompleteSimpleResult* aNewResult);
  void AutoCompleteCutPrefix(nsString* aURL,
                             const AutoCompleteExclude* aExclude);
  void AutoCompleteGetExcludeInfo(const nsAString& aURL,
                                  AutoCompleteExclude* aExclude);
  PRBool AutoCompleteCompare(const nsAString& aHistoryURL,
                             const nsAString& aUserURL,
                             const AutoCompleteExclude& aExclude);
  PR_STATIC_CALLBACK(int) AutoCompleteSortComparison(
                             const void* match1Void,
                             const void* match2Void,
                             void *navHistoryVoid);

  nsresult ImportFromMork();
};

/**
 * Shared between the places components, this function binds the given URI as
 * UTF8 to the given parameter for the statement.
 */
nsresult BindStatementURI(mozIStorageStatement* statement, int index,
                          nsIURI* aURI);
