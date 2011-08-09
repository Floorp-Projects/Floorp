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
 *   Edward Lee <edward.lee@engineering.uiuc.edu>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *   Marco Bonardo <mak77@bonardo.net>
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

#ifndef nsNavHistory_h_
#define nsNavHistory_h_

#include "nsINavHistoryService.h"
#include "nsPIPlacesDatabase.h"
#include "nsPIPlacesHistoryListenersNotifier.h"
#include "nsIBrowserHistory.h"
#include "nsIGlobalHistory.h"
#include "nsIGlobalHistory3.h"
#include "nsIDownloadHistory.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsIObserverService.h"
#include "nsICollation.h"
#include "nsIStringBundle.h"
#include "nsITimer.h"
#include "nsMaybeWeakPtr.h"
#include "nsCategoryCache.h"
#include "nsICharsetResolver.h"
#include "nsNetCID.h"
#include "nsToolkitCompsCID.h"
#include "nsThreadUtils.h"
#include "nsURIHashKey.h"
#include "nsTHashtable.h"

#include "nsINavBookmarksService.h"
#include "nsIPrivateBrowsingService.h"
#include "nsIFaviconService.h"
#include "nsNavHistoryResult.h"
#include "nsNavHistoryQuery.h"

#include "mozilla/storage.h"
#include "mozilla/storage/StatementCache.h"

#define QUERYUPDATE_TIME 0
#define QUERYUPDATE_SIMPLE 1
#define QUERYUPDATE_COMPLEX 2
#define QUERYUPDATE_COMPLEX_WITH_BOOKMARKS 3
#define QUERYUPDATE_HOST 4

// This magic number specified an uninitialized value for the
// mInPrivateBrowsing member
#define PRIVATEBROWSING_NOTINITED (PRBool(0xffffffff))

// Clamp title and URL to generously large, but not too large, length.
// See bug 319004 for details.
#define URI_LENGTH_MAX 65536
#define TITLE_LENGTH_MAX 4096

// Microsecond timeout for "recent" events such as typed and bookmark following.
// If you typed it more than this time ago, it's not recent.
#define RECENT_EVENT_THRESHOLD PRTime((PRInt64)15 * 60 * PR_USEC_PER_SEC)

#ifdef MOZ_XUL
// Fired after autocomplete feedback has been updated.
#define TOPIC_AUTOCOMPLETE_FEEDBACK_UPDATED "places-autocomplete-feedback-updated"
#endif

// Fired after frecency has been updated.
#define TOPIC_FRECENCY_UPDATED "places-frecency-updated"

// Fired after frecency has been updated.
#define TOPIC_FRECENCY_UPDATED "places-frecency-updated"

// Fired when Places is shutting down.  Any code should stop accessing Places
// APIs after this notification.  If you need to listen for Places shutdown
// you should only use this notification, next ones are intended only for
// internal Places use.
#define TOPIC_PLACES_SHUTDOWN "places-shutdown"
// For Internal use only.  Fired when connection is about to be closed, only
// cleanup tasks should run at this stage, nothing should be added to the
// database, nor APIs should be called.
#define TOPIC_PLACES_WILL_CLOSE_CONNECTION "places-will-close-connection"
// For Internal use only. Fired as the last notification before the connection
// is gone.
#define TOPIC_PLACES_CONNECTION_CLOSING "places-connection-closing"
// Fired when the connection has gone, nothing will work from now on.
#define TOPIC_PLACES_CONNECTION_CLOSED "places-connection-closed"

// Fired when Places found a locked database while initing.
#define TOPIC_DATABASE_LOCKED "places-database-locked"
// Fired after Places inited.
#define TOPIC_PLACES_INIT_COMPLETE "places-init-complete"

namespace mozilla {
namespace places {

  enum HistoryStatementId {
    DB_GET_PAGE_INFO_BY_URL = 0
  , DB_GET_TAGS
  , DB_IS_PAGE_VISITED
  , DB_INSERT_VISIT
  , DB_RECENT_VISIT_OF_URL
  , DB_GET_PAGE_VISIT_STATS
  , DB_UPDATE_PAGE_VISIT_STATS
  , DB_ADD_NEW_PAGE
  , DB_GET_URL_PAGE_INFO
  , DB_SET_PLACE_TITLE
  };

