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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com> (original author)
 *   Dietrich Ayala <dietrich@mozilla.com>
 *   Drew Willcoxon <adw@mozilla.com>
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

#include "nsAppDirectoryServiceDefs.h"
#include "nsNavBookmarks.h"
#include "nsNavHistory.h"
#include "mozStorageHelper.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsIDynamicContainer.h"
#include "nsUnicharUtils.h"
#include "nsFaviconService.h"
#include "nsAnnotationService.h"
#include "nsPrintfCString.h"
#include "nsIUUIDGenerator.h"
#include "prprf.h"
#include "nsILivemarkService.h"
#include "nsPlacesTriggers.h"
#include "nsPlacesTables.h"
#include "nsPlacesIndexes.h"
#include "nsPlacesMacros.h"
#include "Helpers.h"

#include "mozilla/FunctionTimer.h"
#include "mozilla/Util.h"

#define BOOKMARKS_TO_KEYWORDS_INITIAL_CACHE_SIZE 64
#define RECENT_BOOKMARKS_INITIAL_CACHE_SIZE 10
// Threashold to expire old bookmarks if the initial cache size is exceeded.
#define RECENT_BOOKMARKS_THRESHOLD PRTime((PRInt64)1 * 60 * PR_USEC_PER_SEC)

#define BEGIN_CRITICAL_BOOKMARK_CACHE_SECTION(_itemId_) \
  mRecentBookmarksCache.RemoveEntry(_itemId_)

#define END_CRITICAL_BOOKMARK_CACHE_SECTION(_itemId_) \
  MOZ_ASSERT(!mRecentBookmarksCache.GetEntry(_itemId_))

#define TOPIC_PLACES_MAINTENANCE "places-maintenance-finished"

using namespace mozilla;

const PRInt32 nsNavBookmarks::kFindURIBookmarksIndex_Id = 0;
const PRInt32 nsNavBookmarks::kFindURIBookmarksIndex_Guid = 1;
const PRInt32 nsNavBookmarks::kFindURIBookmarksIndex_ParentId = 2;
const PRInt32 nsNavBookmarks::kFindURIBookmarksIndex_LastModified = 3;
const PRInt32 nsNavBookmarks::kFindURIBookmarksIndex_ParentGuid = 4;
const PRInt32 nsNavBookmarks::kFindURIBookmarksIndex_GrandParentId = 5;

// These columns sit to the right of the kGetInfoIndex_* columns.
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Position = 14;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Type = 15;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_PlaceID = 16;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_ServiceContractId = 17;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Guid = 18;

const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_Id = 0;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_Url = 1;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_Title = 2;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_Position = 3;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_PlaceId = 4;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_ParentId = 5;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_Type = 6;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_ServiceContractId = 7;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_DateAdded = 8;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_LastModified = 9;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_Guid = 10;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_ParentGuid = 11;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_GrandParentId = 12;

using namespace mozilla::places;

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsNavBookmarks, gBookmarksService)

#define BOOKMARKS_ANNO_PREFIX "bookmarks/"
#define BOOKMARKS_TOOLBAR_FOLDER_ANNO NS_LITERAL_CSTRING(BOOKMARKS_ANNO_PREFIX "toolbarFolder")
#define GUID_ANNO NS_LITERAL_CSTRING("placesInternal/GUID")
#define READ_ONLY_ANNO NS_LITERAL_CSTRING("placesInternal/READ_ONLY")


namespace {

struct keywordSearchData
{
  PRInt64 itemId;
  nsString keyword;
};

PLDHashOperator
SearchBookmarkForKeyword(nsTrimInt64HashKey::KeyType aKey,
                         const nsString aValue,
                         void* aUserArg)
{
  keywordSearchData* data = reinterpret_cast<keywordSearchData*>(aUserArg);
  if (data->keyword.Equals(aValue)) {
    data->itemId = aKey;
    return PL_DHASH_STOP;
  }
  return PL_DHASH_NEXT;
}

template<typename Method, typename DataType>
class AsyncGetBookmarksForURI : public AsyncStatementCallback
{
public:
  AsyncGetBookmarksForURI(nsNavBookmarks* aBookmarksSvc,
                          Method aCallback,
                          const DataType& aData)
  : mBookmarksSvc(aBookmarksSvc)
  , mCallback(aCallback)
  , mData(aData)
  {
  }

  void Init()
  {
    nsCOMPtr<mozIStorageStatement> stmt =
      mBookmarksSvc->GetStatementById(DB_GET_BOOKMARKS_FOR_URI);
    if (stmt) {
      (void)URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"),
                            mData.bookmark.url);
      nsCOMPtr<mozIStoragePendingStatement> pendingStmt;
      (void)stmt->ExecuteAsync(this, getter_AddRefs(pendingStmt));
    }
  }

  NS_IMETHOD HandleResult(mozIStorageResultSet* aResultSet)
  {
    nsCOMPtr<mozIStorageRow> row;
    while (NS_SUCCEEDED(aResultSet->GetNextRow(getter_AddRefs(row))) && row) {
      // Skip tags, for the use-cases of this async getter they are useless.
      PRInt64 grandParentId, tagsFolderId;
      nsresult rv = row->GetInt64(5, &grandParentId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mBookmarksSvc->GetTagsFolder(&tagsFolderId);
      NS_ENSURE_SUCCESS(rv, rv);
      if (grandParentId == tagsFolderId) {
        continue;
      }

      mData.bookmark.grandParentId = grandParentId;
      rv = row->GetInt64(0, &mData.bookmark.id);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = row->GetUTF8String(1, mData.bookmark.guid);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = row->GetInt64(2, &mData.bookmark.parentId);
      NS_ENSURE_SUCCESS(rv, rv);
      // lastModified (3) should not be set for the use-cases of this getter.
      rv = row->GetUTF8String(4, mData.bookmark.parentGuid);
      NS_ENSURE_SUCCESS(rv, rv);

      if (mCallback) {
        ((*mBookmarksSvc).*mCallback)(mData);
      }
    }
    return NS_OK;
  }

private:
  nsRefPtr<nsNavBookmarks> mBookmarksSvc;
  Method mCallback;
  DataType mData;
};

static PLDHashOperator
ExpireNonrecentBookmarksCallback(BookmarkKeyClass* aKey,
                                 void* userArg)
{
  PRInt64* threshold = reinterpret_cast<PRInt64*>(userArg);
  if (aKey->creationTime < *threshold) {
    return PL_DHASH_REMOVE;
  }
  return PL_DHASH_NEXT;
}

static void
ExpireNonrecentBookmarks(nsTHashtable<BookmarkKeyClass>* hashTable)
{
  if (hashTable->Count() > RECENT_BOOKMARKS_INITIAL_CACHE_SIZE) {
    PRInt64 threshold = PR_Now() - RECENT_BOOKMARKS_THRESHOLD;
    (void)hashTable->EnumerateEntries(ExpireNonrecentBookmarksCallback,
                                      reinterpret_cast<void*>(&threshold));
  }
}

static PLDHashOperator
ExpireRecentBookmarksByParentCallback(BookmarkKeyClass* aKey,
                                      void* userArg)
{
  PRInt64* parentId = reinterpret_cast<PRInt64*>(userArg);
  if (aKey->bookmark.parentId == *parentId) {
    return PL_DHASH_REMOVE;
  }
  return PL_DHASH_NEXT;
}

static void
ExpireRecentBookmarksByParent(nsTHashtable<BookmarkKeyClass>* hashTable,
                              PRInt64 aParentId)
{
  (void)hashTable->EnumerateEntries(ExpireRecentBookmarksByParentCallback,
                                    reinterpret_cast<void*>(&aParentId));
}

} // Anonymous namespace.


nsNavBookmarks::nsNavBookmarks() : mItemCount(0)
                                 , mRoot(0)
                                 , mMenuRoot(0)
                                 , mTagsRoot(0)
                                 , mUnfiledRoot(0)
                                 , mToolbarRoot(0)
                                 , mCanNotify(false)
                                 , mCacheObservers("bookmark-observers")
                                 , mShuttingDown(false)
                                 , mBatching(false)
{
  NS_ASSERTION(!gBookmarksService,
               "Attempting to create two instances of the service!");
  gBookmarksService = this;
}


nsNavBookmarks::~nsNavBookmarks()
{
  NS_ASSERTION(gBookmarksService == this,
               "Deleting a non-singleton instance of the service");
  if (gBookmarksService == this)
    gBookmarksService = nsnull;
}


NS_IMPL_ISUPPORTS4(nsNavBookmarks,
                   nsINavBookmarksService,
                   nsINavHistoryObserver,
                   nsIAnnotationObserver,
                   nsIObserver)


nsresult
nsNavBookmarks::Init()
{
  NS_TIME_FUNCTION;

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  mDBConn = history->GetStorageConnection();
  NS_ENSURE_STATE(mDBConn);

  mRecentBookmarksCache.Init(RECENT_BOOKMARKS_INITIAL_CACHE_SIZE);
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    (void)os->AddObserver(this, TOPIC_PLACES_MAINTENANCE, PR_FALSE);
    (void)os->AddObserver(this, TOPIC_PLACES_SHUTDOWN, PR_FALSE);
  }

  // Get our read-only cloned connection.
  nsresult rv = mDBConn->Clone(PR_TRUE, getter_AddRefs(mDBReadOnlyConn));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint16 dbStatus;
  rv = history->GetDatabaseStatus(&dbStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = InitRoots(dbStatus != nsINavHistoryService::DATABASE_STATUS_OK);
  NS_ENSURE_SUCCESS(rv, rv);

  mCanNotify = true;

  // Observe annotations.
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  annosvc->AddObserver(this);

  // Allows us to notify on title changes. MUST BE LAST so it is impossible
  // to fail after this call, or the history service will have a reference to
  // us and we won't go away.
  history->AddObserver(this, PR_FALSE);

  // DO NOT PUT STUFF HERE that can fail. See observer comment above.

  return NS_OK;
}


/**
 * All commands that initialize the database schema should be here.
 * This is called from history init after database connection has been
 * established.
 */
nsresult // static
nsNavBookmarks::InitTables(mozIStorageConnection* aDBConn)
{
  bool exists;
  nsresult rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    rv = aDBConn->ExecuteSimpleSQL(CREATE_MOZ_BOOKMARKS);
    NS_ENSURE_SUCCESS(rv, rv);

    // This index will make it faster to determine if a given item is
    // bookmarked (used by history queries and vacuuming, for example).
    // Making it compound with "type" speeds up type-differentiation
    // queries, such as expiration and search.
    rv = aDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PLACETYPE);
    NS_ENSURE_SUCCESS(rv, rv);

    // The most common operation is to find the children given a parent and position.
    rv = aDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PARENTPOSITION);
    NS_ENSURE_SUCCESS(rv, rv);

    // fast access to lastModified is useful during sync and to get
    // last modified bookmark title for tags container's children.
    rv = aDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PLACELASTMODIFIED);
    NS_ENSURE_SUCCESS(rv, rv);

    // Selecting by guid needs to be fast.
    rv = aDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_GUID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_roots"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    rv = aDBConn->ExecuteSimpleSQL(CREATE_MOZ_BOOKMARKS_ROOTS);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_keywords"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    rv = aDBConn->ExecuteSimpleSQL(CREATE_MOZ_KEYWORDS);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create trigger to update as well
    rv = aDBConn->ExecuteSimpleSQL(CREATE_KEYWORD_VALIDITY_TRIGGER);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


