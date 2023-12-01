/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNavHistory_h_
#define nsNavHistory_h_

#include "nsINavHistoryService.h"

#include "nsIStringBundle.h"
#include "nsITimer.h"
#include "nsMaybeWeakPtr.h"
#include "nsCategoryCache.h"
#include "nsNetCID.h"
#include "nsToolkitCompsCID.h"
#include "nsURIHashKey.h"
#include "nsTHashtable.h"

#include "nsNavHistoryResult.h"
#include "nsNavHistoryQuery.h"
#include "Database.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/intl/Collator.h"
#include "mozilla/UniquePtr.h"
#include "mozIStorageVacuumParticipant.h"

#define QUERYUPDATE_TIME 0
#define QUERYUPDATE_SIMPLE 1
#define QUERYUPDATE_COMPLEX 2
#define QUERYUPDATE_COMPLEX_WITH_BOOKMARKS 3
#define QUERYUPDATE_HOST 4
#define QUERYUPDATE_MOBILEPREF 5
#define QUERYUPDATE_NONE 6

// Clamp title and URL to generously large, but not too large, length.
// See bug 319004 for details.
#define URI_LENGTH_MAX 65536
#define TITLE_LENGTH_MAX 4096

// Microsecond timeout for "recent" events such as typed and bookmark following.
// If you typed it more than this time ago, it's not recent.
#define RECENT_EVENT_THRESHOLD PRTime((int64_t)15 * 60 * PR_USEC_PER_SEC)

// The preference we watch to know when the mobile bookmarks folder is filled by
// sync.
#define MOBILE_BOOKMARKS_PREF "browser.bookmarks.showMobileBookmarks"

// The guid of the mobile bookmarks virtual query.
#define MOBILE_BOOKMARKS_VIRTUAL_GUID "mobile_____v"

#define ROOT_GUID "root________"
#define MENU_ROOT_GUID "menu________"
#define TOOLBAR_ROOT_GUID "toolbar_____"
#define UNFILED_ROOT_GUID "unfiled_____"
#define TAGS_ROOT_GUID "tags________"
#define MOBILE_ROOT_GUID "mobile______"

#define SQL_QUOTE(text) "'" text "'"

class mozIStorageValueArray;
class nsIAutoCompleteController;
class nsIEffectiveTLDService;
class nsIIDNService;
class nsNavHistory;
class PlacesSQLQueryBuilder;

// nsNavHistory