  enum JournalMode {
    // Default SQLite journal mode.
    JOURNAL_DELETE = 0
    // Can reduce fsyncs on Linux when journal is deleted (See bug 460315).
    // We fallback to this mode when WAL is unavailable.
  , JOURNAL_TRUNCATE
    // Unsafe in case of crashes on database swap or low memory.
  , JOURNAL_MEMORY
    // Can reduce number of fsyncs.  We try to use this mode by default.
  , JOURNAL_WAL
  };

} // namespace places
} // namespace mozilla


class mozIAnnotationService;
class nsNavHistory;
class nsNavBookmarks;
class QueryKeyValuePair;
class nsIEffectiveTLDService;
class nsIIDNService;
class PlacesSQLQueryBuilder;
class nsIAutoCompleteController;

// nsNavHistory

class nsNavHistory : public nsSupportsWeakReference
                   , public nsINavHistoryService
                   , public nsIObserver
                   , public nsIBrowserHistory
                   , public nsIGlobalHistory3
                   , public nsIDownloadHistory
                   , public nsICharsetResolver
                   , public nsPIPlacesDatabase
                   , public nsPIPlacesHistoryListenersNotifier
                   , public mozIStorageVacuumParticipant
{
  friend class PlacesSQLQueryBuilder;

public:
  nsNavHistory();

  NS_DECL_ISUPPORTS

  NS_DECL_NSINAVHISTORYSERVICE
  NS_DECL_NSIGLOBALHISTORY2
  NS_DECL_NSIGLOBALHISTORY3
  NS_DECL_NSIDOWNLOADHISTORY
  NS_DECL_NSIBROWSERHISTORY
  NS_DECL_NSIOBSERVER
  NS_DECL_NSPIPLACESDATABASE
  NS_DECL_NSPIPLACESHISTORYLISTENERSNOTIFIER
  NS_DECL_MOZISTORAGEVACUUMPARTICIPANT


  /**
   * Obtains the nsNavHistory object.
   */
  static nsNavHistory *GetSingleton();

  /**
   * Initializes the nsNavHistory object.  This should only be called once.
   */
  nsresult Init();

  /**
   * Used by other components in the places directory such as the annotation
   * service to get a reference to this history object. Returns a pointer to
   * the service if it exists. Otherwise creates one. Returns NULL on error.
   */
  static nsNavHistory *GetHistoryService()
  {
    if (!gHistoryService) {
      nsCOMPtr<nsINavHistoryService> serv =
        do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);
      NS_ENSURE_TRUE(serv, nsnull);
      NS_ASSERTION(gHistoryService, "Should have static instance pointer now");
    }
    return gHistoryService;
  }

  /**
   * Used by other components in the places directory to get a reference to a
   * const version of this history object.
   *
   * @return A pointer to a const version of the service if it exists,
   *         NULL otherwise.
   */
  static const nsNavHistory* GetConstHistoryService()
  {
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
  nsresult GetIdForPage(nsIURI* aURI,
                        PRInt64* _pageId, nsCString& _GUID);

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
   */
  nsresult GetOrCreateIdForPage(nsIURI* aURI,
                                PRInt64* _pageId, nsCString& _GUID);

  nsresult UpdateFrecency(PRInt64 aPlaceId);

  /**
   * Calculate frecencies for places that don't have a valid value yet
   */
  nsresult FixInvalidFrecencies();

  /**
   * Set the frecencies of excluded places so they don't show up in queries
   */
  nsresult FixInvalidFrecenciesForExcludedPlaces();

  /**
   * Returns a pointer to the storage connection used by history. This
   * connection object is also used by the annotation service and bookmarks, so
   * that things can be grouped into transactions across these components.
   *
   * NOT ADDREFed.
   *
   * This connection can only be used in the thread that created it the
   * history service!
   */
  mozIStorageConnection* GetStorageConnection()
  {
    return mDBConn;
  }

  /**
   * These functions return non-owning references to the locale-specific
   * objects for places components.
   */
  nsIStringBundle* GetBundle();
  nsIStringBundle* GetDateFormatBundle();
  nsICollation* GetCollation();
  void GetStringFromName(const PRUnichar* aName, nsACString& aResult);
  void GetAgeInDaysString(PRInt32 aInt, const PRUnichar *aName,
                          nsACString& aResult);
  void GetMonthName(PRInt32 aIndex, nsACString& aResult);
  void GetMonthYear(PRInt32 aMonth, PRInt32 aYear, nsACString& aResult);

  // Returns whether history is enabled or not.
  PRBool IsHistoryDisabled() {
    return !mHistoryEnabled || InPrivateBrowsingMode();
  }

  // Constants for the columns returned by the above statement.
  static const PRInt32 kGetInfoIndex_PageID;
  static const PRInt32 kGetInfoIndex_URL;
  static const PRInt32 kGetInfoIndex_Title;
  static const PRInt32 kGetInfoIndex_RevHost;
  static const PRInt32 kGetInfoIndex_VisitCount;
  static const PRInt32 kGetInfoIndex_VisitDate;
  static const PRInt32 kGetInfoIndex_FaviconURL;
  static const PRInt32 kGetInfoIndex_SessionId;
  static const PRInt32 kGetInfoIndex_ItemId;
  static const PRInt32 kGetInfoIndex_ItemDateAdded;
  static const PRInt32 kGetInfoIndex_ItemLastModified;
  static const PRInt32 kGetInfoIndex_ItemParentId;
  static const PRInt32 kGetInfoIndex_ItemTags;
  static const PRInt32 kGetInfoIndex_Frecency;

  PRInt64 GetTagsFolder();

  // this actually executes a query and gives you results, it is used by
  // nsNavHistoryQueryResultNode
  nsresult GetQueryResults(nsNavHistoryQueryResultNode *aResultNode,
                           const nsCOMArray<nsNavHistoryQuery>& aQueries,
                           nsNavHistoryQueryOptions *aOptions,
                           nsCOMArray<nsNavHistoryResultNode>* aResults);

  // Take a row of kGetInfoIndex_* columns and construct a ResultNode.
  // The row must contain the full set of columns.
  nsresult RowToResult(mozIStorageValueArray* aRow,
                       nsNavHistoryQueryOptions* aOptions,
                       nsNavHistoryResultNode** aResult);
  nsresult QueryRowToResult(PRInt64 aItemId, const nsACString& aURI,
                            const nsACString& aTitle,
                            PRUint32 aAccessCount, PRTime aTime,
                            const nsACString& aFavicon,
                            nsNavHistoryResultNode** aNode);

  nsresult VisitIdToResultNode(PRInt64 visitId,
                               nsNavHistoryQueryOptions* aOptions,
                               nsNavHistoryResultNode** aResult);

  nsresult BookmarkIdToResultNode(PRInt64 aBookmarkId,
                                  nsNavHistoryQueryOptions* aOptions,
                                  nsNavHistoryResultNode** aResult);
  nsresult URIToResultNode(nsIURI* aURI,
                           nsNavHistoryQueryOptions* aOptions,
                           nsNavHistoryResultNode** aResult);

  // used by other places components to send history notifications (for example,
  // when the favicon has changed)
  void SendPageChangedNotification(nsIURI* aURI, PRUint32 aChangedAttribute,
                                   const nsAString& aValue,
                                   const nsACString& aGUID);

  /**
   * Returns current number of days stored in history.
   */
  PRInt32 GetDaysOfHistory();

  // used by query result nodes to update: see comment on body of CanLiveUpdateQuery
  static PRUint32 GetUpdateRequirements(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                                        nsNavHistoryQueryOptions* aOptions,
                                        PRBool* aHasSearchTerms);
  PRBool EvaluateQueryForNode(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                              nsNavHistoryQueryOptions* aOptions,
                              nsNavHistoryResultNode* aNode);

  static nsresult AsciiHostNameFromHostString(const nsACString& aHostName,
                                              nsACString& aAscii);
  void DomainNameFromURI(nsIURI* aURI,
                         nsACString& aDomainName);
  static PRTime NormalizeTime(PRUint32 aRelative, PRTime aOffset);

  // Don't use these directly, inside nsNavHistory use UpdateBatchScoper,
  // else use nsINavHistoryService::RunInBatchMode
  nsresult BeginUpdateBatch();
  nsresult EndUpdateBatch();

  // The level of batches' nesting, 0 when no batches are open.
  PRInt32 mBatchLevel;
  // Current active transaction for a batch.
  mozStorageTransaction* mBatchDBTransaction;

  // better alternative to QueryStringToQueries (in nsNavHistoryQuery.cpp)
  nsresult QueryStringToQueryArray(const nsACString& aQueryString,
                                   nsCOMArray<nsNavHistoryQuery>* aQueries,
                                   nsNavHistoryQueryOptions** aOptions);

  // sets the schema version in the database to match SCHEMA_VERSION
  nsresult UpdateSchemaVersion();

  // Returns true if we are currently in private browsing mode
  PRBool InPrivateBrowsingMode()
  {
    if (mInPrivateBrowsing == PRIVATEBROWSING_NOTINITED) {
      mInPrivateBrowsing = PR_FALSE;
      nsCOMPtr<nsIPrivateBrowsingService> pbs =
        do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
      if (pbs) {
        pbs->GetPrivateBrowsingEnabled(&mInPrivateBrowsing);
      }
    }

    return mInPrivateBrowsing;
  }

  typedef nsDataHashtable<nsCStringHashKey, nsCString> StringHash;

  /**
   * Helper method to finalize a statement
   */
  static nsresult
  FinalizeStatement(mozIStorageStatement *aStatement) {
    nsresult rv;
    if (aStatement) {
      rv = aStatement->Finalize();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
  }

  /**
   * Indicates if it is OK to notify history observers or not.
   *
   * @return true if it is OK to notify, false otherwise.
   */
  bool canNotify() { return mCanNotify; }

  enum RecentEventFlags {
    RECENT_TYPED      = 1 << 0,    // User typed in URL recently
    RECENT_ACTIVATED  = 1 << 1,    // User tapped URL link recently
    RECENT_BOOKMARKED = 1 << 2     // User bookmarked URL recently
  };

  /**
   * Returns any recent activity done with a URL.
   * @return Any recent events associated with this URI.  Each bit is set
   *         according to RecentEventFlags enum values.
   */
  PRUint32 GetRecentFlags(nsIURI *aURI);

  /**
   * Registers a TRANSITION_EMBED visit for the session.
   *
   * @param aURI
   *        URI of the page.
   * @param aTime
   *        Visit time.  Only the last registered visit time is retained.
   */
  void registerEmbedVisit(nsIURI* aURI, PRInt64 aTime);

  /**
   * Returns whether the specified url has a embed visit.
   *
   * @param aURI
   *        URI of the page.
   * @return whether the page has a embed visit.
   */
  bool hasEmbedVisit(nsIURI* aURI);

  /**
   * Clears all registered embed visits.
   */
  void clearEmbedVisits();

  mozIStorageStatement* GetStatementById(
    enum mozilla::places::HistoryStatementId aStatementId
  )
  {
    using namespace mozilla::places;

    NS_ASSERTION(NS_IsMainThread(), "Can only get statement on main thread");

    switch(aStatementId) {
      case DB_GET_PAGE_INFO_BY_URL:
        return GetStatement(mDBGetURLPageInfo);
      case DB_GET_TAGS:
        return GetStatement(mDBGetTags);
      case DB_IS_PAGE_VISITED:
        return GetStatement(mDBIsPageVisited);
      case DB_INSERT_VISIT:
        return GetStatement(mDBInsertVisit);
      case DB_RECENT_VISIT_OF_URL:
        return GetStatement(mDBRecentVisitOfURL);
      case DB_GET_PAGE_VISIT_STATS:
        return GetStatement(mDBGetPageVisitStats);
      case DB_UPDATE_PAGE_VISIT_STATS:
        return GetStatement(mDBUpdatePageVisitStats);
      case DB_ADD_NEW_PAGE:
        return GetStatement(mDBAddNewPage);
      case DB_GET_URL_PAGE_INFO:
        return GetStatement(mDBGetURLPageInfo);
      case DB_SET_PLACE_TITLE:
        return GetStatement(mDBSetPlaceTitle);
    }
    return nsnull;
  }

  /**
   * This cache should be used only for background thread statements.
   *
   * @pre must be running on the background thread of mDBConn.
   */
  mutable mozilla::storage::StatementCache<mozIStorageStatement> mAsyncThreadStatements;
  mutable mozilla::storage::StatementCache<mozIStorageStatement> mStatements;

  template<int N>
  already_AddRefed<mozIStorageStatement>
  GetStatementByStoragePool(const char (&aQuery)[N]) const
  {
    nsDependentCString query(aQuery, N - 1);
    return GetStatementByStoragePool(query);
  }

  already_AddRefed<mozIStorageStatement>
  GetStatementByStoragePool(const nsACString& aQuery) const
  {
    return NS_IsMainThread()
      ? mStatements.GetCachedStatement(aQuery)
      : mAsyncThreadStatements.GetCachedStatement(aQuery);
  }

  PRInt32 GetFrecencyAgedWeight(PRInt32 aAgeInDays) const
  {
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

  PRInt32 GetFrecencyBucketWeight(PRInt32 aBucketIndex) const
  {
    switch(aBucketIndex) {
      case 1:
        return mFirstBucketWeight;
      case 2:
        return mSecondBucketWeight;
      case 3:
        return mThirdBucketWeight;
      case 4:
        return mFourthBucketWeight;
      default:
        return mDefaultWeight;
    }
  }

  PRInt32 GetFrecencyTransitionBonus(PRInt32 aTransitionType,
                                     bool aVisited) const
  {
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
      default:
        // 0 == undefined (see bug #375777 for details)
        NS_WARN_IF_FALSE(!aTransitionType, "new transition but no bonus for frecency");
        return mDefaultVisitBonus;
    }
  }

  PRInt32 GetNumVisitsForFrecency() const
  {
    return mNumVisitsForFrecency;
  }

  PRInt64 GetNewSessionID();

  /**
   * Fires onVisit event to nsINavHistoryService observers
   */
  void NotifyOnVisit(nsIURI* aURI,
                     PRInt64 aVisitID,
                     PRTime aTime,
                     PRInt64 aSessionID,
                     PRInt64 referringVisitID,
                     PRInt32 aTransitionType,
                     const nsACString& aGUID);

  /**
   * Fires onTitleChanged event to nsINavHistoryService observers
   */
  void NotifyTitleChange(nsIURI* aURI,
                         const nsString& title,
                         const nsACString& aGUID);

  bool isBatching() {
    return mBatchLevel > 0;
  }

private:
  ~nsNavHistory();

  // used by GetHistoryService
  static nsNavHistory *gHistoryService;

protected:

  nsCOMPtr<nsIPrefBranch2> mPrefBranch; // MAY BE NULL when we are shutting down

  nsDataHashtable<nsStringHashKey, int> gExpandedItems;

  //
  // Database stuff
  //
  nsCOMPtr<mozIStorageService> mDBService;
  nsCOMPtr<mozIStorageConnection> mDBConn;
  nsCOMPtr<nsIFile> mDBFile;
  PRInt32 mDBPageSize;


  /**
   * Always use this getter and never use directly the statement nsCOMPtr.
   */
  mozIStorageStatement* GetStatement(const nsCOMPtr<mozIStorageStatement>& aStmt);

  // These are mutable because GetStatementById needs to be const, but these are
  // lazy inited.
  nsCOMPtr<mozIStorageStatement> mDBGetURLPageInfo;   // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBGetIdPageInfo;     // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBRecentVisitOfURL; // converts URL into most recent visit ID/session ID
  nsCOMPtr<mozIStorageStatement> mDBRecentVisitOfPlace; // converts placeID into most recent visit ID/session ID
  nsCOMPtr<mozIStorageStatement> mDBInsertVisit; // used by AddVisit
  nsCOMPtr<mozIStorageStatement> mDBGetPageVisitStats; // used by AddVisit
  nsCOMPtr<mozIStorageStatement> mDBIsPageVisited; // used by IsURIStringVisited
  nsCOMPtr<mozIStorageStatement> mDBUpdatePageVisitStats; // used by AddVisit
  nsCOMPtr<mozIStorageStatement> mDBAddNewPage; // used by InternalAddNewPage
  nsCOMPtr<mozIStorageStatement> mDBGetTags; // used by GetTags
  nsCOMPtr<mozIStorageStatement> mDBGetItemsWithAnno; // used by AutoComplete::StartSearch and FilterResultSet
  nsCOMPtr<mozIStorageStatement> mDBSetPlaceTitle; // used by SetPageTitleInternal
  nsCOMPtr<mozIStorageStatement> mDBVisitToURLResult; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBVisitToVisitResult; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBBookmarkToUrlResult; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBUrlToUrlResult; // kGetInfoIndex_* results
  nsCOMPtr<mozIStorageStatement> mDBUpdateFrecency;
  nsCOMPtr<mozIStorageStatement> mDBUpdateHiddenOnFrecency;
#ifdef MOZ_XUL
  // AutoComplete stuff
  nsCOMPtr<mozIStorageStatement> mDBFeedbackIncrease;
#endif

  /**
   * Finalize all internal statements.
   */
  nsresult FinalizeStatements();

  /**
   * Decays frecency and inputhistory values.  Runs on idle-daily.
   */
  nsresult DecayFrecency();

  /**
   * Finalizes all Places internal statements, allowing to safely close the
   * database connection.
   */
  nsresult FinalizeInternalStatements();

  // nsICharsetResolver
  NS_DECL_NSICHARSETRESOLVER

  nsresult CalculateFrecency(PRInt64 aPageID, PRInt32 aTyped, PRInt32 aVisitCount, nsCAutoString &aURL, PRInt32 *aFrecency);
  nsresult CalculateFrecencyInternal(PRInt64 aPageID, PRInt32 aTyped, PRInt32 aVisitCount, PRBool aIsBookmarked, PRInt32 *aFrecency);

  /**
   * Initializes the database file.  If the database does not exist, was
   * corrupted, or aForceInit is true, we recreate the database.  We also backup
   * the database if it was corrupted or aForceInit is true.
   *
   * @param aForceInit
   *        Indicates if we should close an open database connection or not.
   *        Note: A valid database connection must be opened if this is true.
   */
  nsresult InitDBFile(PRBool aForceInit);

  /**
   * Set journal mode on the database.
   */
  nsresult SetJournalMode(enum mozilla::places::JournalMode aJournalMode);
  enum mozilla::places::JournalMode mCurrentJournalMode;

  /**
   * Initializes the database.  This performs any necessary migrations for the
   * database.  All migration is done inside a transaction that is rolled back
   * if any error occurs.  Upon initialization, history is imported, and some
   * preferences that are used are set.
   */
  nsresult InitDB();

  /**
   * Initializes additional database items like functions, triggers, and
   * statements.
   */
  nsresult InitAdditionalDBItems();
  nsresult InitFunctions();
  nsresult InitTriggers();
  nsresult CheckAndUpdateGUIDs();
  nsresult MigrateV7Up(mozIStorageConnection *aDBConn);
  nsresult MigrateV8Up(mozIStorageConnection *aDBConn);
  nsresult MigrateV9Up(mozIStorageConnection *aDBConn);
  nsresult MigrateV10Up(mozIStorageConnection *aDBConn);
  nsresult MigrateV11Up(mozIStorageConnection *aDBConn);

  nsresult RemovePagesInternal(const nsCString& aPlaceIdsQueryString);
  nsresult PreparePlacesForVisitsDelete(const nsCString& aPlaceIdsQueryString);
  nsresult CleanupPlacesOnVisitsDelete(const nsCString& aPlaceIdsQueryString);

  nsresult AddURIInternal(nsIURI* aURI, PRTime aTime, PRBool aRedirect,
                          PRBool aToplevel, nsIURI* aReferrer);

  nsresult AddVisitChain(nsIURI* aURI, PRTime aTime,
                         PRBool aToplevel, PRBool aRedirect,
                         nsIURI* aReferrer, PRInt64* aVisitID,
                         PRInt64* aSessionID);
  nsresult InternalAddNewPage(nsIURI* aURI, const nsAString& aTitle,
                              PRBool aHidden, PRBool aTyped,
                              PRInt32 aVisitCount, PRBool aCalculateFrecency,
                              PRInt64* aPageID, nsACString& guid);
  nsresult InternalAddVisit(PRInt64 aPageID, PRInt64 aReferringVisit,
                            PRInt64 aSessionID, PRTime aTime,
                            PRInt32 aTransitionType, PRInt64* aVisitID);
  PRBool FindLastVisit(nsIURI* aURI,
                       PRInt64* aVisitID,
                       PRTime* aTime,
                       PRInt64* aSessionID);
  PRBool IsURIStringVisited(const nsACString& url);

  /**
   * Loads all of the preferences that we use into member variables.
   *
   * @note If mPrefBranch is NULL, this does nothing.
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

  nsresult ConstructQueryString(const nsCOMArray<nsNavHistoryQuery>& aQueries, 
                                nsNavHistoryQueryOptions* aOptions,
                                nsCString& queryString,
                                PRBool& aParamsPresent,
                                StringHash& aAddParams);

  nsresult QueryToSelectClause(nsNavHistoryQuery* aQuery,
                               nsNavHistoryQueryOptions* aOptions,
                               PRInt32 aQueryIndex,
                               nsCString* aClause);
  nsresult BindQueryClauseParameters(mozIStorageStatement* statement,
                                     PRInt32 aQueryIndex,
                                     nsNavHistoryQuery* aQuery,
                                     nsNavHistoryQueryOptions* aOptions);

  nsresult ResultsAsList(mozIStorageStatement* statement,
                         nsNavHistoryQueryOptions* aOptions,
                         nsCOMArray<nsNavHistoryResultNode>* aResults);

  void TitleForDomain(const nsCString& domain, nsACString& aTitle);

  nsresult SetPageTitleInternal(nsIURI* aURI, const nsAString& aTitle);

  nsresult FilterResultSet(nsNavHistoryQueryResultNode *aParentNode,
                           const nsCOMArray<nsNavHistoryResultNode>& aSet,
                           nsCOMArray<nsNavHistoryResultNode>* aFiltered,
                           const nsCOMArray<nsNavHistoryQuery>& aQueries,
                           nsNavHistoryQueryOptions* aOptions);

  // observers
  nsMaybeWeakPtrArray<nsINavHistoryObserver> mObservers;

  // effective tld service
  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
  nsCOMPtr<nsIIDNService>          mIDNService;

  // localization
  nsCOMPtr<nsIStringBundle> mBundle;
  nsCOMPtr<nsIStringBundle> mDateFormatBundle;
  nsCOMPtr<nsICollation> mCollation;

  // annotation service : MAY BE NULL!
  //nsCOMPtr<mozIAnnotationService> mAnnotationService;

  // recent events
  typedef nsDataHashtable<nsCStringHashKey, PRInt64> RecentEventHash;
  RecentEventHash mRecentTyped;
  RecentEventHash mRecentLink;
  RecentEventHash mRecentBookmark;

  // Embed visits tracking.
  class VisitHashKey : public nsURIHashKey
  {
  public:
    VisitHashKey(const nsIURI* aURI)
    : nsURIHashKey(aURI)
    {
    }
    VisitHashKey(const VisitHashKey& aOther)
    : nsURIHashKey(aOther)
    {
      NS_NOTREACHED("Do not call me!");
    }
    PRTime visitTime;
  };

  nsTHashtable<VisitHashKey> mEmbedVisits;

  PRBool CheckIsRecentEvent(RecentEventHash* hashTable,
                            const nsACString& url);
  void ExpireNonrecentEvents(RecentEventHash* hashTable);

  // redirect tracking. See GetRedirectFor for a description of how this works.
  struct RedirectInfo {
    nsCString mSourceURI;
    PRTime mTimeCreated;
    PRUint32 mType; // one of TRANSITION_REDIRECT_[TEMPORARY,PERMANENT]
  };
  typedef nsDataHashtable<nsCStringHashKey, RedirectInfo> RedirectHash;
  RedirectHash mRecentRedirects;
  static PLDHashOperator ExpireNonrecentRedirects(
      nsCStringHashKey::KeyType aKey, RedirectInfo& aData, void* aUserArg);
  PRBool GetRedirectFor(const nsACString& aDestination, nsACString& aSource,
                        PRTime* aTime, PRUint32* aRedirectType);

  // Sessions tracking.
  PRInt64 mLastSessionID;

#ifdef MOZ_XUL
  nsresult AutoCompleteFeedback(PRInt32 aIndex,
                                nsIAutoCompleteController *aController);
#endif

  // Whether history is enabled or not.
  // Will mimic value of the places.history.enabled preference.
  PRBool mHistoryEnabled;

  // Frecency preferences.
  PRInt32 mNumVisitsForFrecency;
  PRInt32 mFirstBucketCutoffInDays;
  PRInt32 mSecondBucketCutoffInDays;
  PRInt32 mThirdBucketCutoffInDays;
  PRInt32 mFourthBucketCutoffInDays;
  PRInt32 mFirstBucketWeight;
  PRInt32 mSecondBucketWeight;
  PRInt32 mThirdBucketWeight;
  PRInt32 mFourthBucketWeight;
  PRInt32 mDefaultWeight;
  PRInt32 mEmbedVisitBonus;
  PRInt32 mFramedLinkVisitBonus;
  PRInt32 mLinkVisitBonus;
  PRInt32 mTypedVisitBonus;
  PRInt32 mBookmarkVisitBonus;
  PRInt32 mDownloadVisitBonus;
  PRInt32 mPermRedirectVisitBonus;
  PRInt32 mTempRedirectVisitBonus;
  PRInt32 mDefaultVisitBonus;
  PRInt32 mUnvisitedBookmarkBonus;
  PRInt32 mUnvisitedTypedBonus;

  // in nsNavHistoryQuery.cpp
  nsresult TokensToQueries(const nsTArray<QueryKeyValuePair>& aTokens,
                           nsCOMArray<nsNavHistoryQuery>* aQueries,
                           nsNavHistoryQueryOptions* aOptions);

  PRInt64 mTagsFolder;

  PRBool mInPrivateBrowsing;

  PRUint16 mDatabaseStatus;

  PRInt8 mHasHistoryEntries;

  // Used to enable and disable the observer notifications
  bool mCanNotify;
  nsCategoryCache<nsINavHistoryObserver> mCacheObservers;
};


#define PLACES_URI_PREFIX "place:"

/* Returns true if the given URI represents a history query. */
inline PRBool IsQueryURI(const nsCString &uri)
{
  return StringBeginsWith(uri, NS_LITERAL_CSTRING(PLACES_URI_PREFIX));
}

/* Extracts the query string from a query URI. */
inline const nsDependentCSubstring QueryURIToQuery(const nsCString &uri)
{
  NS_ASSERTION(IsQueryURI(uri), "should only be called for query URIs");
  return Substring(uri, NS_LITERAL_CSTRING(PLACES_URI_PREFIX).Length());
}

#endif // nsNavHistory_h_