mozIStorageStatement*
nsNavBookmarks::GetStatement(const nsCOMPtr<mozIStorageStatement>& aStmt)
{
  if (mShuttingDown)
    return nsnull;

  // Double ordering covers possible lastModified ties, that could happen when
  // importing, syncing or due to extensions.
  // Note: not using a JOIN is cheaper in this case.
  RETURN_IF_STMT(mDBFindURIBookmarks, NS_LITERAL_CSTRING(
    "SELECT b.id, b.guid, b.parent, b.lastModified, t.guid, t.parent "
    "FROM moz_bookmarks b "
    "JOIN moz_bookmarks t on t.id = b.parent "
    "WHERE b.fk = (SELECT id FROM moz_places WHERE url = :page_url) "
    "ORDER BY b.lastModified DESC, b.id DESC "));

  // Select all children of a given folder, sorted by position.
  // This is a LEFT JOIN because not all bookmarks types have a place.
  // We construct a result where the first columns exactly match those returned
  // by mDBGetURLPageInfo, and additionally contains columns for position,
  // item_child, and folder_child from moz_bookmarks.
  RETURN_IF_STMT(mDBGetChildren, NS_LITERAL_CSTRING(
    "SELECT h.id, h.url, IFNULL(b.title, h.title), h.rev_host, h.visit_count, "
           "h.last_visit_date, f.url, null, b.id, b.dateAdded, b.lastModified, "
           "b.parent, null, h.frecency, b.position, b.type, b.fk, "
           "b.folder_type, b.guid "
    "FROM moz_bookmarks b "
    "LEFT JOIN moz_places h ON b.fk = h.id "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE b.parent = :parent "
    "ORDER BY b.position ASC"));

  // Get information on a folder.
  // This query has to always return results, so it can't be written as a join,
  // apart doing a left join of 2 subqueries, but the cost would be the same.
  RETURN_IF_STMT(mDBFolderInfo, NS_LITERAL_CSTRING(
    "SELECT count(*), "
            "(SELECT guid FROM moz_bookmarks WHERE id = :parent), "
            "(SELECT parent FROM moz_bookmarks WHERE id = :parent) "
    "FROM moz_bookmarks "
    "WHERE parent = :parent"));

  RETURN_IF_STMT(mDBGetChildAt, NS_LITERAL_CSTRING(
    "SELECT id, fk, type FROM moz_bookmarks "
    "WHERE parent = :parent AND position = :item_index"));

  // Get bookmark/folder/separator properties.
  // This is a LEFT JOIN because not all bookmarks types have a place.
  RETURN_IF_STMT(mDBGetItemProperties, NS_LITERAL_CSTRING(
    "SELECT b.id, h.url, b.title, b.position, b.fk, b.parent, b.type, "
           "b.folder_type, b.dateAdded, b.lastModified, b.guid, "
           "t.guid, t.parent "
    "FROM moz_bookmarks b "
    "LEFT JOIN moz_bookmarks t ON t.id = b.parent "
    "LEFT JOIN moz_places h ON h.id = b.fk "
    "WHERE b.id = :item_id"));

  RETURN_IF_STMT(mDBGetItemIdForGUID, NS_LITERAL_CSTRING(
    "SELECT item_id FROM moz_items_annos "
    "WHERE content = :guid "
    "LIMIT 1"));

  RETURN_IF_STMT(mDBInsertBookmark, NS_LITERAL_CSTRING(
    "INSERT INTO moz_bookmarks "
      "(id, fk, type, parent, position, title, folder_type, "
       "dateAdded, lastModified, guid) "
    "VALUES (:item_id, :page_id, :item_type, :parent, :item_index, "
            ":item_title, :folder_type, :date_added, :last_modified, "
            "GENERATE_GUID())"));

  // Just select position since it's just an int32 and may be faster.
  // We don't actually care about the data, just whether there is any.
  RETURN_IF_STMT(mDBIsBookmarkedInDatabase, NS_LITERAL_CSTRING(
    "SELECT 1 FROM moz_bookmarks WHERE fk = :page_id"));

  RETURN_IF_STMT(mDBIsURIBookmarkedInDatabase, NS_LITERAL_CSTRING(
    "SELECT 1 FROM moz_bookmarks b "
    "JOIN moz_places h ON b.fk = h.id "
    "WHERE h.url = :page_url"));

  // Checks to make sure a place id is a bookmark, and isn't a livemark.
  RETURN_IF_STMT(mDBIsRealBookmark, NS_LITERAL_CSTRING(
    "SELECT id "
    "FROM moz_bookmarks "
    "WHERE fk = :page_id "
      "AND type = :item_type "
      "AND parent NOT IN ("
        "SELECT a.item_id "
        "FROM moz_items_annos a "
        "JOIN moz_anno_attributes n ON a.anno_attribute_id = n.id "
        "WHERE n.name = :anno_name"
      ") "));

  RETURN_IF_STMT(mDBGetLastBookmarkID, NS_LITERAL_CSTRING(
    "SELECT id, guid "
    "FROM moz_bookmarks "
    "ORDER BY ROWID DESC "
    "LIMIT 1"));

  // lastModified is set to the same value as dateAdded.  We do this for
  // performance reasons, since it will allow us to use an index to sort items
  // by date.
  RETURN_IF_STMT(mDBSetItemDateAdded, NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET dateAdded = :date, lastModified = :date "
    "WHERE id = :item_id"));

  RETURN_IF_STMT(mDBSetItemLastModified, NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET lastModified = :date WHERE id = :item_id"));

  RETURN_IF_STMT(mDBSetItemIndex, NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET position = :item_index WHERE id = :item_id"));

  RETURN_IF_STMT(mDBGetKeywordForURI, NS_LITERAL_CSTRING(
    "SELECT k.keyword "
    "FROM moz_places h "
    "JOIN moz_bookmarks b ON b.fk = h.id "
    "JOIN moz_keywords k ON k.id = b.keyword_id "
    "WHERE h.url = :page_url "));

  RETURN_IF_STMT(mDBAdjustPosition, NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET position = position + :delta "
    "WHERE parent = :parent "
      "AND position BETWEEN :from_index AND :to_index"));

  RETURN_IF_STMT(mDBRemoveItem, NS_LITERAL_CSTRING(
    "DELETE FROM moz_bookmarks WHERE id = :item_id"));

  RETURN_IF_STMT(mDBGetLastChildId, NS_LITERAL_CSTRING(
    "SELECT id FROM moz_bookmarks WHERE parent = :parent "
    "ORDER BY position DESC LIMIT 1"));

  RETURN_IF_STMT(mDBMoveItem, NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET parent = :parent, position = :item_index "
    "WHERE id = :item_id "));

  RETURN_IF_STMT(mDBSetItemTitle, NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET title = :item_title, lastModified = :date "
    "WHERE id = :item_id "));

  RETURN_IF_STMT(mDBChangeBookmarkURI, NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET fk = :page_id, lastModified = :date "
    "WHERE id = :item_id "));

  // The next query finds the bookmarked ancestors in a redirects chain.
  // It won't go further than 3 levels of redirects (a->b->c->your_place_id).
  // To make this path 100% correct (up to any level) we would need either:
  //  - A separate hash, build through recursive querying of the database.
  //    This solution was previously implemented, but it had a negative effect
  //    on startup since at each startup we have to recursively query the
  //    database to rebuild a hash that is always the same across sessions.
  //    It must be updated at each visit and bookmarks change too.  The code to
  //    manage it is complex and prone to errors, sometimes causing incorrect
  //    data fetches (for example wrong favicon for a redirected bookmark).
  //  - A better way to track redirects for a visit.
  //    We would need a separate table to track redirects, in the table we would
  //    have visit_id, redirect_session.  To get all sources for
  //    a visit then we could just join this table and get all visit_id that
  //    are in the same redirect_session as our visit.  This has the drawback
  //    that we can't ensure data integrity in the downgrade -> upgrade path,
  //    since an old version would not update the table on new visits.
  //
  // For most cases these levels of redirects should be fine though, it's hard
  // to hit a page that is 4 or 5 levels of redirects below a bookmarked page.
  //
  // As a bonus the query also checks first if place_id is already a bookmark,
  // so you don't have to check that apart.

#define COALESCE_PLACEID \
  "COALESCE(greatgrandparent.place_id, grandparent.place_id, parent.place_id) "

  nsCString redirectsFragment =
    nsPrintfCString(3, "%d,%d",
                    nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
                    nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY);

  RETURN_IF_STMT(mDBFindRedirectedBookmark, NS_LITERAL_CSTRING(
    "SELECT "
      "(SELECT url FROM moz_places WHERE id = :page_id) "
    "FROM moz_bookmarks b "
    "WHERE b.fk = :page_id "
    "UNION ALL " // Not directly bookmarked.
    "SELECT "
      "(SELECT url FROM moz_places WHERE id = " COALESCE_PLACEID ") "
    "FROM moz_historyvisits self "
    "JOIN moz_bookmarks b ON b.fk = " COALESCE_PLACEID
    "LEFT JOIN moz_historyvisits parent ON parent.id = self.from_visit "
    "LEFT JOIN moz_historyvisits grandparent ON parent.from_visit = grandparent.id "
      "AND parent.visit_type IN (") + redirectsFragment + NS_LITERAL_CSTRING(") "
    "LEFT JOIN moz_historyvisits greatgrandparent ON grandparent.from_visit = greatgrandparent.id "
      "AND grandparent.visit_type IN (") + redirectsFragment + NS_LITERAL_CSTRING(") "
    "WHERE self.visit_type IN (") + redirectsFragment + NS_LITERAL_CSTRING(") "
      "AND self.place_id = :page_id "
    "LIMIT 1 " // Stop at the first result.
  ));
#undef COALESCE_PLACEID

  return nsnull;
}