class nsNavHistory final : public nsSupportsWeakReference,
                           public nsINavHistoryService,
                           public nsIObserver,
                           public mozIStorageVacuumParticipant {
  friend class PlacesSQLQueryBuilder;

 public:
  nsNavHistory();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINAVHISTORYSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_MOZISTORAGEVACUUMPARTICIPANT

  /**
   * Obtains the nsNavHistory object.
   */
  static already_AddRefed<nsNavHistory> GetSingleton();

  /**
   * Initializes the nsNavHistory object.  This should only be called once.
   */
  nsresult Init();

  /**
   * Used by other components in the places directory such as the annotation
   * service to get a reference to this history object. Returns a pointer to
   * the service if it exists. Otherwise creates one. Returns nullptr on error.
   */
  static nsNavHistory* GetHistoryService() {
    if (!gHistoryService) {
      nsCOMPtr<nsINavHistoryService> serv =
          do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);
      NS_ENSURE_TRUE(serv, nullptr);
      NS_ASSERTION(gHistoryService, "Should have static instance pointer now");
    }
    return gHistoryService;
  }

  /**
   * Used by other components in the places directory to get a reference to a
   * const version of this history object.
   *
   * @return A pointer to a const version of the service if it exists,
   *         nullptr otherwise.
   */
  static const nsNavHistory* GetConstHistoryService() {
    const nsNavHistory* const history = gHistoryService;
    return history;
  }

  /**
   * Fetches the database id and the GUID associated to the given URI.
   *
   * @param aURI
   *        The page to look for.
   * @param _pageId
   *        Will be set to the database id associated with the page.
   *        If the page doesn't exist, this will be zero.
   * @param _GUID
   *        Will be set to the unique id associated with the page.
   *        If the page doesn't exist, this will be empty.
   * @note This DOES NOT check for bad URLs other than that they're nonempty.
   */
  nsresult GetIdForPage(nsIURI* aURI, int64_t* _pageId, nsCString& _GUID);

  /**
   * Fetches the database id and the GUID associated to the given URI, creating
   * a new database entry if one doesn't exist yet.
   *
   * @param aURI
   *        The page to look for or create.
   * @param _pageId
   *        Will be set to the database id associated with the page.
   * @param _GUID
   *        Will be set to the unique id associated with the page.
   * @note This DOES NOT check for bad URLs other than that they're nonempty.
   * @note This DOES NOT update frecency of the page.
   */
  nsresult GetOrCreateIdForPage(nsIURI* aURI, int64_t* _pageId,
                                nsCString& _GUID);

  /**
   * These functions return non-owning references to the locale-specific
   * objects for places components.
   */
  nsIStringBundle* GetBundle();
  const mozilla::intl::Collator* GetCollator();
  void GetStringFromName(const char* aName, nsACString& aResult);
  void GetAgeInDaysString(int32_t aInt, const char* aName, nsACString& aResult);
  static void GetMonthName(const PRExplodedTime& aTime, nsACString& aResult);
  static void GetMonthYear(const PRExplodedTime& aTime, nsACString& aResult);

  // Returns whether history is enabled or not.
  bool IsHistoryDisabled() { return !mHistoryEnabled; }

  // Returns whether or not diacritics must match in history text searches.
  bool MatchDiacritics() const { return mMatchDiacritics; }

  // Constants for the columns returned by the above statement.
  static const int32_t kGetInfoIndex_PageID;
  static const int32_t kGetInfoIndex_URL;
  static const int32_t kGetInfoIndex_Title;
  static const int32_t kGetInfoIndex_RevHost;
  static const int32_t kGetInfoIndex_VisitCount;
  static const int32_t kGetInfoIndex_VisitDate;
  static const int32_t kGetInfoIndex_FaviconURL;
  static const int32_t kGetInfoIndex_ItemId;
  static const int32_t kGetInfoIndex_ItemDateAdded;
  static const int32_t kGetInfoIndex_ItemLastModified;
  static const int32_t kGetInfoIndex_ItemParentId;
  static const int32_t kGetInfoIndex_ItemTags;
  static const int32_t kGetInfoIndex_Frecency;
  static const int32_t kGetInfoIndex_Hidden;
  static const int32_t kGetInfoIndex_Guid;
  static const int32_t kGetInfoIndex_VisitId;
  static const int32_t kGetInfoIndex_FromVisitId;
  static const int32_t kGetInfoIndex_VisitType;
  static const int32_t kGetTargetFolder_Guid;
  static const int32_t kGetTargetFolder_ItemId;
  static const int32_t kGetTargetFolder_Title;

  int64_t GetTagsFolder();

  // this actually executes a query and gives you results, it is used by
  // nsNavHistoryQueryResultNode
  nsresult GetQueryResults(nsNavHistoryQueryResultNode* aResultNode,
                           const RefPtr<nsNavHistoryQuery>& aQuery,
                           const RefPtr<nsNavHistoryQueryOptions>& aOptions,
                           nsCOMArray<nsNavHistoryResultNode>* aResults);

  // Take a row of kGetInfoIndex_* columns and construct a ResultNode.
  // The row must contain the full set of columns.
  nsresult RowToResult(mozIStorageValueArray* aRow,
                       nsNavHistoryQueryOptions* aOptions,
                       nsNavHistoryResultNode** aResult);

  nsresult QueryUriToResult(const nsACString& aQueryURI, int64_t aItemId,
                            const nsACString& aBookmarkGuid,
                            const nsACString& aTitle,
                            int64_t aTargetFolderItemId,
                            const nsACString& aTargetFolderGuid,
                            const nsACString& aTargetFolderTitle,
                            uint32_t aAccessCount, PRTime aTime,
                            nsNavHistoryResultNode** aNode);

  /**
   * Returns current number of days stored in history.
   */
  int32_t GetDaysOfHistory();

  void DomainNameFromURI(nsIURI* aURI, nsACString& aDomainName);
  static PRTime NormalizeTime(uint32_t aRelative, PRTime aOffset);

  typedef nsTHashMap<nsCStringHashKey, nsCString> StringHash;

  enum RecentEventFlags {
    RECENT_TYPED = 1 << 0,      // User typed in URL recently
    RECENT_ACTIVATED = 1 << 1,  // User tapped URL link recently
    RECENT_BOOKMARKED = 1 << 2  // User bookmarked URL recently
  };

  /**
   * Returns any recent activity done with a URL.
   * @return Any recent events associated with this URI.  Each bit is set
   *         according to RecentEventFlags enum values.
   */
  uint32_t GetRecentFlags(nsIURI* aURI);

  /**
   * Whether there are visits.
   * Note: This may cause synchronous I/O.
   */
  bool hasHistoryEntries();

  /**
   * Returns whether the specified url has a embed visit.
   *
   * @param aURI
   *        URI of the page.
   * @return whether the page has a embed visit.
   */
  bool hasEmbedVisit(nsIURI* aURI);

  int32_t GetFrecencyAgedWeight(int32_t aAgeInDays) const {
    if (aAgeInDays <= mFirstBucketCutoffInDays) {
      return mFirstBucketWeight;
    }
    if (aAgeInDays <= mSecondBucketCutoffInDays) {
      return mSecondBucketWeight;
    }
    if (aAgeInDays <= mThirdBucketCutoffInDays) {
      return mThirdBucketWeight;
    }
    if (aAgeInDays <= mFourthBucketCutoffInDays) {
      return mFourthBucketWeight;
    }
    return mDefaultWeight;
  }

  int32_t GetFrecencyTransitionBonus(int32_t aTransitionType, bool aVisited,
                                     bool aRedirect = false) const {
    if (aRedirect) {
      return mRedirectSourceVisitBonus;
    }

    switch (aTransitionType) {
      case nsINavHistoryService::TRANSITION_EMBED:
        return mEmbedVisitBonus;
      case nsINavHistoryService::TRANSITION_FRAMED_LINK:
        return mFramedLinkVisitBonus;
      case nsINavHistoryService::TRANSITION_LINK:
        return mLinkVisitBonus;
      case nsINavHistoryService::TRANSITION_TYPED:
        return aVisited ? mTypedVisitBonus : mUnvisitedTypedBonus;
      case nsINavHistoryService::TRANSITION_BOOKMARK:
        return aVisited ? mBookmarkVisitBonus : mUnvisitedBookmarkBonus;
      case nsINavHistoryService::TRANSITION_DOWNLOAD:
        return mDownloadVisitBonus;
      case nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT:
        return mPermRedirectVisitBonus;
      case nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY:
        return mTempRedirectVisitBonus;
      case nsINavHistoryService::TRANSITION_RELOAD:
        return mReloadVisitBonus;
      default:
        // 0 == undefined (see bug #375777 for details)
        NS_WARNING_ASSERTION(!aTransitionType,
                             "new transition but no bonus for frecency");
        return mDefaultVisitBonus;
    }
  }

  int32_t GetNumVisitsForFrecency() const { return mNumVisitsForFrecency; }

  /**
   * Updates and invalidates the mDaysOfHistory cache. Should be
   * called whenever a visit is added.
   */
  void UpdateDaysOfHistory(PRTime visitTime);

  /**
   * Get a SQL fragment to pre-cache all the tagged bookmark into a `tagged`
   * CTE.
   */
  static nsLiteralCString GetTagsSqlFragment(const uint16_t aQueryType,
                                             bool aExcludeItems);

  /**
   * Get target folder guid from given query URI.
   * If the folder guid is not found, returns Nonthing().
   */
  static mozilla::Maybe<nsCString> GetTargetFolderGuid(
      const nsACString& aQueryURI);

  /**
   * Store last insterted id for a table.
   */
  static mozilla::Atomic<int64_t> sLastInsertedPlaceId;
  static mozilla::Atomic<int64_t> sLastInsertedVisitId;

  /**
   * Tracks whether frecency is currently being decayed.
   */
  static mozilla::Atomic<bool> sIsFrecencyDecaying;
  /**
   * Tracks whether there's frecency to be recalculated.
   */
  static mozilla::Atomic<bool> sShouldStartFrecencyRecalculation;

  static void StoreLastInsertedId(const nsACString& aTable,
                                  const int64_t aLastInsertedId);

  static nsresult FilterResultSet(
      nsNavHistoryQueryResultNode* aParentNode,
      const nsCOMArray<nsNavHistoryResultNode>& aSet,
      nsCOMArray<nsNavHistoryResultNode>* aFiltered,
      const RefPtr<nsNavHistoryQuery>& aQuery,
      nsNavHistoryQueryOptions* aOptions);

  static void InvalidateDaysOfHistory();

  static nsresult TokensToQuery(
      const nsTArray<mozilla::places::QueryKeyValuePair>& aTokens,
      nsNavHistoryQuery* aQuery, nsNavHistoryQueryOptions* aOptions);

 private:
  ~nsNavHistory();

  // used by GetHistoryService
  static nsNavHistory* gHistoryService;

  static mozilla::Atomic<int32_t> sDaysOfHistory;

 protected:
  // Database handle.
  RefPtr<mozilla::places::Database> mDB;

  /**
   * Loads all of the preferences that we use into member variables.
   *
   * @note If mPrefBranch is nullptr, this does nothing.
   */
  void LoadPrefs();

  /**
   * Calculates and returns value for mCachedNow.
   * This is an hack to avoid calling PR_Now() too often, as is the case when
   * we're asked the ageindays of many history entries in a row.  A timer is
   * set which will clear our valid flag after a short timeout.
   */
  PRTime GetNow();
  PRTime mCachedNow;
  nsCOMPtr<nsITimer> mExpireNowTimer;
  /**
   * Called when the cached now value is expired and needs renewal.
   */
  static void expireNowTimerCallback(nsITimer* aTimer, void* aClosure);

  nsresult ConstructQueryString(
      const RefPtr<nsNavHistoryQuery>& aQuery,
      const RefPtr<nsNavHistoryQueryOptions>& aOptions, nsCString& queryString,
      bool& aParamsPresent, StringHash& aAddParams);

  nsresult QueryToSelectClause(const RefPtr<nsNavHistoryQuery>& aQuery,
                               const RefPtr<nsNavHistoryQueryOptions>& aOptions,
                               nsCString* aClause);
  nsresult BindQueryClauseParameters(
      mozIStorageBaseStatement* statement,
      const RefPtr<nsNavHistoryQuery>& aQuery,
      const RefPtr<nsNavHistoryQueryOptions>& aOptions);

  nsresult ResultsAsList(mozIStorageStatement* statement,
                         nsNavHistoryQueryOptions* aOptions,
                         nsCOMArray<nsNavHistoryResultNode>* aResults);

  // effective tld service
  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
  nsCOMPtr<nsIIDNService> mIDNService;

  // localization
  nsCOMPtr<nsIStringBundle> mBundle;
  mozilla::UniquePtr<const mozilla::intl::Collator> mCollator;

  // recent events
  typedef nsTHashMap<nsCStringHashKey, int64_t> RecentEventHash;
  RecentEventHash mRecentTyped;
  RecentEventHash mRecentLink;
  RecentEventHash mRecentBookmark;

  bool CheckIsRecentEvent(RecentEventHash* hashTable, const nsACString& url);
  void ExpireNonrecentEvents(RecentEventHash* hashTable);

  // Whether history is enabled or not.
  // Will mimic value of the places.history.enabled preference.
  bool mHistoryEnabled;

  // Whether or not diacritics must match in history text searches.
  // Will mimic value of the places.search.matchDiacritics preference.
  bool mMatchDiacritics;

  // Frecency preferences.
  int32_t mNumVisitsForFrecency;
  int32_t mFirstBucketCutoffInDays;
  int32_t mSecondBucketCutoffInDays;
  int32_t mThirdBucketCutoffInDays;
  int32_t mFourthBucketCutoffInDays;
  int32_t mFirstBucketWeight;
  int32_t mSecondBucketWeight;
  int32_t mThirdBucketWeight;
  int32_t mFourthBucketWeight;
  int32_t mDefaultWeight;
  int32_t mEmbedVisitBonus;
  int32_t mFramedLinkVisitBonus;
  int32_t mLinkVisitBonus;
  int32_t mTypedVisitBonus;
  int32_t mBookmarkVisitBonus;
  int32_t mDownloadVisitBonus;
  int32_t mPermRedirectVisitBonus;
  int32_t mTempRedirectVisitBonus;
  int32_t mRedirectSourceVisitBonus;
  int32_t mDefaultVisitBonus;
  int32_t mUnvisitedBookmarkBonus;
  int32_t mUnvisitedTypedBonus;
  int32_t mReloadVisitBonus;

  int64_t mTagsFolder;
  int64_t mLastCachedStartOfDay;
  int64_t mLastCachedEndOfDay;
};

#define PLACES_URI_PREFIX "place:"

/* Returns true if the given URI represents a history query. */
inline static bool IsQueryURI(const nsACString& uri) {
  return StringBeginsWith(uri, nsLiteralCString(PLACES_URI_PREFIX));
}

/* Extracts the query string from a query URI. */
inline const nsDependentCSubstring QueryURIToQuery(const nsCString& uri) {
  NS_ASSERTION(IsQueryURI(uri), "should only be called for query URIs");
  return Substring(uri, nsLiteralCString(PLACES_URI_PREFIX).Length());
}

#endif  // nsNavHistory_h_