nsresult
nsNavBookmarks::FinalizeStatements() {
  mShuttingDown = true;

  mozIStorageStatement* stmts[] = {
    mDBGetChildren,
    mDBFindURIBookmarks,
    mDBFolderInfo,
    mDBGetChildAt,
    mDBGetItemProperties,
    mDBGetItemIdForGUID,
    mDBInsertBookmark,
    mDBIsBookmarkedInDatabase,
    mDBIsRealBookmark,
    mDBGetLastBookmarkID,
    mDBSetItemDateAdded,
    mDBSetItemLastModified,
    mDBSetItemIndex,
    mDBGetKeywordForURI,
    mDBAdjustPosition,
    mDBRemoveItem,
    mDBGetLastChildId,
    mDBMoveItem,
    mDBSetItemTitle,
    mDBChangeBookmarkURI,
    mDBIsURIBookmarkedInDatabase,
    mDBFindRedirectedBookmark,
  };

  for (PRUint32 i = 0; i < ArrayLength(stmts); i++) {
    nsresult rv = nsNavHistory::FinalizeStatement(stmts[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Since we are shutting down, close the read-only connection.
  (void)mDBReadOnlyConn->AsyncClose(nsnull);

#ifdef DEBUG
  // Sanity check that all bookmarks have guids.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT * "
    "FROM moz_bookmarks "
    "WHERE guid IS NULL "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool haveNullGuids;
  rv = stmt->ExecuteStep(&haveNullGuids);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(!haveNullGuids,
               "Someone added a bookmark without adding a GUID!");
#endif

  return NS_OK;
}


nsresult
nsNavBookmarks::InitRoots(bool aForceCreate)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBReadOnlyConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT root_name, folder_id FROM moz_bookmarks_roots"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsCAutoString rootName;
    rv = stmt->GetUTF8String(0, rootName);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt64 rootId;
    rv = stmt->GetInt64(1, &rootId);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ABORT_IF_FALSE(rootId != 0, "Root id is 0, that is an invalid value.");

    if (rootName.EqualsLiteral("places")) {
      mRoot = rootId;
    }
    else if (rootName.EqualsLiteral("menu")) {
      mMenuRoot = rootId;
    }
    else if (rootName.EqualsLiteral("toolbar")) {
      mToolbarRoot = rootId;
    }
    else if (rootName.EqualsLiteral("tags")) {
      mTagsRoot = rootId;
    }
    else if (rootName.EqualsLiteral("unfiled")) {
      mUnfiledRoot = rootId;
    }
  }

  if (aForceCreate) {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    nsIStringBundle* bundle = history->GetBundle();
    NS_ENSURE_TRUE(bundle, NS_ERROR_OUT_OF_MEMORY);

    mozStorageTransaction transaction(mDBConn, PR_FALSE);

    rv = CreateRoot(NS_LITERAL_CSTRING("places"), &mRoot, 0,
                    nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CreateRoot(NS_LITERAL_CSTRING("menu"), &mMenuRoot, mRoot, bundle,
                    NS_LITERAL_STRING("BookmarksMenuFolderTitle").get());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CreateRoot(NS_LITERAL_CSTRING("toolbar"), &mToolbarRoot, mRoot, bundle,
                    NS_LITERAL_STRING("BookmarksToolbarFolderTitle").get());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CreateRoot(NS_LITERAL_CSTRING("tags"), &mTagsRoot, mRoot, bundle,
                    NS_LITERAL_STRING("TagsFolderTitle").get());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CreateRoot(NS_LITERAL_CSTRING("unfiled"), &mUnfiledRoot, mRoot, bundle,
                    NS_LITERAL_STRING("UnsortedBookmarksFolderTitle").get());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mBatching) {
      ForceWALCheckpoint(mDBConn);
    }
  }

  return NS_OK;
}


nsresult
nsNavBookmarks::CreateRoot(const nsCString& name,
                           PRInt64* _itemId,
                           PRInt64 aParentId,
                           nsIStringBundle* aBundle,
                           const PRUnichar* aTitleStringId)
{
  nsresult rv;

  if (*_itemId == 0) {
    // The root does not exist.  Create a new untitled folder for it.
    rv = CreateFolder(aParentId, EmptyCString(), DEFAULT_INDEX, _itemId);
    NS_ENSURE_SUCCESS(rv, rv);

    // Create a entry  in moz_bookmarks_roots to link the folder to the root.
    nsCOMPtr<mozIStorageStatement> stmt;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO moz_bookmarks_roots (root_name, folder_id) "
      "VALUES (:root_name, :item_id)"
    ), getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("root_name"), name);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), *_itemId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Now set the title on the root.  Notice we do this regardless, to take in
  // could title changes when schema changes.
  if (aTitleStringId) {
    nsXPIDLString title;
    rv = aBundle->GetStringFromName(aTitleStringId, getter_Copies(title));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = SetItemTitle(*_itemId, NS_ConvertUTF16toUTF8(title));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


bool
nsNavBookmarks::IsRealBookmark(PRInt64 aPlaceId)
{
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT_RET(stmt, mDBIsRealBookmark, PR_FALSE);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlaceId);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Binding failed");
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_type"), TYPE_BOOKMARK);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Binding failed");
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"),
                                  NS_LITERAL_CSTRING(LMANNO_FEEDURI));
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Binding failed");

  // If we get any rows, then there exists at least one bookmark corresponding
  // to aPlaceId that is not a livemark item.
  bool isBookmark;
  rv = stmt->ExecuteStep(&isBookmark);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "ExecuteStep failed");
  if (NS_SUCCEEDED(rv))
    return isBookmark;

  return PR_FALSE;
}


// nsNavBookmarks::IsBookmarkedInDatabase
//
//    This checks to see if the specified place_id is actually bookmarked.

nsresult
nsNavBookmarks::IsBookmarkedInDatabase(PRInt64 aPlaceId,
                                       bool* aIsBookmarked)
{
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBIsBookmarkedInDatabase);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlaceId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->ExecuteStep(aIsBookmarked);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


nsresult
nsNavBookmarks::AdjustIndices(PRInt64 aFolderId,
                              PRInt32 aStartIndex,
                              PRInt32 aEndIndex,
                              PRInt32 aDelta)
{
  NS_ASSERTION(aStartIndex >= 0 && aEndIndex <= PR_INT32_MAX &&
               aStartIndex <= aEndIndex, "Bad indices");

  // Expire all cached items for this parent, since all positions are going to
  // change.
  ExpireRecentBookmarksByParent(&mRecentBookmarksCache, aFolderId);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBAdjustPosition);
  nsresult rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("delta"), aDelta);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("from_index"), aStartIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("to_index"), aEndIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
 
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetPlacesRoot(PRInt64* aRoot)
{
  *aRoot = mRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetBookmarksMenuFolder(PRInt64* aRoot)
{
  *aRoot = mMenuRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetToolbarFolder(PRInt64* aFolderId)
{
  *aFolderId = mToolbarRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetTagsFolder(PRInt64* aRoot)
{
  *aRoot = mTagsRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetUnfiledBookmarksFolder(PRInt64* aRoot)
{
  *aRoot = mUnfiledRoot;
  return NS_OK;
}


nsresult
nsNavBookmarks::InsertBookmarkInDB(PRInt64 aPlaceId,
                                   enum ItemType aItemType,
                                   PRInt64 aParentId,
                                   PRInt32 aIndex,
                                   const nsACString& aTitle,
                                   PRTime aDateAdded,
                                   PRTime aLastModified,
                                   const nsAString& aServiceContractId,
                                   PRInt64* _itemId,
                                   nsACString& _guid)
{
  // Check for a valid itemId.
  MOZ_ASSERT(_itemId && (*_itemId == -1 || *_itemId > 0));
  // Check for a valid placeId.
  MOZ_ASSERT(aPlaceId && (aPlaceId == -1 || aPlaceId > 0));

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBInsertBookmark);

  nsresult rv;
  if (*_itemId != -1)
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), *_itemId);
  else
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("item_id"));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aPlaceId != -1)
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlaceId);
  else
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("page_id"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_type"), aItemType);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aParentId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_index"), aIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  // Support NULL titles.
  if (aTitle.IsVoid())
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("item_title"));
  else
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("item_title"), aTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aServiceContractId.IsEmpty()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("folder_type"));
  }
  else {
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("folder_type"),
                                aServiceContractId);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("date_added"), aDateAdded);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aLastModified) {
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("last_modified"),
                               aLastModified);
  }
  else {
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("last_modified"), aDateAdded);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  if (*_itemId == -1) {
    // Get the newly inserted item id and GUID.
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(lastInsertIdStmt, mDBGetLastBookmarkID);
    bool hasResult;
    rv = lastInsertIdStmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(hasResult, NS_ERROR_UNEXPECTED);
    rv = lastInsertIdStmt->GetInt64(0, _itemId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = lastInsertIdStmt->GetUTF8String(1, _guid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aParentId > 0) {
    // Update last modified date of the ancestors.
    // TODO (bug 408991): Doing this for all ancestors would be slow without a
    //                    nested tree, so for now update only the parent.
    rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                             aParentId, aDateAdded);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::InsertBookmark(PRInt64 aFolder,
                               nsIURI* aURI,
                               PRInt32 aIndex,
                               const nsACString& aTitle,
                               PRInt64* aNewBookmarkId)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(aNewBookmarkId);
  NS_ENSURE_ARG_MIN(aIndex, nsINavBookmarksService::DEFAULT_INDEX);

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  PRInt64 placeId;
  nsCAutoString placeGuid;
  nsresult rv = history->GetOrCreateIdForPage(aURI, &placeId, placeGuid);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the correct index for insertion.  This also ensures the parent exists.
  PRInt32 index, folderCount;
  PRInt64 grandParentId;
  nsCAutoString folderGuid;
  rv = FetchFolderInfo(aFolder, &folderCount, folderGuid, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      aIndex >= folderCount) {
    index = folderCount;
  }
  else {
    index = aIndex;
    // Create space for the insertion.
    rv = AdjustIndices(aFolder, index, PR_INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aNewBookmarkId = -1;
  PRTime dateAdded = PR_Now();
  nsCAutoString guid;
  rv = InsertBookmarkInDB(placeId, BOOKMARK, aFolder, index,
                          aTitle, dateAdded, nsnull, EmptyString(),
                          aNewBookmarkId, guid);
  NS_ENSURE_SUCCESS(rv, rv);

  // Recalculate frecency for this entry, since it changed.
  rv = history->UpdateFrecency(placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mBatching) {
    ForceWALCheckpoint(mDBConn);
  }

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemAdded(*aNewBookmarkId, aFolder, index, TYPE_BOOKMARK,
                               aURI, aTitle, dateAdded, guid, folderGuid));

  // If the bookmark has been added to a tag container, notify all
  // bookmark-folder result nodes which contain a bookmark for the new
  // bookmark's url.
  if (grandParentId == mTagsRoot) {
    // Notify a tags change to all bookmarks for this URI.
    nsTArray<BookmarkData> bookmarks;
    rv = GetBookmarksForURI(aURI, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRUint32 i = 0; i < bookmarks.Length(); ++i) {
      // Check that bookmarks doesn't include the current tag itemId.
      MOZ_ASSERT(bookmarks[i].id != *aNewBookmarkId);

      NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                       nsINavBookmarkObserver,
                       OnItemChanged(bookmarks[i].id,
                                     NS_LITERAL_CSTRING("tags"),
                                     PR_FALSE,
                                     EmptyCString(),
                                     bookmarks[i].lastModified,
                                     TYPE_BOOKMARK,
                                     bookmarks[i].parentId,
                                     bookmarks[i].guid,
                                     bookmarks[i].parentGuid));
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RemoveItem(PRInt64 aItemId)
{
  NS_ENSURE_ARG(aItemId != mRoot);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnBeforeItemRemoved(bookmark.id,
                                       bookmark.type,
                                       bookmark.parentId,
                                       bookmark.guid,
                                       bookmark.parentGuid));

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // First, remove item annotations
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  rv = annosvc->RemoveItemAnnotations(bookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);

  if (bookmark.type == TYPE_FOLDER) {
    // If this is a dynamic container, try to notify its service.
    if (!bookmark.serviceCID.IsEmpty()) {
      nsCOMPtr<nsIDynamicContainer> svc =
        do_GetService(bookmark.serviceCID.get());
      if (svc) {
        (void)svc->OnContainerRemoving(bookmark.id);
      }
    }

    // Remove all of the folder's children.
    rv = RemoveFolderChildren(bookmark.id);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  BEGIN_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBRemoveItem);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), bookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Fix indices in the parent.
  if (bookmark.position != DEFAULT_INDEX) {
    rv = AdjustIndices(bookmark.parentId,
                       bookmark.position + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bookmark.lastModified = PR_Now();
  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           bookmark.parentId, bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  END_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  if (!mBatching) {
    ForceWALCheckpoint(mDBConn);
  }

  nsCOMPtr<nsIURI> uri;
  if (bookmark.type == TYPE_BOOKMARK) {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    rv = history->UpdateFrecency(bookmark.placeId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = UpdateKeywordsHashForRemovedBookmark(aItemId);
    NS_ENSURE_SUCCESS(rv, rv);

    // A broken url should not interrupt the removal process.
    (void)NS_NewURI(getter_AddRefs(uri), bookmark.url);
  }

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemRemoved(bookmark.id,
                                 bookmark.parentId,
                                 bookmark.position,
                                 bookmark.type,
                                 uri,
                                 bookmark.guid,
                                 bookmark.parentGuid));

  if (bookmark.type == TYPE_BOOKMARK && bookmark.grandParentId == mTagsRoot &&
      uri) {
    // If the removed bookmark was child of a tag container, notify a tags
    // change to all bookmarks for this URI.
    nsTArray<BookmarkData> bookmarks;
    rv = GetBookmarksForURI(uri, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRUint32 i = 0; i < bookmarks.Length(); ++i) {
      NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                       nsINavBookmarkObserver,
                       OnItemChanged(bookmarks[i].id,
                                     NS_LITERAL_CSTRING("tags"),
                                     PR_FALSE,
                                     EmptyCString(),
                                     bookmarks[i].lastModified,
                                     TYPE_BOOKMARK,
                                     bookmarks[i].parentId,
                                     bookmarks[i].guid,
                                     bookmarks[i].parentGuid));
    }

  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::CreateFolder(PRInt64 aParent, const nsACString& aName,
                             PRInt32 aIndex, PRInt64* aNewFolder)
{
  // NOTE: aParent can be null for root creation, so not checked
  NS_ENSURE_ARG_POINTER(aNewFolder);

  // CreateContainerWithID returns the index of the new folder, but that's not
  // used here.  To avoid any risk of corrupting data should this function
  // be changed, we'll use a local variable to hold it.  The PR_TRUE argument
  // will cause notifications to be sent to bookmark observers.
  PRInt32 localIndex = aIndex;
  nsresult rv = CreateContainerWithID(-1, aParent, aName, EmptyString(),
                                      PR_TRUE, &localIndex, aNewFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::CreateDynamicContainer(PRInt64 aParent,
                                       const nsACString& aName,
                                       const nsAString& aContractId,
                                       PRInt32 aIndex,
                                       PRInt64* aNewFolder)
{
  NS_ENSURE_FALSE(aContractId.IsEmpty(), NS_ERROR_INVALID_ARG);

  nsresult rv = CreateContainerWithID(-1, aParent, aName, aContractId,
                                      PR_FALSE, &aIndex, aNewFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetFolderReadonly(PRInt64 aFolder, bool* aResult)
{
  NS_ENSURE_ARG_MIN(aFolder, 1);
  NS_ENSURE_ARG_POINTER(aResult);

  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = annosvc->ItemHasAnnotation(aFolder, READ_ONLY_ANNO, aResult);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetFolderReadonly(PRInt64 aFolder, bool aReadOnly)
{
  NS_ENSURE_ARG_MIN(aFolder, 1);

  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv;
  if (aReadOnly) {
    rv = annosvc->SetItemAnnotationInt32(aFolder, READ_ONLY_ANNO, 1, 0,
                                         nsAnnotationService::EXPIRE_NEVER);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    bool hasAnno;
    rv = annosvc->ItemHasAnnotation(aFolder, READ_ONLY_ANNO, &hasAnno);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasAnno) {
      rv = annosvc->RemoveItemAnnotation(aFolder, READ_ONLY_ANNO);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}


nsresult
nsNavBookmarks::CreateContainerWithID(PRInt64 aItemId,
                                      PRInt64 aParent,
                                      const nsACString& aTitle,
                                      const nsAString& aContractId,
                                      bool aIsBookmarkFolder,
                                      PRInt32* aIndex,
                                      PRInt64* aNewFolder)
{
  NS_ENSURE_ARG_MIN(*aIndex, nsINavBookmarksService::DEFAULT_INDEX);

  // Get the correct index for insertion.  This also ensures the parent exists.
  PRInt32 index, folderCount;
  PRInt64 grandParentId;
  nsCAutoString folderGuid;
  nsresult rv = FetchFolderInfo(aParent, &folderCount, folderGuid, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  if (*aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      *aIndex >= folderCount) {
    index = folderCount;
  } else {
    index = *aIndex;
    // Create space for the insertion.
    rv = AdjustIndices(aParent, index, PR_INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aNewFolder = aItemId;
  PRTime dateAdded = PR_Now();
  nsCAutoString guid;
  ItemType containerType = aIsBookmarkFolder ? FOLDER
                                             : DYNAMIC_CONTAINER;
  rv = InsertBookmarkInDB(-1, containerType, aParent, index,
                          aTitle, dateAdded, nsnull, aContractId, aNewFolder,
                          guid);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mBatching) {
    ForceWALCheckpoint(mDBConn);
  }

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemAdded(*aNewFolder, aParent, index, containerType,
                               nsnull, aTitle, dateAdded, guid, folderGuid));

  *aIndex = index;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::InsertSeparator(PRInt64 aParent,
                                PRInt32 aIndex,
                                PRInt64* aNewItemId)
{
  NS_ENSURE_ARG_MIN(aParent, 1);
  NS_ENSURE_ARG_MIN(aIndex, nsINavBookmarksService::DEFAULT_INDEX);
  NS_ENSURE_ARG_POINTER(aNewItemId);

  // Get the correct index for insertion.  This also ensures the parent exists.
  PRInt32 index, folderCount;
  PRInt64 grandParentId;
  nsCAutoString folderGuid;
  nsresult rv = FetchFolderInfo(aParent, &folderCount, folderGuid, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  if (aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      aIndex >= folderCount) {
    index = folderCount;
  }
  else {
    index = aIndex;
    // Create space for the insertion.
    rv = AdjustIndices(aParent, index, PR_INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aNewItemId = -1;
  // Set a NULL title rather than an empty string.
  nsCString voidString;
  voidString.SetIsVoid(PR_TRUE);
  nsCAutoString guid;
  PRTime dateAdded = PR_Now();
  rv = InsertBookmarkInDB(-1, SEPARATOR, aParent, index, voidString, dateAdded,
                          nsnull, EmptyString(), aNewItemId, guid);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemAdded(*aNewItemId, aParent, index, TYPE_SEPARATOR,
                               nsnull, voidString, dateAdded, guid, folderGuid));

  return NS_OK;
}


nsresult
nsNavBookmarks::GetLastChildId(PRInt64 aFolderId, PRInt64* aItemId)
{
  NS_ASSERTION(aFolderId > 0, "Invalid folder id");
  *aItemId = -1;

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetLastChildId);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);
  bool found;
  rv = stmt->ExecuteStep(&found);
  NS_ENSURE_SUCCESS(rv, rv);
  if (found) {
    rv = stmt->GetInt64(0, aItemId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetIdForItemAt(PRInt64 aFolder,
                               PRInt32 aIndex,
                               PRInt64* aItemId)
{
  NS_ENSURE_ARG_MIN(aFolder, 1);
  NS_ENSURE_ARG_POINTER(aItemId);

  *aItemId = -1;

  nsresult rv;
  if (aIndex == nsINavBookmarksService::DEFAULT_INDEX) {
    // Get last item within aFolder.
    rv = GetLastChildId(aFolder, aItemId);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Get the item in aFolder with position aIndex.
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetChildAt);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolder);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_index"), aIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    bool found;
    rv = stmt->ExecuteStep(&found);
    NS_ENSURE_SUCCESS(rv, rv);
    if (found) {
      rv = stmt->GetInt64(0, aItemId);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsNavBookmarks::RemoveFolderTransaction, nsITransaction)

NS_IMETHODIMP
nsNavBookmarks::GetRemoveFolderTransaction(PRInt64 aFolderId, nsITransaction** aResult)
{
  NS_ENSURE_ARG_MIN(aFolderId, 1);
  NS_ENSURE_ARG_POINTER(aResult);

  // Create and initialize a RemoveFolderTransaction object that can be used to
  // recreate the folder safely later. 

  RemoveFolderTransaction* rft = 
    new RemoveFolderTransaction(aFolderId);
  if (!rft)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult = rft);
  return NS_OK;
}


nsresult
nsNavBookmarks::GetDescendantChildren(PRInt64 aFolderId,
                                      const nsACString& aFolderGuid,
                                      PRInt64 aGrandParentId,
                                      nsTArray<BookmarkData>& aFolderChildrenArray) {
  // New children will be added from this index on.
  PRUint32 startIndex = aFolderChildrenArray.Length();
  nsresult rv;
  {
    // Collect children informations.
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetChildren);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasMore;
    while (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
      BookmarkData child;
      rv = stmt->GetInt64(nsNavHistory::kGetInfoIndex_ItemId, &child.id);
      NS_ENSURE_SUCCESS(rv, rv);
      child.parentId = aFolderId;
      child.grandParentId = aGrandParentId;
      child.parentGuid = aFolderGuid;
      rv = stmt->GetInt32(kGetChildrenIndex_Type, &child.type);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->GetInt64(kGetChildrenIndex_PlaceID, &child.placeId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->GetInt32(kGetChildrenIndex_Position, &child.position);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->GetUTF8String(kGetChildrenIndex_Guid, child.guid);
      NS_ENSURE_SUCCESS(rv, rv);

      if (child.type == TYPE_BOOKMARK) {
        rv = stmt->GetUTF8String(nsNavHistory::kGetInfoIndex_URL, child.url);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else if (child.type == TYPE_FOLDER) {
        bool isNull;
        rv = stmt->GetIsNull(kGetChildrenIndex_ServiceContractId, &isNull);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!isNull) {
          rv = stmt->GetUTF8String(kGetChildrenIndex_ServiceContractId,
                                   child.serviceCID);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
      // Append item to children's array.
      aFolderChildrenArray.AppendElement(child);
    }
  }

  // Recursively call GetDescendantChildren for added folders.
  // We start at startIndex since previous folders are checked
  // by previous calls to this method.
  PRUint32 childCount = aFolderChildrenArray.Length();
  for (PRUint32 i = startIndex; i < childCount; ++i) {
    if (aFolderChildrenArray[i].type == TYPE_FOLDER) {
      // nsTarray assumes that all children can be memmove()d, thus we can't
      // just pass aFolderChildrenArray[i].guid to a method that will change
      // the array itself.  Otherwise, since it's passed by reference, after a
      // memmove() it could point to garbage and cause intermittent crashes.
      nsCString guid = aFolderChildrenArray[i].guid;
      GetDescendantChildren(aFolderChildrenArray[i].id,
                            guid,
                            aFolderId,
                            aFolderChildrenArray);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RemoveFolderChildren(PRInt64 aFolderId)
{
  NS_ENSURE_ARG_MIN(aFolderId, 1);

  BookmarkData folder;
  nsresult rv = FetchItemInfo(aFolderId, folder);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG(folder.type == TYPE_FOLDER);

  // Fill folder children array recursively.
  nsTArray<BookmarkData> folderChildrenArray;
  rv = GetDescendantChildren(folder.id, folder.guid, folder.parentId,
                             folderChildrenArray);
  NS_ENSURE_SUCCESS(rv, rv);

  // Build a string of folders whose children will be removed.
  nsCString foldersToRemove;
  for (PRUint32 i = 0; i < folderChildrenArray.Length(); ++i) {
    BookmarkData& child = folderChildrenArray[i];

    // Notify observers that we are about to remove this child.
    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavBookmarkObserver,
                     OnBeforeItemRemoved(child.id,
                                         child.type,
                                         child.parentId,
                                         child.guid,
                                         child.parentGuid));

    if (child.type == TYPE_FOLDER) {
      foldersToRemove.AppendLiteral(",");
      foldersToRemove.AppendInt(child.id);

      // If this is a dynamic container, try to notify its service that we
      // are going to remove it.
      // XXX (bug 484094) this should use a bookmark observer!
      if (!child.serviceCID.IsEmpty()) {
        nsCOMPtr<nsIDynamicContainer> svc =
          do_GetService(child.serviceCID.get());
        if (svc) {
          (void)svc->OnContainerRemoving(child.id);
        }
      }
    }

    BEGIN_CRITICAL_BOOKMARK_CACHE_SECTION(child.id);
  }

  // Delete items from the database now.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  nsCOMPtr<mozIStorageStatement> deleteStatement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_bookmarks "
      "WHERE parent IN (:parent") +
        foldersToRemove +
      NS_LITERAL_CSTRING(")"),
    getter_AddRefs(deleteStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteStatement->BindInt64ByName(NS_LITERAL_CSTRING("parent"), folder.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Clean up orphan items annotations.
  rv = mDBConn->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING(
      "DELETE FROM moz_items_annos "
      "WHERE id IN ("
        "SELECT a.id from moz_items_annos a "
        "LEFT JOIN moz_bookmarks b ON a.item_id = b.id "
        "WHERE b.id ISNULL)"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the lastModified date.
  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           folder.id, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < folderChildrenArray.Length(); i++) {
    BookmarkData& child = folderChildrenArray[i];
    if (child.type == TYPE_BOOKMARK) {
      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
      rv = history->UpdateFrecency(child.placeId);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = UpdateKeywordsHashForRemovedBookmark(child.id);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    END_CRITICAL_BOOKMARK_CACHE_SECTION(child.id);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mBatching) {
    ForceWALCheckpoint(mDBConn);
  }

  // Call observers in reverse order to serve children before their parent.
  for (PRInt32 i = folderChildrenArray.Length() - 1; i >= 0; --i) {
    BookmarkData& child = folderChildrenArray[i];
    nsCOMPtr<nsIURI> uri;
    if (child.type == TYPE_BOOKMARK) {
      // A broken url should not interrupt the removal process.
      (void)NS_NewURI(getter_AddRefs(uri), child.url);
    }

    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavBookmarkObserver,
                     OnItemRemoved(child.id,
                                   child.parentId,
                                   child.position,
                                   child.type,
                                   uri,
                                   child.guid,
                                   child.parentGuid));

    if (child.type == TYPE_BOOKMARK && child.grandParentId == mTagsRoot &&
        uri) {
      // If the removed bookmark was a child of a tag container, notify all
      // bookmark-folder result nodes which contain a bookmark for the removed
      // bookmark's url.
      nsTArray<BookmarkData> bookmarks;
      rv = GetBookmarksForURI(uri, bookmarks);
      NS_ENSURE_SUCCESS(rv, rv);

      for (PRUint32 i = 0; i < bookmarks.Length(); ++i) {
        NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                         nsINavBookmarkObserver,
                         OnItemChanged(bookmarks[i].id,
                                       NS_LITERAL_CSTRING("tags"),
                                       PR_FALSE,
                                       EmptyCString(),
                                       bookmarks[i].lastModified,
                                       TYPE_BOOKMARK,
                                       bookmarks[i].parentId,
                                       bookmarks[i].guid,
                                       bookmarks[i].parentGuid));
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::MoveItem(PRInt64 aItemId, PRInt64 aNewParent, PRInt32 aIndex)
{
  NS_ENSURE_TRUE(aItemId != mRoot, NS_ERROR_INVALID_ARG);
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_MIN(aNewParent, 1);
  // -1 is append, but no other negative number is allowed.
  NS_ENSURE_ARG_MIN(aIndex, -1);
  // Disallow making an item its own parent.
  NS_ENSURE_TRUE(aItemId != aNewParent, NS_ERROR_INVALID_ARG);

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  // if parent and index are the same, nothing to do
  if (bookmark.parentId == aNewParent && bookmark.position == aIndex)
    return NS_OK;

  // Make sure aNewParent is not aFolder or a subfolder of aFolder.
  // TODO: make this performant, maybe with a nested tree (bug 408991).
  if (bookmark.type == TYPE_FOLDER) {
    PRInt64 ancestorId = aNewParent;

    while (ancestorId) {
      if (ancestorId == bookmark.id) {
        return NS_ERROR_INVALID_ARG;
      }
      rv = GetFolderIdForItem(ancestorId, &ancestorId);
      if (NS_FAILED(rv)) {
        break;
      }
    }
  }

  // calculate new index
  PRInt32 newIndex, folderCount;
  PRInt64 grandParentId;
  nsCAutoString newParentGuid;
  rv = FetchFolderInfo(aNewParent, &folderCount, newParentGuid, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      aIndex >= folderCount) {
    newIndex = folderCount;
    // If the parent remains the same, then the folder is really being moved
    // to count - 1 (since it's being removed from the old position)
    if (bookmark.parentId == aNewParent) {
      --newIndex;
    }
  } else {
    newIndex = aIndex;

    if (bookmark.parentId == aNewParent && newIndex > bookmark.position) {
      // when an item is being moved lower in the same folder, the new index
      // refers to the index before it was removed. Removal causes everything
      // to shift up.
      --newIndex;
    }
  }

  // this is like the previous check, except this covers if
  // the specified index was -1 (append), and the calculated
  // new index is the same as the existing index
  if (aNewParent == bookmark.parentId && newIndex == bookmark.position) {
    // Nothing to do!
    return NS_OK;
  }

  // adjust indices to account for the move
  // do this before we update the parent/index fields
  // or we'll re-adjust the index for the item we are moving
  if (bookmark.parentId == aNewParent) {
    // We can optimize the updates if moving within the same container.
    // We only shift the items between the old and new positions, since the
    // insertion will offset the deletion.
    if (bookmark.position > newIndex) {
      rv = AdjustIndices(bookmark.parentId, newIndex, bookmark.position - 1, 1);
    }
    else {
      rv = AdjustIndices(bookmark.parentId, bookmark.position + 1, newIndex, -1);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // We're moving between containers, so this happens in two steps.
    // First, fill the hole from the removal from the old parent.
    rv = AdjustIndices(bookmark.parentId, bookmark.position + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
    // Now, make room in the new parent for the insertion.
    rv = AdjustIndices(aNewParent, newIndex, PR_INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  BEGIN_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  {
    // Update parent and position.
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBMoveItem);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aNewParent);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_index"), newIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), bookmark.id);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRTime now = PR_Now();
  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           bookmark.parentId, now);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           aNewParent, now);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  END_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemMoved(bookmark.id,
                               bookmark.parentId,
                               bookmark.position,
                               aNewParent,
                               newIndex,
                               bookmark.type,
                               bookmark.guid,
                               bookmark.parentGuid,
                               newParentGuid));

  // notify dynamic container provider if there is one
  if (!bookmark.serviceCID.IsEmpty()) {
    nsCOMPtr<nsIDynamicContainer> svc =
      do_GetService(bookmark.serviceCID.get());
    if (svc) {
      (void)svc->OnContainerMoved(bookmark.id, aNewParent, newIndex);
    }
  }
  return NS_OK;
}

nsresult
nsNavBookmarks::FetchItemInfo(PRInt64 aItemId,
                              BookmarkData& _bookmark)
{
  // Check if the requested id is in the recent cache and avoid the database
  // lookup if so.  Invalidate the cache after getting data if requested.
  BookmarkKeyClass* key = mRecentBookmarksCache.GetEntry(aItemId);
  if (key) {
    _bookmark = key->bookmark;
    return NS_OK;
  }

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(hasResult, NS_ERROR_INVALID_ARG);

  _bookmark.id = aItemId;
  rv = stmt->GetUTF8String(kGetItemPropertiesIndex_Url, _bookmark.url);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isNull;
  rv = stmt->GetIsNull(kGetItemPropertiesIndex_Title, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isNull) {
    _bookmark.title.SetIsVoid(PR_TRUE);
  }
  else {
    rv = stmt->GetUTF8String(kGetItemPropertiesIndex_Title, _bookmark.title);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = stmt->GetInt32(kGetItemPropertiesIndex_Position, &_bookmark.position);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt64(kGetItemPropertiesIndex_PlaceId, &_bookmark.placeId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt64(kGetItemPropertiesIndex_ParentId, &_bookmark.parentId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt32(kGetItemPropertiesIndex_Type, &_bookmark.type);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetIsNull(7, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isNull) {
    _bookmark.serviceCID.SetIsVoid(PR_TRUE);
  }
  else {
    rv = stmt->GetUTF8String(kGetItemPropertiesIndex_ServiceContractId,
                             _bookmark.serviceCID);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = stmt->GetInt64(kGetItemPropertiesIndex_DateAdded, &_bookmark.dateAdded);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt64(kGetItemPropertiesIndex_LastModified,
                      &_bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetUTF8String(kGetItemPropertiesIndex_Guid, _bookmark.guid);
  NS_ENSURE_SUCCESS(rv, rv);
  // Getting properties of the root would show no parent.
  rv = stmt->GetIsNull(kGetItemPropertiesIndex_ParentGuid, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isNull) {
    rv = stmt->GetUTF8String(kGetItemPropertiesIndex_ParentGuid,
                             _bookmark.parentGuid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetInt64(kGetItemPropertiesIndex_GrandParentId,
                        &_bookmark.grandParentId);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    _bookmark.grandParentId = -1;
  }

  // Make space for the new entry.
  ExpireNonrecentBookmarks(&mRecentBookmarksCache);
  // Update the recent bookmarks cache.
  key = mRecentBookmarksCache.PutEntry(aItemId);
  if (key) {
    key->bookmark = _bookmark;
  }

  return NS_OK;
}

nsresult
nsNavBookmarks::SetItemDateInternal(mozIStorageStatement* aStatement,
                                    PRInt64 aItemId,
                                    PRTime aValue)
{
  BEGIN_CRITICAL_BOOKMARK_CACHE_SECTION(aItemId);

  NS_ENSURE_STATE(aStatement);
  mozStorageStatementScoper scoper(aStatement);

  nsresult rv = aStatement->BindInt64ByName(NS_LITERAL_CSTRING("date"), aValue);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStatement->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  END_CRITICAL_BOOKMARK_CACHE_SECTION(aItemId);

  // note, we are not notifying the observers
  // that the item has changed.

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemDateAdded(PRInt64 aItemId, PRTime aDateAdded)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);
  bookmark.dateAdded = aDateAdded;

  rv = SetItemDateInternal(GetStatement(mDBSetItemDateAdded),
                           bookmark.id, bookmark.dateAdded);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note: mDBSetItemDateAdded also sets lastModified to aDateAdded.
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(bookmark.id,
                                 NS_LITERAL_CSTRING("dateAdded"),
                                 PR_FALSE,
                                 nsPrintfCString(16, "%lld", bookmark.dateAdded),
                                 bookmark.dateAdded,
                                 bookmark.type,
                                 bookmark.parentId,
                                 bookmark.guid,
                                 bookmark.parentGuid));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemDateAdded(PRInt64 aItemId, PRTime* _dateAdded)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(_dateAdded);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  *_dateAdded = bookmark.dateAdded;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemLastModified(PRInt64 aItemId, PRTime aLastModified)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);
  bookmark.lastModified = aLastModified;

  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           bookmark.id, bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note: mDBSetItemDateAdded also sets lastModified to aDateAdded.
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(bookmark.id,
                                 NS_LITERAL_CSTRING("lastModified"),
                                 PR_FALSE,
                                 nsPrintfCString(16, "%lld", bookmark.lastModified),
                                 bookmark.lastModified,
                                 bookmark.type,
                                 bookmark.parentId,
                                 bookmark.guid,
                                 bookmark.parentGuid));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemLastModified(PRInt64 aItemId, PRTime* _lastModified)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(_lastModified);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  *_lastModified = bookmark.lastModified;
  return NS_OK;
}


nsresult
nsNavBookmarks::GetGUIDBase(nsAString &aGUIDBase)
{
  if (!mGUIDBase.IsEmpty()) {
    aGUIDBase = mGUIDBase;
    return NS_OK;
  }

  // generate a new GUID base for this session
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1");
  NS_ENSURE_TRUE(uuidgen, NS_ERROR_OUT_OF_MEMORY);
  nsID GUID;
  nsresult rv = uuidgen->GenerateUUIDInPlace(&GUID);
  NS_ENSURE_SUCCESS(rv, rv);
  char GUIDChars[NSID_LENGTH];
  GUID.ToProvidedString(GUIDChars);
  CopyASCIItoUTF16(GUIDChars, mGUIDBase);
  aGUIDBase = mGUIDBase;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemGUID(PRInt64 aItemId, nsAString& aGUID)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = annosvc->GetItemAnnotationString(aItemId, GUID_ANNO, aGUID);
  if (NS_SUCCEEDED(rv) || rv != NS_ERROR_NOT_AVAILABLE)
    return rv;

  nsAutoString tmp;
  tmp.AppendInt(mItemCount++);
  aGUID.SetCapacity(NSID_LENGTH - 1 + tmp.Length());
  nsString GUIDBase;
  rv = GetGUIDBase(GUIDBase);
  NS_ENSURE_SUCCESS(rv, rv);
  aGUID.Assign(GUIDBase);
  aGUID.Append(tmp);

  rv = SetItemGUID(aItemId, aGUID);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemGUID(PRInt64 aItemId, const nsAString& aGUID)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  PRInt64 checkId;
  GetItemIdForGUID(aGUID, &checkId);
  if (checkId != -1)
    return NS_ERROR_INVALID_ARG; // invalid GUID, already exists

  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = annosvc->SetItemAnnotationString(aItemId, GUID_ANNO, aGUID, 0,
                                                 nsIAnnotationService::EXPIRE_NEVER);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemIdForGUID(const nsAString& aGUID, PRInt64* aItemId)
{
  NS_ENSURE_ARG_POINTER(aItemId);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemIdForGUID);
  nsresult rv = stmt->BindStringByName(NS_LITERAL_CSTRING("guid"), aGUID);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore = false;
  rv = stmt->ExecuteStep(&hasMore);
  if (NS_FAILED(rv) || ! hasMore) {
    *aItemId = -1;
    return NS_OK; // not found: return -1
  }

  // found, get the itemId
  rv = stmt->GetInt64(0, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemTitle(PRInt64 aItemId, const nsACString& aTitle)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  BEGIN_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(statement, mDBSetItemTitle);
  // Support setting a null title, we support this in insertBookmark.
  if (aTitle.IsVoid()) {
    rv = statement->BindNullByName(NS_LITERAL_CSTRING("item_title"));
  }
  else {
    rv = statement->BindUTF8StringByName(NS_LITERAL_CSTRING("item_title"),
                                         aTitle);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  bookmark.lastModified = PR_Now();
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("date"),
                                  bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), bookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  END_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(bookmark.id,
                                 NS_LITERAL_CSTRING("title"),
                                 PR_FALSE,
                                 aTitle,
                                 bookmark.lastModified,
                                 bookmark.type,
                                 bookmark.parentId,
                                 bookmark.guid,
                                 bookmark.parentGuid));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemTitle(PRInt64 aItemId,
                             nsACString& _title)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  _title = bookmark.title;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetBookmarkURI(PRInt64 aItemId,
                               nsIURI** _URI)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(_URI);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewURI(_URI, bookmark.url);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemType(PRInt64 aItemId, PRUint16* _type)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(_type);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  *_type = static_cast<PRUint16>(bookmark.type);
  return NS_OK;
}


nsresult
nsNavBookmarks::GetFolderType(PRInt64 aItemId,
                              nsACString& _type)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  _type = bookmark.serviceCID;
  return NS_OK;
}


nsresult
nsNavBookmarks::ResultNodeForContainer(PRInt64 aItemId,
                                       nsNavHistoryQueryOptions* aOptions,
                                       nsNavHistoryResultNode** aNode)
{
  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  if (bookmark.type == TYPE_DYNAMIC_CONTAINER) {
    *aNode = new nsNavHistoryContainerResultNode(EmptyCString(),
                                                 bookmark.title,
                                                 EmptyCString(),
                                                 nsINavHistoryResultNode::RESULT_TYPE_DYNAMIC_CONTAINER,
                                                 PR_TRUE,
                                                 bookmark.serviceCID,
                                                 aOptions);
    (*aNode)->mItemId = bookmark.id;
  }
  else if (bookmark.type == TYPE_FOLDER) { // TYPE_FOLDER
    *aNode = new nsNavHistoryFolderResultNode(bookmark.title,
                                              aOptions,
                                              bookmark.id,
                                              EmptyCString());
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  (*aNode)->mDateAdded = bookmark.dateAdded;
  (*aNode)->mLastModified = bookmark.lastModified;

  NS_ADDREF(*aNode);
  return NS_OK;
}


nsresult
nsNavBookmarks::QueryFolderChildren(
  PRInt64 aFolderId,
  nsNavHistoryQueryOptions* aOptions,
  nsCOMArray<nsNavHistoryResultNode>* aChildren)
{
  NS_ENSURE_ARG_POINTER(aOptions);
  NS_ENSURE_ARG_POINTER(aChildren);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetChildren);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(stmt, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 index = -1;
  bool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    rv = ProcessFolderNodeRow(row, aOptions, aChildren, index);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


nsresult
nsNavBookmarks::ProcessFolderNodeRow(
  mozIStorageValueArray* aRow,
  nsNavHistoryQueryOptions* aOptions,
  nsCOMArray<nsNavHistoryResultNode>* aChildren,
  PRInt32& aCurrentIndex)
{
  NS_ENSURE_ARG_POINTER(aRow);
  NS_ENSURE_ARG_POINTER(aOptions);
  NS_ENSURE_ARG_POINTER(aChildren);

  // The results will be in order of aCurrentIndex. Even if we don't add a node
  // because it was excluded, we need to count its index, so do that before
  // doing anything else.
  aCurrentIndex++;

  PRInt32 itemType;
  nsresult rv = aRow->GetInt32(kGetChildrenIndex_Type, &itemType);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt64 id;
  rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemId, &id);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRefPtr<nsNavHistoryResultNode> node;
  if (itemType == TYPE_BOOKMARK) {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    rv = history->RowToResult(aRow, aOptions, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 nodeType;
    node->GetType(&nodeType);
    if ((nodeType == nsINavHistoryResultNode::RESULT_TYPE_QUERY &&
         aOptions->ExcludeQueries()) ||
        (nodeType != nsINavHistoryResultNode::RESULT_TYPE_QUERY &&
         nodeType != nsINavHistoryResultNode::RESULT_TYPE_FOLDER_SHORTCUT &&
         aOptions->ExcludeItems())) {
      return NS_OK;
    }
  }
  else if (itemType == TYPE_FOLDER || itemType == TYPE_DYNAMIC_CONTAINER) {
    if (aOptions->ExcludeReadOnlyFolders()) {
      // If the folder is read-only, skip it.
      bool readOnly;
      if (itemType == TYPE_DYNAMIC_CONTAINER) {
        readOnly = PR_TRUE;
      }
      else {
        readOnly = PR_FALSE;
        GetFolderReadonly(id, &readOnly);
      }
      if (readOnly)
        return NS_OK;
    }
    rv = ResultNodeForContainer(id, aOptions, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // This is a separator.
    if (aOptions->ExcludeItems()) {
      return NS_OK;
    }
    node = new nsNavHistorySeparatorResultNode();
    NS_ENSURE_TRUE(node, NS_ERROR_OUT_OF_MEMORY);

    rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemId, &node->mItemId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemDateAdded,
                        &node->mDateAdded);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemLastModified,
                        &node->mLastModified);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Store the index of the node within this container.  Note that this is not
  // moz_bookmarks.position.
  node->mBookmarkIndex = aCurrentIndex;

  NS_ENSURE_TRUE(aChildren->AppendObject(node), NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}


nsresult
nsNavBookmarks::QueryFolderChildrenAsync(
  nsNavHistoryFolderResultNode* aNode,
  PRInt64 aFolderId,
  mozIStoragePendingStatement** _pendingStmt)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(_pendingStmt);

  mozStorageStatementScoper scope(mDBGetChildren);

  nsresult rv = mDBGetChildren->BindInt64ByName(NS_LITERAL_CSTRING("parent"),
                                                aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> pendingStmt;
  rv = mDBGetChildren->ExecuteAsync(aNode, getter_AddRefs(pendingStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*_pendingStmt = pendingStmt);
  return NS_OK;
}


nsresult
nsNavBookmarks::FetchFolderInfo(PRInt64 aFolderId,
                                PRInt32* _folderCount,
                                nsACString& _guid,
                                PRInt64* _parentId)
{
  *_folderCount = 0;
  *_parentId = -1;

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBFolderInfo);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(hasResult, NS_ERROR_UNEXPECTED);

  // Ensure that the folder we are looking for exists.
  // Can't rely only on parent, since the root has parent 0, that doesn't exist.
  bool isNull;
  rv = stmt->GetIsNull(2, &isNull);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && (!isNull || aFolderId == 0),
                 NS_ERROR_INVALID_ARG);

  rv = stmt->GetInt32(0, _folderCount);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isNull) {
    rv = stmt->GetUTF8String(1, _guid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetInt64(2, _parentId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::IsBookmarked(nsIURI* aURI, bool* aBookmarked)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(aBookmarked);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBIsURIBookmarkedInDatabase);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->ExecuteStep(aBookmarked);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetBookmarkedURIFor(nsIURI* aURI, nsIURI** _retval)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = nsnull;

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  PRInt64 placeId;
  nsCAutoString placeGuid;
  nsresult rv = history->GetIdForPage(aURI, &placeId, placeGuid);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!placeId) {
    // This URI is unknown, just return null.
    return NS_OK;
  }

  // Check if a bookmark exists in the redirects chain for this URI.
  // The query will also check if the page is directly bookmarked, and return
  // the first found bookmark in case.  The check is directly on moz_bookmarks
  // without special filtering.
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBFindRedirectedBookmark);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), placeId);
  NS_ENSURE_SUCCESS(rv, rv);
  bool hasBookmarkedOrigin;
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasBookmarkedOrigin)) &&
      hasBookmarkedOrigin) {
    nsCAutoString spec;
    rv = stmt->GetUTF8String(0, spec);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = NS_NewURI(_retval, spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If there is no bookmarked origin, we will just return null.
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::ChangeBookmarkURI(PRInt64 aBookmarkId, nsIURI* aNewURI)
{
  NS_ENSURE_ARG_MIN(aBookmarkId, 1);
  NS_ENSURE_ARG(aNewURI);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aBookmarkId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG(bookmark.type == TYPE_BOOKMARK);

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  PRInt64 newPlaceId;
  nsCAutoString newPlaceGuid;
  rv = history->GetOrCreateIdForPage(aNewURI, &newPlaceId, newPlaceGuid);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!newPlaceId)
    return NS_ERROR_INVALID_ARG;

  BEGIN_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(statement, mDBChangeBookmarkURI);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), newPlaceId);
  NS_ENSURE_SUCCESS(rv, rv);
  bookmark.lastModified = PR_Now();
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("date"),
                                  bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), bookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  END_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  rv = history->UpdateFrecency(newPlaceId);
  NS_ENSURE_SUCCESS(rv, rv);

  // Upon changing the URI for a bookmark, update the frecency for the old
  // place as well.
  rv = history->UpdateFrecency(bookmark.placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  rv = aNewURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(bookmark.id,
                                 NS_LITERAL_CSTRING("uri"),
                                 PR_FALSE,
                                 spec,
                                 bookmark.lastModified,
                                 bookmark.type,
                                 bookmark.parentId,
                                 bookmark.guid,
                                 bookmark.parentGuid));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetFolderIdForItem(PRInt64 aItemId, PRInt64* _parentId)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(_parentId);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  // this should not happen, but see bug #400448 for details
  NS_ENSURE_TRUE(bookmark.id != bookmark.parentId, NS_ERROR_UNEXPECTED);

  *_parentId = bookmark.parentId;
  return NS_OK;
}


nsresult
nsNavBookmarks::GetBookmarkIdsForURITArray(nsIURI* aURI,
                                           nsTArray<PRInt64>& aResult,
                                           bool aSkipTags)
{
  NS_ENSURE_ARG(aURI);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBFindURIBookmarks);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool more;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&more))) && more) {
    if (aSkipTags) {
      // Skip tags, for the use-cases of this async getter they are useless.
      PRInt64 grandParentId;
      nsresult rv = stmt->GetInt64(kFindURIBookmarksIndex_GrandParentId,
                                   &grandParentId);
      NS_ENSURE_SUCCESS(rv, rv);
      if (grandParentId == mTagsRoot) {
        continue;
      }
    }
    PRInt64 bookmarkId;
    rv = stmt->GetInt64(kFindURIBookmarksIndex_Id, &bookmarkId);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(aResult.AppendElement(bookmarkId), NS_ERROR_OUT_OF_MEMORY);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavBookmarks::GetBookmarksForURI(nsIURI* aURI,
                                   nsTArray<BookmarkData>& aBookmarks)
{
  NS_ENSURE_ARG(aURI);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBFindURIBookmarks);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool more;
  nsAutoString tags;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&more))) && more) {
    // Skip tags.
    PRInt64 grandParentId;
    nsresult rv = stmt->GetInt64(kFindURIBookmarksIndex_GrandParentId,
                                 &grandParentId);
    NS_ENSURE_SUCCESS(rv, rv);
    if (grandParentId == mTagsRoot) {
      continue;
    }

    BookmarkData bookmark;
    bookmark.grandParentId = grandParentId;
    rv = stmt->GetInt64(kFindURIBookmarksIndex_Id, &bookmark.id);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetUTF8String(kFindURIBookmarksIndex_Guid, bookmark.guid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetInt64(kFindURIBookmarksIndex_ParentId, &bookmark.parentId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetInt64(kFindURIBookmarksIndex_LastModified,
                        &bookmark.lastModified);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetUTF8String(kFindURIBookmarksIndex_ParentGuid,
                             bookmark.parentGuid);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(aBookmarks.AppendElement(bookmark), NS_ERROR_OUT_OF_MEMORY);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkIdsForURI(nsIURI* aURI, PRUint32* aCount,
                                     PRInt64** aBookmarks)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aBookmarks);

  *aCount = 0;
  *aBookmarks = nsnull;
  nsTArray<PRInt64> bookmarks;

  // Get the information from the DB as a TArray
  // TODO (bug 653816): make this API skip tags by default.
  nsresult rv = GetBookmarkIdsForURITArray(aURI, bookmarks, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the results into a new array for output
  if (bookmarks.Length()) {
    *aBookmarks =
      static_cast<PRInt64*>(nsMemory::Alloc(sizeof(PRInt64) * bookmarks.Length()));
    if (!*aBookmarks)
      return NS_ERROR_OUT_OF_MEMORY;
    for (PRUint32 i = 0; i < bookmarks.Length(); i ++)
      (*aBookmarks)[i] = bookmarks[i];
  }

  *aCount = bookmarks.Length();
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemIndex(PRInt64 aItemId, PRInt32* _index)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(_index);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  // With respect to the API.
  if (NS_FAILED(rv)) {
    *_index = -1;
    return NS_OK;
  }

  *_index = bookmark.position;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemIndex(PRInt64 aItemId, PRInt32 aNewIndex)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_MIN(aNewIndex, 0);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  // Ensure we are not going out of range.
  PRInt32 folderCount;
  PRInt64 grandParentId;
  nsCAutoString folderGuid;
  rv = FetchFolderInfo(bookmark.parentId, &folderCount, folderGuid, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(aNewIndex < folderCount, NS_ERROR_INVALID_ARG);
  // Check the parent's guid is the expected one.
  MOZ_ASSERT(bookmark.parentGuid == folderGuid);

  BEGIN_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBSetItemIndex);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_index"), aNewIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  END_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemMoved(bookmark.id,
                               bookmark.parentId,
                               bookmark.position,
                               bookmark.parentId,
                               aNewIndex,
                               bookmark.type,
                               bookmark.guid,
                               bookmark.parentGuid,
                               bookmark.parentGuid));

  return NS_OK;
}


nsresult
nsNavBookmarks::UpdateKeywordsHashForRemovedBookmark(PRInt64 aItemId)
{
  nsAutoString kw;
  if (NS_SUCCEEDED(GetKeywordForBookmark(aItemId, kw)) && !kw.IsEmpty()) {
    nsresult rv = EnsureKeywordsHash();
    NS_ENSURE_SUCCESS(rv, rv);
    mBookmarkToKeywordHash.Remove(aItemId);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetKeywordForBookmark(PRInt64 aBookmarkId,
                                      const nsAString& aUserCasedKeyword)
{
  NS_ENSURE_ARG_MIN(aBookmarkId, 1);

  // This also ensures the bookmark is valid.
  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aBookmarkId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureKeywordsHash();
  NS_ENSURE_SUCCESS(rv, rv);

  // Shortcuts are always lowercased internally.
  nsAutoString keyword(aUserCasedKeyword);
  ToLowerCase(keyword);

  // Check if bookmark was already associated to a keyword.
  nsAutoString oldKeyword;
  rv = GetKeywordForBookmark(bookmark.id, oldKeyword);
  NS_ENSURE_SUCCESS(rv, rv);

  // Trying to set the same value or to remove a nonexistent keyword is a no-op.
  if (keyword.Equals(oldKeyword) || (keyword.IsEmpty() && oldKeyword.IsEmpty()))
    return NS_OK;

  BEGIN_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  nsCOMPtr<mozIStorageStatement> updateBookmarkStmt;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks "
    "SET keyword_id = (SELECT id FROM moz_keywords WHERE keyword = :keyword), "
        "lastModified = :date "
    "WHERE id = :item_id "
  ), getter_AddRefs(updateBookmarkStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  if (keyword.IsEmpty()) {
    // Remove keyword association from the hash.
    mBookmarkToKeywordHash.Remove(bookmark.id);
    rv = updateBookmarkStmt->BindNullByName(NS_LITERAL_CSTRING("keyword"));
  }
   else {
    // We are associating bookmark to a new keyword. Create a new keyword
    // record if needed.
    nsCOMPtr<mozIStorageStatement> newKeywordStmt;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT OR IGNORE INTO moz_keywords (keyword) VALUES (:keyword)"
    ), getter_AddRefs(newKeywordStmt));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = newKeywordStmt->BindStringByName(NS_LITERAL_CSTRING("keyword"),
                                          keyword);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = newKeywordStmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    // Add new keyword association to the hash, removing the old one if needed.
    if (!oldKeyword.IsEmpty())
      mBookmarkToKeywordHash.Remove(bookmark.id);
    mBookmarkToKeywordHash.Put(bookmark.id, keyword);
    rv = updateBookmarkStmt->BindStringByName(NS_LITERAL_CSTRING("keyword"), keyword);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  bookmark.lastModified = PR_Now();
  rv = updateBookmarkStmt->BindInt64ByName(NS_LITERAL_CSTRING("date"),
                                           bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = updateBookmarkStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"),
                                           bookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = updateBookmarkStmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  END_CRITICAL_BOOKMARK_CACHE_SECTION(bookmark.id);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(bookmark.id,
                                 NS_LITERAL_CSTRING("keyword"),
                                 PR_FALSE,
                                 NS_ConvertUTF16toUTF8(keyword),
                                 bookmark.lastModified,
                                 bookmark.type,
                                 bookmark.parentId,
                                 bookmark.guid,
                                 bookmark.parentGuid));

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetKeywordForURI(nsIURI* aURI, nsAString& aKeyword)
{
  NS_ENSURE_ARG(aURI);
  aKeyword.Truncate(0);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetKeywordForURI);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore = false;
  rv = stmt->ExecuteStep(&hasMore);
  if (NS_FAILED(rv) || !hasMore) {
    aKeyword.SetIsVoid(PR_TRUE);
    return NS_OK; // not found: return void keyword string
  }

  // found, get the keyword
  rv = stmt->GetString(0, aKeyword);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetKeywordForBookmark(PRInt64 aBookmarkId, nsAString& aKeyword)
{
  NS_ENSURE_ARG_MIN(aBookmarkId, 1);
  aKeyword.Truncate(0);

  nsresult rv = EnsureKeywordsHash();
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString keyword;
  if (!mBookmarkToKeywordHash.Get(aBookmarkId, &keyword)) {
    aKeyword.SetIsVoid(PR_TRUE);
  }
  else {
    aKeyword.Assign(keyword);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetURIForKeyword(const nsAString& aUserCasedKeyword,
                                 nsIURI** aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_TRUE(!aUserCasedKeyword.IsEmpty(), NS_ERROR_INVALID_ARG);
  *aURI = nsnull;

  // Shortcuts are always lowercased internally.
  nsAutoString keyword(aUserCasedKeyword);
  ToLowerCase(keyword);

  nsresult rv = EnsureKeywordsHash();
  NS_ENSURE_SUCCESS(rv, rv);

  keywordSearchData searchData;
  searchData.keyword.Assign(keyword);
  searchData.itemId = -1;
  mBookmarkToKeywordHash.EnumerateRead(SearchBookmarkForKeyword, &searchData);

  if (searchData.itemId == -1) {
    // Not found.
    return NS_OK;
  }

  rv = GetBookmarkURI(searchData.itemId, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsNavBookmarks::EnsureKeywordsHash() {
  if (mBookmarkToKeywordHash.IsInitialized())
    return NS_OK;

  mBookmarkToKeywordHash.Init(BOOKMARKS_TO_KEYWORDS_INITIAL_CACHE_SIZE);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT b.id, k.keyword "
    "FROM moz_bookmarks b "
    "JOIN moz_keywords k ON k.id = b.keyword_id "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
    PRInt64 itemId;
    rv = stmt->GetInt64(0, &itemId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoString keyword;
    rv = stmt->GetString(1, keyword);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mBookmarkToKeywordHash.Put(itemId, keyword);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RunInBatchMode(nsINavHistoryBatchCallback* aCallback,
                               nsISupports* aUserData) {
  NS_ENSURE_ARG(aCallback);

  mBatching = true;

  // Just forward the request to history.  History service must exist for
  // bookmarks to work and we are observing it, thus batch notifications will be
  // forwarded to bookmarks observers.
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = history->RunInBatchMode(aCallback, aUserData);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::AddObserver(nsINavBookmarkObserver* aObserver,
                            bool aOwnsWeak)
{
  NS_ENSURE_ARG(aObserver);
  return mObservers.AppendWeakElement(aObserver, aOwnsWeak);
}


NS_IMETHODIMP
nsNavBookmarks::RemoveObserver(nsINavBookmarkObserver* aObserver)
{
  return mObservers.RemoveWeakElement(aObserver);
}

void
nsNavBookmarks::NotifyItemVisited(const ItemVisitData& aData)
{
  nsCOMPtr<nsIURI> uri;
  (void)NS_NewURI(getter_AddRefs(uri), aData.bookmark.url);
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemVisited(aData.bookmark.id,
                                 aData.visitId,
                                 aData.time,
                                 aData.transitionType,
                                 uri,
                                 aData.bookmark.parentId,
                                 aData.bookmark.guid,
                                 aData.bookmark.parentGuid));
}

void
nsNavBookmarks::NotifyItemChanged(const ItemChangeData& aData)
{
  // A guid must always be defined.
  MOZ_ASSERT(!aData.bookmark.guid.IsEmpty());
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(aData.bookmark.id,
                                 aData.property,
                                 aData.isAnnotation,
                                 aData.newValue,
                                 aData.bookmark.lastModified,
                                 aData.bookmark.type,
                                 aData.bookmark.parentId,
                                 aData.bookmark.guid,
                                 aData.bookmark.parentGuid));
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsNavBookmarks::Observe(nsISupports *aSubject, const char *aTopic,
                        const PRUnichar *aData)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  if (strcmp(aTopic, TOPIC_PLACES_MAINTENANCE) == 0) {
    // Maintenance can execute direct writes to the database, thus clear all
    // the cached bookmarks.
    mRecentBookmarksCache.Clear();
  }
  else if (strcmp(aTopic, TOPIC_PLACES_SHUTDOWN) == 0) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      (void)os->RemoveObserver(this, TOPIC_PLACES_MAINTENANCE);
      (void)os->RemoveObserver(this, TOPIC_PLACES_SHUTDOWN);
    }
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsINavHistoryObserver

NS_IMETHODIMP
nsNavBookmarks::OnBeginUpdateBatch()
{
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver, OnBeginUpdateBatch());
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnEndUpdateBatch()
{
  if (mBatching) {
    mBatching = false;
    ForceWALCheckpoint(mDBConn);
  }

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver, OnEndUpdateBatch());
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnVisit(nsIURI* aURI, PRInt64 aVisitId, PRTime aTime,
                        PRInt64 aSessionID, PRInt64 aReferringID,
                        PRUint32 aTransitionType, const nsACString& aGUID,
                        PRUint32* aAdded)
{
  // If the page is bookmarked, notify observers for each associated bookmark.
  ItemVisitData visitData;
  nsresult rv = aURI->GetSpec(visitData.bookmark.url);
  NS_ENSURE_SUCCESS(rv, rv);
  visitData.visitId = aVisitId;
  visitData.time = aTime;
  visitData.transitionType = aTransitionType;

  nsRefPtr< AsyncGetBookmarksForURI<ItemVisitMethod, ItemVisitData> > notifier =
    new AsyncGetBookmarksForURI<ItemVisitMethod, ItemVisitData>(this, &nsNavBookmarks::NotifyItemVisited, visitData);
  notifier->Init();
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnBeforeDeleteURI(nsIURI* aURI,
                                  const nsACString& aGUID,
                                  PRUint16 aReason)
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnDeleteURI(nsIURI* aURI,
                            const nsACString& aGUID,
                            PRUint16 aReason)
{
#ifdef DEBUG
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  PRInt64 placeId;
  nsCAutoString placeGuid;
  NS_ABORT_IF_FALSE(
    history && NS_SUCCEEDED(history->GetIdForPage(aURI, &placeId, placeGuid)) && !placeId,
    "OnDeleteURI was notified for a page that still exists?"
  );
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnClearHistory()
{
  // TODO(bryner): we should notify on visited-time change for all URIs
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnTitleChanged(nsIURI* aURI,
                               const nsAString& aPageTitle,
                               const nsACString& aGUID)
{
  // NOOP. We don't consume page titles from moz_places anymore.
  // Title-change notifications are sent from SetItemTitle.
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnPageChanged(nsIURI* aURI,
                              PRUint32 aChangedAttribute,
                              const nsAString& aNewValue,
                              const nsACString& aGUID)
{
  nsresult rv;
  if (aChangedAttribute == nsINavHistoryObserver::ATTRIBUTE_FAVICON) {
    ItemChangeData changeData;
    rv = aURI->GetSpec(changeData.bookmark.url);
    NS_ENSURE_SUCCESS(rv, rv);
    changeData.property = NS_LITERAL_CSTRING("favicon");
    changeData.isAnnotation = PR_FALSE;
    changeData.newValue = NS_ConvertUTF16toUTF8(aNewValue);
    changeData.bookmark.lastModified = 0;
    changeData.bookmark.type = TYPE_BOOKMARK;

    // Favicons may be set to either pure URIs or to folder URIs
    bool isPlaceURI;
    rv = aURI->SchemeIs("place", &isPlaceURI);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isPlaceURI) {
      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  
      nsCOMArray<nsNavHistoryQuery> queries;
      nsCOMPtr<nsNavHistoryQueryOptions> options;
      rv = history->QueryStringToQueryArray(changeData.bookmark.url,
                                            &queries, getter_AddRefs(options));
      NS_ENSURE_SUCCESS(rv, rv);

      if (queries.Count() == 1 && queries[0]->Folders().Length() == 1) {
        // Fetch missing data.
        rv = FetchItemInfo(queries[0]->Folders()[0], changeData.bookmark);
        NS_ENSURE_SUCCESS(rv, rv);        
        NotifyItemChanged(changeData);
      }
    }
    else {
      nsRefPtr< AsyncGetBookmarksForURI<ItemChangeMethod, ItemChangeData> > notifier =
        new AsyncGetBookmarksForURI<ItemChangeMethod, ItemChangeData>(this, &nsNavBookmarks::NotifyItemChanged, changeData);
      notifier->Init();
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnDeleteVisits(nsIURI* aURI, PRTime aVisitTime,
                               const nsACString& aGUID,
                               PRUint16 aReason)
{
  // Notify "cleartime" only if all visits to the page have been removed.
  if (!aVisitTime) {
    // If the page is bookmarked, notify observers for each associated bookmark.
    ItemChangeData changeData;
    nsresult rv = aURI->GetSpec(changeData.bookmark.url);
    NS_ENSURE_SUCCESS(rv, rv);
    changeData.property = NS_LITERAL_CSTRING("cleartime");
    changeData.isAnnotation = PR_FALSE;
    changeData.bookmark.lastModified = 0;
    changeData.bookmark.type = TYPE_BOOKMARK;

    nsRefPtr< AsyncGetBookmarksForURI<ItemChangeMethod, ItemChangeData> > notifier =
      new AsyncGetBookmarksForURI<ItemChangeMethod, ItemChangeData>(this, &nsNavBookmarks::NotifyItemChanged, changeData);
    notifier->Init();
  }
  return NS_OK;
}


// nsIAnnotationObserver

NS_IMETHODIMP
nsNavBookmarks::OnPageAnnotationSet(nsIURI* aPage, const nsACString& aName)
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnItemAnnotationSet(PRInt64 aItemId, const nsACString& aName)
{
  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  bookmark.lastModified = PR_Now();
  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           bookmark.id, bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(bookmark.id,
                                 aName,
                                 PR_TRUE,
                                 EmptyCString(),
                                 bookmark.lastModified,
                                 bookmark.type,
                                 bookmark.parentId,
                                 bookmark.guid,
                                 bookmark.parentGuid));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnPageAnnotationRemoved(nsIURI* aPage, const nsACString& aName)
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnItemAnnotationRemoved(PRInt64 aItemId, const nsACString& aName)
{
  // As of now this is doing the same as OnItemAnnotationSet, so just forward
  // the call.
  nsresult rv = OnItemAnnotationSet(aItemId, aName);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}
