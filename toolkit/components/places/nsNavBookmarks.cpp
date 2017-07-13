/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNavBookmarks.h"

#include "nsNavHistory.h"
#include "nsAnnotationService.h"
#include "nsPlacesMacros.h"
#include "Helpers.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/Preferences.h"
#include "mozilla/storage.h"

#include "GeckoProfiler.h"

using namespace mozilla;

// These columns sit to the right of the kGetInfoIndex_* columns.
const int32_t nsNavBookmarks::kGetChildrenIndex_Guid = 18;
const int32_t nsNavBookmarks::kGetChildrenIndex_Position = 19;
const int32_t nsNavBookmarks::kGetChildrenIndex_Type = 20;
const int32_t nsNavBookmarks::kGetChildrenIndex_PlaceID = 21;
const int32_t nsNavBookmarks::kGetChildrenIndex_SyncStatus = 22;

using namespace mozilla::places;

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsNavBookmarks, gBookmarksService)

#define BOOKMARKS_ANNO_PREFIX "bookmarks/"
#define BOOKMARKS_TOOLBAR_FOLDER_ANNO NS_LITERAL_CSTRING(BOOKMARKS_ANNO_PREFIX "toolbarFolder")
#define FEED_URI_ANNO NS_LITERAL_CSTRING("livemark/feedURI")
#define SYNC_PARENT_ANNO "sync/parent"
#define SQLITE_MAX_VARIABLE_NUMBER 999


namespace {

#define SKIP_TAGS(condition) ((condition) ? SkipTags : DontSkip)

bool DontSkip(nsCOMPtr<nsINavBookmarkObserver> obs) { return false; }
bool SkipTags(nsCOMPtr<nsINavBookmarkObserver> obs) {
  bool skipTags = false;
  (void) obs->GetSkipTags(&skipTags);
  return skipTags;
}
bool SkipDescendants(nsCOMPtr<nsINavBookmarkObserver> obs) {
  bool skipDescendantsOnItemRemoval = false;
  (void) obs->GetSkipTags(&skipDescendantsOnItemRemoval);
  return skipDescendantsOnItemRemoval;
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
    RefPtr<Database> DB = Database::GetDatabase();
    if (DB) {
      nsCOMPtr<mozIStorageAsyncStatement> stmt = DB->GetAsyncStatement(
        "/* do not warn (bug 1175249) */ "
        "SELECT b.id, b.guid, b.parent, b.lastModified, t.guid, t.parent "
        "FROM moz_bookmarks b "
        "JOIN moz_bookmarks t on t.id = b.parent "
        "WHERE b.fk = (SELECT id FROM moz_places WHERE url_hash = hash(:page_url) AND url = :page_url) "
        "ORDER BY b.lastModified DESC, b.id DESC "
      );
      if (stmt) {
        (void)URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"),
                              mData.bookmark.url);
        nsCOMPtr<mozIStoragePendingStatement> pendingStmt;
        (void)stmt->ExecuteAsync(this, getter_AddRefs(pendingStmt));
      }
    }
  }

  NS_IMETHOD HandleResult(mozIStorageResultSet* aResultSet)
  {
    nsCOMPtr<mozIStorageRow> row;
    while (NS_SUCCEEDED(aResultSet->GetNextRow(getter_AddRefs(row))) && row) {
      // Skip tags, for the use-cases of this async getter they are useless.
      int64_t grandParentId, tagsFolderId;
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
  RefPtr<nsNavBookmarks> mBookmarksSvc;
  Method mCallback;
  DataType mData;
};

// Returns the sync change counter increment for a change source constant.
inline int64_t
DetermineSyncChangeDelta(uint16_t aSource) {
  return aSource == nsINavBookmarksService::SOURCE_SYNC ? 0 : 1;
}

// Returns the sync status for a new item inserted by a change source.
inline int32_t
DetermineInitialSyncStatus(uint16_t aSource) {
  if (aSource == nsINavBookmarksService::SOURCE_SYNC) {
    return nsINavBookmarksService::SYNC_STATUS_NORMAL;
  }
  if (aSource == nsINavBookmarksService::SOURCE_IMPORT_REPLACE) {
    return nsINavBookmarksService::SYNC_STATUS_UNKNOWN;
  }
  return nsINavBookmarksService::SYNC_STATUS_NEW;
}

// Indicates whether an item has been uploaded to the server and
// needs a tombstone on deletion.
inline bool
NeedsTombstone(const BookmarkData& aBookmark) {
  return aBookmark.syncStatus == nsINavBookmarksService::SYNC_STATUS_NORMAL;
}

} // namespace


nsNavBookmarks::nsNavBookmarks()
  : mItemCount(0)
  , mRoot(0)
  , mMenuRoot(0)
  , mTagsRoot(0)
  , mUnfiledRoot(0)
  , mToolbarRoot(0)
  , mMobileRoot(0)
  , mCanNotify(false)
  , mCacheObservers("bookmark-observers")
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
    gBookmarksService = nullptr;
}


NS_IMPL_ISUPPORTS(nsNavBookmarks
, nsINavBookmarksService
, nsINavHistoryObserver
, nsIAnnotationObserver
, nsIObserver
, nsISupportsWeakReference
)


Atomic<int64_t> nsNavBookmarks::sLastInsertedItemId(0);


void // static
nsNavBookmarks::StoreLastInsertedId(const nsACString& aTable,
                                    const int64_t aLastInsertedId) {
  MOZ_ASSERT(aTable.EqualsLiteral("moz_bookmarks"));
  sLastInsertedItemId = aLastInsertedId;
}


nsresult
nsNavBookmarks::Init()
{
  mDB = Database::GetDatabase();
  NS_ENSURE_STATE(mDB);

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    (void)os->AddObserver(this, TOPIC_PLACES_SHUTDOWN, true);
    (void)os->AddObserver(this, TOPIC_PLACES_CONNECTION_CLOSED, true);
  }

  mCanNotify = true;

  // Observe annotations.
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  annosvc->AddObserver(this);

  // Allows us to notify on title changes. MUST BE LAST so it is impossible
  // to fail after this call, or the history service will have a reference to
  // us and we won't go away.
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_STATE(history);
  history->AddObserver(this, true);

  // DO NOT PUT STUFF HERE that can fail. See observer comment above.

  return NS_OK;
}

nsresult
nsNavBookmarks::EnsureRoots()
{
  if (mRoot)
    return NS_OK;

  nsCOMPtr<mozIStorageConnection> conn = mDB->MainConn();
  if (!conn) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = conn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT guid, id FROM moz_bookmarks WHERE guid IN ( "
      "'root________', 'menu________', 'toolbar_____', "
      "'tags________', 'unfiled_____', 'mobile______' )"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsAutoCString guid;
    rv = stmt->GetUTF8String(0, guid);
    NS_ENSURE_SUCCESS(rv, rv);
    int64_t id;
    rv = stmt->GetInt64(1, &id);
    NS_ENSURE_SUCCESS(rv, rv);

    if (guid.EqualsLiteral("root________")) {
      mRoot = id;
    }
    else if (guid.EqualsLiteral("menu________")) {
      mMenuRoot = id;
    }
    else if (guid.EqualsLiteral("toolbar_____")) {
      mToolbarRoot = id;
    }
    else if (guid.EqualsLiteral("tags________")) {
      mTagsRoot = id;
    }
    else if (guid.EqualsLiteral("unfiled_____")) {
      mUnfiledRoot = id;
    }
    else if (guid.EqualsLiteral("mobile______")) {
      mMobileRoot = id;
    }
  }

  if (!mRoot || !mMenuRoot || !mToolbarRoot || !mTagsRoot || !mUnfiledRoot ||
      !mMobileRoot)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

// nsNavBookmarks::IsBookmarkedInDatabase
//
//    This checks to see if the specified place_id is actually bookmarked.

nsresult
nsNavBookmarks::IsBookmarkedInDatabase(int64_t aPlaceId,
                                       bool* aIsBookmarked)
{
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "SELECT 1 FROM moz_bookmarks WHERE fk = :page_id"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlaceId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->ExecuteStep(aIsBookmarked);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


nsresult
nsNavBookmarks::AdjustIndices(int64_t aFolderId,
                              int32_t aStartIndex,
                              int32_t aEndIndex,
                              int32_t aDelta)
{
  NS_ASSERTION(aStartIndex >= 0 && aEndIndex <= INT32_MAX &&
               aStartIndex <= aEndIndex, "Bad indices");

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "UPDATE moz_bookmarks SET position = position + :delta "
      "WHERE parent = :parent "
        "AND position BETWEEN :from_index AND :to_index"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

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


nsresult
nsNavBookmarks::AdjustSeparatorsSyncCounter(int64_t aFolderId,
                                            int32_t aStartIndex,
                                            int64_t aSyncChangeDelta)
{
  MOZ_ASSERT(aStartIndex >= 0, "Bad start position");
  if (!aSyncChangeDelta) {
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "UPDATE moz_bookmarks SET syncChangeCounter = syncChangeCounter + :delta "
      "WHERE parent = :parent AND position >= :start_index "
      "AND type = :item_type "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("delta"), aSyncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("start_index"), aStartIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_type"), TYPE_SEPARATOR);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetPlacesRoot(int64_t* aRoot)
{
  nsresult rv = EnsureRoots();
  NS_ENSURE_SUCCESS(rv, rv);
  *aRoot = mRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetBookmarksMenuFolder(int64_t* aRoot)
{
  nsresult rv = EnsureRoots();
  NS_ENSURE_SUCCESS(rv, rv);
  *aRoot = mMenuRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetToolbarFolder(int64_t* aFolderId)
{
  nsresult rv = EnsureRoots();
  NS_ENSURE_SUCCESS(rv, rv);
  *aFolderId = mToolbarRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetTagsFolder(int64_t* aRoot)
{
  nsresult rv = EnsureRoots();
  NS_ENSURE_SUCCESS(rv, rv);
  *aRoot = mTagsRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetUnfiledBookmarksFolder(int64_t* aRoot)
{
  nsresult rv = EnsureRoots();
  NS_ENSURE_SUCCESS(rv, rv);
  *aRoot = mUnfiledRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetMobileFolder(int64_t* aRoot)
{
  nsresult rv = EnsureRoots();
  NS_ENSURE_SUCCESS(rv, rv);
  *aRoot = mMobileRoot;
  return NS_OK;
}


nsresult
nsNavBookmarks::InsertBookmarkInDB(int64_t aPlaceId,
                                   enum ItemType aItemType,
                                   int64_t aParentId,
                                   int32_t aIndex,
                                   const nsACString& aTitle,
                                   PRTime aDateAdded,
                                   PRTime aLastModified,
                                   const nsACString& aParentGuid,
                                   int64_t aGrandParentId,
                                   nsIURI* aURI,
                                   uint16_t aSource,
                                   int64_t* _itemId,
                                   nsACString& _guid)
{
  // Check for a valid itemId.
  MOZ_ASSERT(_itemId && (*_itemId == -1 || *_itemId > 0));
  // Check for a valid placeId.
  MOZ_ASSERT(aPlaceId && (aPlaceId == -1 || aPlaceId > 0));

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "INSERT INTO moz_bookmarks "
      "(id, fk, type, parent, position, title, "
       "dateAdded, lastModified, guid, syncStatus, syncChangeCounter) "
    "VALUES (:item_id, :page_id, :item_type, :parent, :item_index, "
            ":item_title, :date_added, :last_modified, "
            ":item_guid, :sync_status, :change_counter)"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

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

  if (aTitle.IsEmpty())
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("item_title"));
  else
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("item_title"), aTitle);
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

  // Could use IsEmpty because our callers check for GUID validity,
  // but it doesn't hurt.
  bool hasExistingGuid = _guid.Length() == 12;
  if (hasExistingGuid) {
    MOZ_ASSERT(IsValidGUID(_guid));
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("item_guid"), _guid);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    nsAutoCString guid;
    rv = GenerateGUID(guid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("item_guid"), guid);
    NS_ENSURE_SUCCESS(rv, rv);
    _guid.Assign(guid);
  }

  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("change_counter"),
                             syncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  uint16_t syncStatus = DetermineInitialSyncStatus(aSource);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("sync_status"),
                             syncStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Remove stale tombstones if we're reinserting an item.
  if (hasExistingGuid) {
    rv = RemoveTombstone(_guid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (*_itemId == -1) {
    *_itemId = sLastInsertedItemId;
  }

  if (aParentId > 0) {
    // Update last modified date of the ancestors.
    // TODO (bug 408991): Doing this for all ancestors would be slow without a
    //                    nested tree, so for now update only the parent.
    rv = SetItemDateInternal(LAST_MODIFIED, syncChangeDelta, aParentId,
                             aDateAdded);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isTagging = aGrandParentId == tagsRootId;
  if (isTagging) {
    // If we're tagging a bookmark, increment the change counter for all
    // bookmarks with the URI.
    rv = AddSyncChangesForBookmarksWithURI(aURI, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Mark all affected separators as changed
  rv = AdjustSeparatorsSyncCounter(aParentId, aIndex + 1, syncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add a cache entry since we know everything about this bookmark.
  BookmarkData bookmark;
  bookmark.id = *_itemId;
  bookmark.guid.Assign(_guid);
  if (!aTitle.IsEmpty()) {
    bookmark.title.Assign(aTitle);
  }
  bookmark.position = aIndex;
  bookmark.placeId = aPlaceId;
  bookmark.parentId = aParentId;
  bookmark.type = aItemType;
  bookmark.dateAdded = aDateAdded;
  if (aLastModified)
    bookmark.lastModified = aLastModified;
  else
    bookmark.lastModified = aDateAdded;
  if (aURI) {
    rv = aURI->GetSpec(bookmark.url);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  bookmark.parentGuid = aParentGuid;
  bookmark.grandParentId = aGrandParentId;
  bookmark.syncStatus = syncStatus;

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::InsertBookmark(int64_t aFolder,
                               nsIURI* aURI,
                               int32_t aIndex,
                               const nsACString& aTitle,
                               const nsACString& aGUID,
                               uint16_t aSource,
                               int64_t* aNewBookmarkId)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(aNewBookmarkId);
  NS_ENSURE_ARG_MIN(aIndex, nsINavBookmarksService::DEFAULT_INDEX);

  if (!aGUID.IsEmpty() && !IsValidGUID(aGUID))
    return NS_ERROR_INVALID_ARG;

  mozStorageTransaction transaction(mDB->MainConn(), false);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  int64_t placeId;
  nsAutoCString placeGuid;
  nsresult rv = history->GetOrCreateIdForPage(aURI, &placeId, placeGuid);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the correct index for insertion.  This also ensures the parent exists.
  int32_t index, folderCount;
  int64_t grandParentId;
  nsAutoCString folderGuid;
  rv = FetchFolderInfo(aFolder, &folderCount, folderGuid, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      aIndex >= folderCount) {
    index = folderCount;
  }
  else {
    index = aIndex;
    // Create space for the insertion.
    rv = AdjustIndices(aFolder, index, INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aNewBookmarkId = -1;
  PRTime dateAdded = RoundedPRNow();
  nsAutoCString guid(aGUID);
  nsCString title;
  TruncateTitle(aTitle, title);

  rv = InsertBookmarkInDB(placeId, BOOKMARK, aFolder, index, title, dateAdded,
                          0, folderGuid, grandParentId, aURI, aSource,
                          aNewBookmarkId, guid);
  NS_ENSURE_SUCCESS(rv, rv);

  // If not a tag, recalculate frecency for this entry, since it changed.
  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);
  if (grandParentId != tagsRootId) {
    rv = history->UpdateFrecency(placeId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_BOOKMARKS_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                             SKIP_TAGS(grandParentId == mTagsRoot),
                             OnItemAdded(*aNewBookmarkId, aFolder, index,
                                         TYPE_BOOKMARK, aURI, title, dateAdded,
                                         guid, folderGuid, aSource));

  // If the bookmark has been added to a tag container, notify all
  // bookmark-folder result nodes which contain a bookmark for the new
  // bookmark's url.
  if (grandParentId == tagsRootId) {
    // Notify a tags change to all bookmarks for this URI.
    nsTArray<BookmarkData> bookmarks;
    rv = GetBookmarksForURI(aURI, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    for (uint32_t i = 0; i < bookmarks.Length(); ++i) {
      // Check that bookmarks doesn't include the current tag itemId.
      MOZ_ASSERT(bookmarks[i].id != *aNewBookmarkId);

      NOTIFY_BOOKMARKS_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                                 DontSkip,
                                 OnItemChanged(bookmarks[i].id,
                                               NS_LITERAL_CSTRING("tags"),
                                               false,
                                               EmptyCString(),
                                               bookmarks[i].lastModified,
                                               TYPE_BOOKMARK,
                                               bookmarks[i].parentId,
                                               bookmarks[i].guid,
                                               bookmarks[i].parentGuid,
                                               EmptyCString(),
                                               aSource));
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RemoveItem(int64_t aItemId, uint16_t aSource)
{
  AUTO_PROFILER_LABEL("nsNavBookmarks::RemoveItem", OTHER);

  NS_ENSURE_ARG(!IsRoot(aItemId));

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mDB->MainConn(), false);

  // First, if not a tag, remove item annotations.
  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isUntagging = bookmark.grandParentId == tagsRootId;
  if (bookmark.parentId != tagsRootId && !isUntagging) {
    nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
    NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
    rv = annosvc->RemoveItemAnnotations(bookmark.id, aSource);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (bookmark.type == TYPE_FOLDER) {
    // Remove all of the folder's children.
    rv = RemoveFolderChildren(bookmark.id, aSource);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "DELETE FROM moz_bookmarks WHERE id = :item_id"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), bookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Fix indices in the parent.
  if (bookmark.position != DEFAULT_INDEX) {
    rv = AdjustIndices(bookmark.parentId,
                       bookmark.position + 1, INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  // Add a tombstone for synced items.
  if (syncChangeDelta) {
    rv = InsertTombstone(bookmark);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bookmark.lastModified = RoundedPRNow();
  rv = SetItemDateInternal(LAST_MODIFIED, syncChangeDelta,
                           bookmark.parentId, bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  // Mark all affected separators as changed
  rv = AdjustSeparatorsSyncCounter(bookmark.parentId, bookmark.position, syncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isUntagging) {
    // If we're removing a tag, increment the change counter for all bookmarks
    // with the URI.
    rv = AddSyncChangesForBookmarksWithURL(bookmark.url, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  if (bookmark.type == TYPE_BOOKMARK) {
    // If not a tag, recalculate frecency for this entry, since it changed.
    if (bookmark.grandParentId != tagsRootId) {
      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
      rv = history->UpdateFrecency(bookmark.placeId);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // A broken url should not interrupt the removal process.
    (void)NS_NewURI(getter_AddRefs(uri), bookmark.url);
    // We cannot assert since some automated tests are checking this path.
    NS_WARNING_ASSERTION(uri, "Invalid URI in RemoveItem");
  }

  NOTIFY_BOOKMARKS_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                             SKIP_TAGS(bookmark.parentId == tagsRootId ||
                                       bookmark.grandParentId == tagsRootId),
                             OnItemRemoved(bookmark.id,
                                           bookmark.parentId,
                                           bookmark.position,
                                           bookmark.type,
                                           uri,
                                           bookmark.guid,
                                           bookmark.parentGuid,
                                           aSource));

  if (bookmark.type == TYPE_BOOKMARK && bookmark.grandParentId == tagsRootId &&
      uri) {
    // If the removed bookmark was child of a tag container, notify a tags
    // change to all bookmarks for this URI.
    nsTArray<BookmarkData> bookmarks;
    rv = GetBookmarksForURI(uri, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    for (uint32_t i = 0; i < bookmarks.Length(); ++i) {
      NOTIFY_BOOKMARKS_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                                 DontSkip,
                                 OnItemChanged(bookmarks[i].id,
                                               NS_LITERAL_CSTRING("tags"),
                                               false,
                                               EmptyCString(),
                                               bookmarks[i].lastModified,
                                               TYPE_BOOKMARK,
                                               bookmarks[i].parentId,
                                               bookmarks[i].guid,
                                               bookmarks[i].parentGuid,
                                               EmptyCString(),
                                               aSource));
    }

  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::CreateFolder(int64_t aParent, const nsACString& aName,
                             int32_t aIndex, const nsACString& aGUID,
                             uint16_t aSource, int64_t* aNewFolder)
{
  // NOTE: aParent can be null for root creation, so not checked
  NS_ENSURE_ARG_POINTER(aNewFolder);

  if (!aGUID.IsEmpty() && !IsValidGUID(aGUID))
    return NS_ERROR_INVALID_ARG;

  // CreateContainerWithID returns the index of the new folder, but that's not
  // used here.  To avoid any risk of corrupting data should this function
  // be changed, we'll use a local variable to hold it.  The true argument
  // will cause notifications to be sent to bookmark observers.
  int32_t localIndex = aIndex;
  nsresult rv = CreateContainerWithID(-1, aParent, aName, true, &localIndex,
                                      aGUID, aSource, aNewFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

bool nsNavBookmarks::IsLivemark(int64_t aFolderId)
{
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, false);
  bool isLivemark;
  nsresult rv = annosvc->ItemHasAnnotation(aFolderId,
                                           FEED_URI_ANNO,
                                           &isLivemark);
  NS_ENSURE_SUCCESS(rv, false);
  return isLivemark;
}

nsresult
nsNavBookmarks::CreateContainerWithID(int64_t aItemId,
                                      int64_t aParent,
                                      const nsACString& aTitle,
                                      bool aIsBookmarkFolder,
                                      int32_t* aIndex,
                                      const nsACString& aGUID,
                                      uint16_t aSource,
                                      int64_t* aNewFolder)
{
  NS_ENSURE_ARG_MIN(*aIndex, nsINavBookmarksService::DEFAULT_INDEX);

  // Get the correct index for insertion.  This also ensures the parent exists.
  int32_t index, folderCount;
  int64_t grandParentId;
  nsAutoCString folderGuid;
  nsresult rv = FetchFolderInfo(aParent, &folderCount, folderGuid, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mDB->MainConn(), false);

  if (*aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      *aIndex >= folderCount) {
    index = folderCount;
  } else {
    index = *aIndex;
    // Create space for the insertion.
    rv = AdjustIndices(aParent, index, INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aNewFolder = aItemId;
  PRTime dateAdded = RoundedPRNow();
  nsAutoCString guid(aGUID);
  nsCString title;
  TruncateTitle(aTitle, title);

  rv = InsertBookmarkInDB(-1, FOLDER, aParent, index,
                          title, dateAdded, 0, folderGuid, grandParentId,
                          nullptr, aSource, aNewFolder, guid);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_BOOKMARKS_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                             SKIP_TAGS(aParent == tagsRootId),
                             OnItemAdded(*aNewFolder, aParent, index, FOLDER,
                                         nullptr, title, dateAdded, guid,
                                         folderGuid, aSource));

  *aIndex = index;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::InsertSeparator(int64_t aParent,
                                int32_t aIndex,
                                const nsACString& aGUID,
                                uint16_t aSource,
                                int64_t* aNewItemId)
{
  NS_ENSURE_ARG_MIN(aParent, 1);
  NS_ENSURE_ARG_MIN(aIndex, nsINavBookmarksService::DEFAULT_INDEX);
  NS_ENSURE_ARG_POINTER(aNewItemId);

  if (!aGUID.IsEmpty() && !IsValidGUID(aGUID))
    return NS_ERROR_INVALID_ARG;

  // Get the correct index for insertion.  This also ensures the parent exists.
  int32_t index, folderCount;
  int64_t grandParentId;
  nsAutoCString folderGuid;
  nsresult rv = FetchFolderInfo(aParent, &folderCount, folderGuid, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mDB->MainConn(), false);

  if (aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      aIndex >= folderCount) {
    index = folderCount;
  }
  else {
    index = aIndex;
    // Create space for the insertion.
    rv = AdjustIndices(aParent, index, INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aNewItemId = -1;
  nsAutoCString guid(aGUID);
  PRTime dateAdded = RoundedPRNow();
  rv = InsertBookmarkInDB(-1, SEPARATOR, aParent, index, EmptyCString(), dateAdded,
                          0, folderGuid, grandParentId, nullptr, aSource,
                          aNewItemId, guid);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_BOOKMARKS_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                             DontSkip,
                             OnItemAdded(*aNewItemId, aParent, index, TYPE_SEPARATOR,
                                         nullptr, EmptyCString(), dateAdded, guid,
                                         folderGuid, aSource));

  return NS_OK;
}


nsresult
nsNavBookmarks::GetLastChildId(int64_t aFolderId, int64_t* aItemId)
{
  NS_ASSERTION(aFolderId > 0, "Invalid folder id");
  *aItemId = -1;

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "SELECT id FROM moz_bookmarks WHERE parent = :parent "
    "ORDER BY position DESC LIMIT 1"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

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
nsNavBookmarks::GetIdForItemAt(int64_t aFolder,
                               int32_t aIndex,
                               int64_t* aItemId)
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
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT id, fk, type FROM moz_bookmarks "
      "WHERE parent = :parent AND position = :item_index"
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

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

NS_IMPL_ISUPPORTS(nsNavBookmarks::RemoveFolderTransaction, nsITransaction)

NS_IMETHODIMP
nsNavBookmarks::GetRemoveFolderTransaction(int64_t aFolderId, uint16_t aSource,
                                           nsITransaction** aResult)
{
  NS_ENSURE_ARG_MIN(aFolderId, 1);
  NS_ENSURE_ARG_POINTER(aResult);

  // Create and initialize a RemoveFolderTransaction object that can be used to
  // recreate the folder safely later.

  RemoveFolderTransaction* rft =
    new RemoveFolderTransaction(aFolderId, aSource);
  if (!rft)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult = rft);
  return NS_OK;
}


nsresult
nsNavBookmarks::GetDescendantFolders(int64_t aFolderId,
                                     nsTArray<int64_t>& aDescendantFoldersArray) {
  nsresult rv;
  // New descendant folders will be added from this index on.
  uint32_t startIndex = aDescendantFoldersArray.Length();
  {
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT id "
      "FROM moz_bookmarks "
      "WHERE parent = :parent "
      "AND type = :item_type "
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_type"), TYPE_FOLDER);
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasMore = false;
    while (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
      int64_t itemId;
      rv = stmt->GetInt64(0, &itemId);
      NS_ENSURE_SUCCESS(rv, rv);
      aDescendantFoldersArray.AppendElement(itemId);
    }
  }

  // Recursively call GetDescendantFolders for added folders.
  // We start at startIndex since previous folders are checked
  // by previous calls to this method.
  uint32_t childCount = aDescendantFoldersArray.Length();
  for (uint32_t i = startIndex; i < childCount; ++i) {
    GetDescendantFolders(aDescendantFoldersArray[i], aDescendantFoldersArray);
  }

  return NS_OK;
}


nsresult
nsNavBookmarks::GetDescendantChildren(int64_t aFolderId,
                                      const nsACString& aFolderGuid,
                                      int64_t aGrandParentId,
                                      nsTArray<BookmarkData>& aFolderChildrenArray) {
  // New children will be added from this index on.
  uint32_t startIndex = aFolderChildrenArray.Length();
  nsresult rv;
  {
    // Collect children informations.
    // Select all children of a given folder, sorted by position.
    // This is a LEFT JOIN because not all bookmarks types have a place.
    // We construct a result where the first columns exactly match
    // kGetInfoIndex_* order, and additionally contains columns for position,
    // item_child, and folder_child from moz_bookmarks.
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT h.id, h.url, b.title, h.rev_host, h.visit_count, "
             "h.last_visit_date, null, b.id, b.dateAdded, b.lastModified, "
             "b.parent, null, h.frecency, h.hidden, h.guid, null, null, null, "
             "b.guid, b.position, b.type, b.fk, b.syncStatus "
      "FROM moz_bookmarks b "
      "LEFT JOIN moz_places h ON b.fk = h.id "
      "WHERE b.parent = :parent "
      "ORDER BY b.position ASC"
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

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
      rv = stmt->GetInt32(kGetChildrenIndex_SyncStatus, &child.syncStatus);
      NS_ENSURE_SUCCESS(rv, rv);

      if (child.type == TYPE_BOOKMARK) {
        rv = stmt->GetUTF8String(nsNavHistory::kGetInfoIndex_URL, child.url);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Append item to children's array.
      aFolderChildrenArray.AppendElement(child);
    }
  }

  // Recursively call GetDescendantChildren for added folders.
  // We start at startIndex since previous folders are checked
  // by previous calls to this method.
  uint32_t childCount = aFolderChildrenArray.Length();
  for (uint32_t i = startIndex; i < childCount; ++i) {
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
nsNavBookmarks::RemoveFolderChildren(int64_t aFolderId, uint16_t aSource)
{
  AUTO_PROFILER_LABEL("nsNavBookmarks::RemoveFolderChilder", OTHER);

  NS_ENSURE_ARG_MIN(aFolderId, 1);
  int64_t rootId;
  nsresult rv = GetPlacesRoot(&rootId);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG(aFolderId != rootId);

  BookmarkData folder;
  rv = FetchItemInfo(aFolderId, folder);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG(folder.type == TYPE_FOLDER);

  // Fill folder children array recursively.
  nsTArray<BookmarkData> folderChildrenArray;
  rv = GetDescendantChildren(folder.id, folder.guid, folder.parentId,
                             folderChildrenArray);
  NS_ENSURE_SUCCESS(rv, rv);

  // Build a string of folders whose children will be removed.
  nsCString foldersToRemove;
  for (uint32_t i = 0; i < folderChildrenArray.Length(); ++i) {
    BookmarkData& child = folderChildrenArray[i];

    if (child.type == TYPE_FOLDER) {
      foldersToRemove.Append(',');
      foldersToRemove.AppendInt(child.id);
    }
  }

  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  // Delete items from the database now.
  mozStorageTransaction transaction(mDB->MainConn(), false);

  nsCOMPtr<mozIStorageStatement> deleteStatement = mDB->GetStatement(
    NS_LITERAL_CSTRING(
      "DELETE FROM moz_bookmarks "
      "WHERE parent IN (:parent") + foldersToRemove + NS_LITERAL_CSTRING(")")
  );
  NS_ENSURE_STATE(deleteStatement);
  mozStorageStatementScoper deleteStatementScoper(deleteStatement);

  rv = deleteStatement->BindInt64ByName(NS_LITERAL_CSTRING("parent"), folder.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Clean up orphan items annotations.
  nsCOMPtr<mozIStorageConnection> conn = mDB->MainConn();
  if (!conn) {
    return NS_ERROR_UNEXPECTED;
  }
  rv = conn->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING(
      "DELETE FROM moz_items_annos "
      "WHERE id IN ("
        "SELECT a.id from moz_items_annos a "
        "LEFT JOIN moz_bookmarks b ON a.item_id = b.id "
        "WHERE b.id ISNULL)"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the lastModified date.
  rv = SetItemDateInternal(LAST_MODIFIED, syncChangeDelta, folder.id,
                           RoundedPRNow());
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);

  if (syncChangeDelta) {
    nsTArray<TombstoneData> tombstones(folderChildrenArray.Length());
    PRTime dateRemoved = RoundedPRNow();

    for (uint32_t i = 0; i < folderChildrenArray.Length(); ++i) {
      BookmarkData& child = folderChildrenArray[i];
      if (NeedsTombstone(child)) {
        // Write tombstones for synced children.
        TombstoneData childTombstone = {child.guid, dateRemoved};
        tombstones.AppendElement(childTombstone);
      }
      bool isUntagging = child.grandParentId == tagsRootId;
      if (isUntagging) {
        // Bump the change counter for all tagged bookmarks when removing a tag
        // folder.
        rv = AddSyncChangesForBookmarksWithURL(child.url, syncChangeDelta);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    rv = InsertTombstones(tombstones);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // Call observers in reverse order to serve children before their parent.
  for (int32_t i = folderChildrenArray.Length() - 1; i >= 0; --i) {
    BookmarkData& child = folderChildrenArray[i];

    nsCOMPtr<nsIURI> uri;
    if (child.type == TYPE_BOOKMARK) {
      // If not a tag, recalculate frecency for this entry, since it changed.
      if (child.grandParentId != tagsRootId) {
        nsNavHistory* history = nsNavHistory::GetHistoryService();
        NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
        rv = history->UpdateFrecency(child.placeId);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // A broken url should not interrupt the removal process.
      (void)NS_NewURI(getter_AddRefs(uri), child.url);
      // We cannot assert since some automated tests are checking this path.
      NS_WARNING_ASSERTION(uri, "Invalid URI in RemoveFolderChildren");
    }

    NOTIFY_BOOKMARKS_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                               ((child.grandParentId == tagsRootId) ? SkipTags : SkipDescendants),
                               OnItemRemoved(child.id,
                                             child.parentId,
                                             child.position,
                                             child.type,
                                             uri,
                                             child.guid,
                                             child.parentGuid,
                                             aSource));

    if (child.type == TYPE_BOOKMARK && child.grandParentId == tagsRootId &&
        uri) {
      // If the removed bookmark was a child of a tag container, notify all
      // bookmark-folder result nodes which contain a bookmark for the removed
      // bookmark's url.
      nsTArray<BookmarkData> bookmarks;
      rv = GetBookmarksForURI(uri, bookmarks);
      NS_ENSURE_SUCCESS(rv, rv);

      for (uint32_t i = 0; i < bookmarks.Length(); ++i) {
        NOTIFY_BOOKMARKS_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                                   DontSkip,
                                   OnItemChanged(bookmarks[i].id,
                                                 NS_LITERAL_CSTRING("tags"),
                                                 false,
                                                 EmptyCString(),
                                                 bookmarks[i].lastModified,
                                                 TYPE_BOOKMARK,
                                                 bookmarks[i].parentId,
                                                 bookmarks[i].guid,
                                                 bookmarks[i].parentGuid,
                                                 EmptyCString(),
                                                 aSource));
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::MoveItem(int64_t aItemId,
                         int64_t aNewParent,
                         int32_t aIndex,
                         uint16_t aSource)
{
  NS_ENSURE_ARG(!IsRoot(aItemId));
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_MIN(aNewParent, 1);
  // -1 is append, but no other negative number is allowed.
  NS_ENSURE_ARG_MIN(aIndex, -1);
  // Disallow making an item its own parent.
  NS_ENSURE_ARG(aItemId != aNewParent);

  mozStorageTransaction transaction(mDB->MainConn(), false);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  // if parent and index are the same, nothing to do
  if (bookmark.parentId == aNewParent && bookmark.position == aIndex)
    return NS_OK;

  // Make sure aNewParent is not aFolder or a subfolder of aFolder.
  // TODO: make this performant, maybe with a nested tree (bug 408991).
  if (bookmark.type == TYPE_FOLDER) {
    int64_t ancestorId = aNewParent;

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
  int32_t newIndex, folderCount;
  int64_t grandParentId;
  nsAutoCString newParentGuid;
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
  bool sameParent = bookmark.parentId == aNewParent;
  if (sameParent) {
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
    rv = AdjustIndices(bookmark.parentId, bookmark.position + 1, INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
    // Now, make room in the new parent for the insertion.
    rv = AdjustIndices(aNewParent, newIndex, INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  {
    // Update parent and position.
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "UPDATE moz_bookmarks SET parent = :parent, position = :item_index "
      "WHERE id = :item_id "
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aNewParent);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_index"), newIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), bookmark.id);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRTime now = RoundedPRNow();
  rv = SetItemDateInternal(LAST_MODIFIED, syncChangeDelta,
                           bookmark.parentId, now);
  NS_ENSURE_SUCCESS(rv, rv);
  if (sameParent) {
    // If we're moving within the same container, only the parent needs a sync
    // change. Update the item's last modified date without bumping its counter.
    rv = SetItemDateInternal(LAST_MODIFIED, 0, bookmark.id, now);
    NS_ENSURE_SUCCESS(rv, rv);

    // Mark all affected separators as changed
    int32_t startIndex = std::min(bookmark.position, newIndex);
    rv = AdjustSeparatorsSyncCounter(bookmark.parentId, startIndex, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // Otherwise, if we're moving between containers, both parents and the child
    // need sync changes.
    rv = SetItemDateInternal(LAST_MODIFIED, syncChangeDelta, aNewParent, now);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = SetItemDateInternal(LAST_MODIFIED, syncChangeDelta, bookmark.id, now);
    NS_ENSURE_SUCCESS(rv, rv);

    // Mark all affected separators as changed
    rv = AdjustSeparatorsSyncCounter(bookmark.parentId, bookmark.position, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AdjustSeparatorsSyncCounter(aNewParent, newIndex, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isChangingTagFolder = bookmark.parentId == tagsRootId;
  if (isChangingTagFolder) {
    // Moving a tag folder out of the tags root untags all its bookmarks. This
    // is an odd case, but the tagging service adds an observer to handle it,
    // so we bump the change counter for each untagged item for consistency.
    rv = AddSyncChangesForBookmarksInFolder(bookmark.id, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = PreventSyncReparenting(bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

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
                               newParentGuid,
                               aSource));
  return NS_OK;
}

nsresult
nsNavBookmarks::FetchItemInfo(int64_t aItemId,
                              BookmarkData& _bookmark)
{
  // LEFT JOIN since not all bookmarks have an associated place.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "SELECT b.id, h.url, b.title, b.position, b.fk, b.parent, b.type, "
           "b.dateAdded, b.lastModified, b.guid, t.guid, t.parent, "
           "b.syncStatus "
    "FROM moz_bookmarks b "
    "LEFT JOIN moz_bookmarks t ON t.id = b.parent "
    "LEFT JOIN moz_places h ON h.id = b.fk "
    "WHERE b.id = :item_id"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasResult) {
    return NS_ERROR_INVALID_ARG;
  }

  _bookmark.id = aItemId;
  rv = stmt->GetUTF8String(1, _bookmark.url);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isNull;
  rv = stmt->GetIsNull(2, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isNull) {
    rv = stmt->GetUTF8String(2, _bookmark.title);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = stmt->GetInt32(3, &_bookmark.position);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt64(4, &_bookmark.placeId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt64(5, &_bookmark.parentId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt32(6, &_bookmark.type);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt64(7, reinterpret_cast<int64_t*>(&_bookmark.dateAdded));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt64(8, reinterpret_cast<int64_t*>(&_bookmark.lastModified));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetUTF8String(9, _bookmark.guid);
  NS_ENSURE_SUCCESS(rv, rv);
  // Getting properties of the root would show no parent.
  rv = stmt->GetIsNull(10, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isNull) {
    rv = stmt->GetUTF8String(10, _bookmark.parentGuid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetInt64(11, &_bookmark.grandParentId);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    _bookmark.grandParentId = -1;
  }
  rv = stmt->GetInt32(12, &_bookmark.syncStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavBookmarks::SetItemDateInternal(enum BookmarkDate aDateType,
                                    int64_t aSyncChangeDelta,
                                    int64_t aItemId,
                                    PRTime aValue)
{
  aValue = RoundToMilliseconds(aValue);

  nsCOMPtr<mozIStorageStatement> stmt;
  if (aDateType == DATE_ADDED) {
    // lastModified is set to the same value as dateAdded.  We do this for
    // performance reasons, since it will allow us to use an index to sort items
    // by date.
    stmt = mDB->GetStatement(
      "UPDATE moz_bookmarks SET dateAdded = :date, lastModified = :date, "
       "syncChangeCounter = syncChangeCounter + :delta "
      "WHERE id = :item_id"
    );
  }
  else {
    stmt = mDB->GetStatement(
      "UPDATE moz_bookmarks SET lastModified = :date, "
       "syncChangeCounter = syncChangeCounter + :delta "
      "WHERE id = :item_id"
    );
  }
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("date"), aValue);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("delta"), aSyncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // note, we are not notifying the observers
  // that the item has changed.

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemDateAdded(int64_t aItemId, PRTime aDateAdded,
                                 uint16_t aSource)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);
  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isTagging = bookmark.grandParentId == tagsRootId;
  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  // Round here so that we notify with the right value.
  bookmark.dateAdded = RoundToMilliseconds(aDateAdded);

  if (isTagging) {
    // If we're changing a tag, bump the change counter for all tagged
    // bookmarks. We use a separate code path to avoid a transaction for
    // non-tags.
    mozStorageTransaction transaction(mDB->MainConn(), false);

    rv = SetItemDateInternal(DATE_ADDED, syncChangeDelta, bookmark.id,
                             bookmark.dateAdded);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AddSyncChangesForBookmarksWithURL(bookmark.url, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = SetItemDateInternal(DATE_ADDED, syncChangeDelta, bookmark.id,
                             bookmark.dateAdded);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Note: mDBSetItemDateAdded also sets lastModified to aDateAdded.
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(bookmark.id,
                                 NS_LITERAL_CSTRING("dateAdded"),
                                 false,
                                 nsPrintfCString("%" PRId64, bookmark.dateAdded),
                                 bookmark.dateAdded,
                                 bookmark.type,
                                 bookmark.parentId,
                                 bookmark.guid,
                                 bookmark.parentGuid,
                                 EmptyCString(),
                                 aSource));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemDateAdded(int64_t aItemId, PRTime* _dateAdded)
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
nsNavBookmarks::SetItemLastModified(int64_t aItemId, PRTime aLastModified,
                                    uint16_t aSource)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isTagging = bookmark.grandParentId == tagsRootId;
  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  // Round here so that we notify with the right value.
  bookmark.lastModified = RoundToMilliseconds(aLastModified);

  if (isTagging) {
    // If we're changing a tag, bump the change counter for all tagged
    // bookmarks. We use a separate code path to avoid a transaction for
    // non-tags.
    mozStorageTransaction transaction(mDB->MainConn(), false);

    rv = SetItemDateInternal(LAST_MODIFIED, syncChangeDelta, bookmark.id,
                             bookmark.lastModified);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AddSyncChangesForBookmarksWithURL(bookmark.url, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = SetItemDateInternal(LAST_MODIFIED, syncChangeDelta, bookmark.id,
                             bookmark.lastModified);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Note: mDBSetItemDateAdded also sets lastModified to aDateAdded.
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(bookmark.id,
                                 NS_LITERAL_CSTRING("lastModified"),
                                 false,
                                 nsPrintfCString("%" PRId64, bookmark.lastModified),
                                 bookmark.lastModified,
                                 bookmark.type,
                                 bookmark.parentId,
                                 bookmark.guid,
                                 bookmark.parentGuid,
                                 EmptyCString(),
                                 aSource));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemLastModified(int64_t aItemId, PRTime* _lastModified)
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
nsNavBookmarks::AddSyncChangesForBookmarksWithURL(const nsACString& aURL,
                                                  int64_t aSyncChangeDelta)
{
  if (!aSyncChangeDelta) {
    return NS_OK;
  }
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Ignore sync changes for invalid URLs.
    return NS_OK;
  }
  return AddSyncChangesForBookmarksWithURI(uri, aSyncChangeDelta);
}


nsresult
nsNavBookmarks::AddSyncChangesForBookmarksWithURI(nsIURI* aURI,
                                                  int64_t aSyncChangeDelta)
{
  if (NS_WARN_IF(!aURI) || !aSyncChangeDelta) {
    // Ignore sync changes for invalid URIs.
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> statement = mDB->GetStatement(
   "UPDATE moz_bookmarks SET "
    "syncChangeCounter = syncChangeCounter + :delta "
   "WHERE type = :type AND "
         "fk = (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND "
               "url = :url)"
  );
  NS_ENSURE_STATE(statement);
  mozStorageStatementScoper scoper(statement);

  nsresult rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("delta"),
                                           aSyncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("type"),
                                  nsINavBookmarksService::TYPE_BOOKMARK);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = URIBinder::Bind(statement, NS_LITERAL_CSTRING("url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return statement->Execute();
}


nsresult
nsNavBookmarks::AddSyncChangesForBookmarksInFolder(int64_t aFolderId,
                                                   int64_t aSyncChangeDelta)
{
  if (!aSyncChangeDelta) {
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> statement = mDB->GetStatement(
   "UPDATE moz_bookmarks SET "
    "syncChangeCounter = syncChangeCounter + :delta "
    "WHERE type = :type AND "
          "fk = (SELECT fk FROM moz_bookmarks WHERE parent = :parent)"
  );
  NS_ENSURE_STATE(statement);
  mozStorageStatementScoper scoper(statement);

  nsresult rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("delta"),
                                           aSyncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("type"),
                                  nsINavBookmarksService::TYPE_BOOKMARK);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsNavBookmarks::InsertTombstone(const BookmarkData& aBookmark)
{
  if (!NeedsTombstone(aBookmark)) {
    return NS_OK;
  }
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "INSERT INTO moz_bookmarks_deleted (guid, dateRemoved) "
    "VALUES (:guid, :date_removed)"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"),
                                           aBookmark.guid);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("date_removed"),
                             RoundedPRNow());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsNavBookmarks::InsertTombstones(const nsTArray<TombstoneData>& aTombstones)
{
  if (aTombstones.IsEmpty()) {
    return NS_OK;
  }

  size_t maxRowsPerChunk = SQLITE_MAX_VARIABLE_NUMBER / 2;
  for (uint32_t startIndex = 0; startIndex < aTombstones.Length(); startIndex += maxRowsPerChunk) {
    size_t rowsPerChunk = std::min(maxRowsPerChunk, aTombstones.Length() - startIndex);

    // Build a query to insert all tombstones in a single statement, chunking to
    // avoid the SQLite bound parameter limit.
    nsAutoCString tombstonesToInsert;
    tombstonesToInsert.AppendLiteral("VALUES (?, ?)");
    for (uint32_t i = 1; i < rowsPerChunk; ++i) {
      tombstonesToInsert.AppendLiteral(", (?, ?)");
    }
#ifdef DEBUG
    MOZ_ASSERT(tombstonesToInsert.CountChar('?') == rowsPerChunk * 2,
               "Expected one binding param per column for each tombstone");
#endif

    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_deleted "
        "(guid, dateRemoved) ") +
      tombstonesToInsert
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    uint32_t paramIndex = 0;
    nsresult rv;
    for (uint32_t i = 0; i < rowsPerChunk; ++i) {
      const TombstoneData& tombstone = aTombstones[startIndex + i];
      rv = stmt->BindUTF8StringByIndex(paramIndex++, tombstone.guid);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt64ByIndex(paramIndex++, tombstone.dateRemoved);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


nsresult
nsNavBookmarks::RemoveTombstone(const nsACString& aGUID)
{
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "DELETE FROM moz_bookmarks_deleted WHERE guid = :guid"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), aGUID);
  NS_ENSURE_SUCCESS(rv, rv);

  return stmt->Execute();
}


nsresult
nsNavBookmarks::PreventSyncReparenting(const BookmarkData& aBookmark)
{
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "DELETE FROM moz_items_annos WHERE "
      "item_id = :item_id AND "
      "anno_attribute_id = (SELECT id FROM moz_anno_attributes "
                           "WHERE name = :orphan_anno)"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aBookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("orphan_anno"),
                                  NS_LITERAL_CSTRING(SYNC_PARENT_ANNO));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemTitle(int64_t aItemId, const nsACString& aTitle,
                             uint16_t aSource)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isChangingTagFolder = bookmark.parentId == tagsRootId;
  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  nsAutoCString title;
  TruncateTitle(aTitle, title);

  if (isChangingTagFolder) {
    // If we're changing the title of a tag folder, bump the change counter
    // for all tagged bookmarks. We use a separate code path to avoid a
    // transaction for non-tags.
    mozStorageTransaction transaction(mDB->MainConn(), false);

    rv = SetItemTitleInternal(bookmark, title, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = AddSyncChangesForBookmarksInFolder(bookmark.id, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = SetItemTitleInternal(bookmark, title, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NOTIFY_BOOKMARKS_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                             SKIP_TAGS(isChangingTagFolder),
                             OnItemChanged(bookmark.id,
                                           NS_LITERAL_CSTRING("title"),
                                           false,
                                           title,
                                           bookmark.lastModified,
                                           bookmark.type,
                                           bookmark.parentId,
                                           bookmark.guid,
                                           bookmark.parentGuid,
                                           EmptyCString(),
                                           aSource));
  return NS_OK;
}


nsresult
nsNavBookmarks::SetItemTitleInternal(BookmarkData& aBookmark,
                                     const nsACString& aTitle,
                                     int64_t aSyncChangeDelta)
{
  nsCOMPtr<mozIStorageStatement> statement = mDB->GetStatement(
    "UPDATE moz_bookmarks SET "
     "title = :item_title, lastModified = :date, "
     "syncChangeCounter = syncChangeCounter + :delta "
    "WHERE id = :item_id"
  );
  NS_ENSURE_STATE(statement);
  mozStorageStatementScoper scoper(statement);

  nsresult rv;
  if (aTitle.IsEmpty()) {
    rv = statement->BindNullByName(NS_LITERAL_CSTRING("item_title"));
  }
  else {
    rv = statement->BindUTF8StringByName(NS_LITERAL_CSTRING("item_title"),
                                         aTitle);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  aBookmark.lastModified = RoundToMilliseconds(RoundedPRNow());
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("date"),
                                  aBookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aBookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("delta"),
                                  aSyncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemTitle(int64_t aItemId,
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
nsNavBookmarks::GetBookmarkURI(int64_t aItemId,
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
nsNavBookmarks::GetItemType(int64_t aItemId, uint16_t* _type)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(_type);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  *_type = static_cast<uint16_t>(bookmark.type);
  return NS_OK;
}


nsresult
nsNavBookmarks::ResultNodeForContainer(int64_t aItemId,
                                       nsNavHistoryQueryOptions* aOptions,
                                       nsNavHistoryResultNode** aNode)
{
  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  if (bookmark.type == TYPE_FOLDER) { // TYPE_FOLDER
    *aNode = new nsNavHistoryFolderResultNode(bookmark.title,
                                              aOptions,
                                              bookmark.id);
  }
  else {
    return NS_ERROR_INVALID_ARG;
  }

  (*aNode)->mDateAdded = bookmark.dateAdded;
  (*aNode)->mLastModified = bookmark.lastModified;
  (*aNode)->mBookmarkGuid = bookmark.guid;
  (*aNode)->GetAsFolder()->mTargetFolderGuid = bookmark.guid;

  NS_ADDREF(*aNode);
  return NS_OK;
}


nsresult
nsNavBookmarks::QueryFolderChildren(
  int64_t aFolderId,
  nsNavHistoryQueryOptions* aOptions,
  nsCOMArray<nsNavHistoryResultNode>* aChildren)
{
  NS_ENSURE_ARG_POINTER(aOptions);
  NS_ENSURE_ARG_POINTER(aChildren);

  // Select all children of a given folder, sorted by position.
  // This is a LEFT JOIN because not all bookmarks types have a place.
  // We construct a result where the first columns exactly match those returned
  // by mDBGetURLPageInfo, and additionally contains columns for position,
  // item_child, and folder_child from moz_bookmarks.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "SELECT h.id, h.url, b.title, h.rev_host, h.visit_count, "
           "h.last_visit_date, null, b.id, b.dateAdded, b.lastModified, "
           "b.parent, null, h.frecency, h.hidden, h.guid, null, null, null, "
           "b.guid, b.position, b.type, b.fk "
    "FROM moz_bookmarks b "
    "LEFT JOIN moz_places h ON b.fk = h.id "
    "WHERE b.parent = :parent "
    "ORDER BY b.position ASC"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(stmt, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t index = -1;
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
  int32_t& aCurrentIndex)
{
  NS_ENSURE_ARG_POINTER(aRow);
  NS_ENSURE_ARG_POINTER(aOptions);
  NS_ENSURE_ARG_POINTER(aChildren);

  // The results will be in order of aCurrentIndex. Even if we don't add a node
  // because it was excluded, we need to count its index, so do that before
  // doing anything else.
  aCurrentIndex++;

  int32_t itemType;
  nsresult rv = aRow->GetInt32(kGetChildrenIndex_Type, &itemType);
  NS_ENSURE_SUCCESS(rv, rv);
  int64_t id;
  rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemId, &id);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsNavHistoryResultNode> node;

  if (itemType == TYPE_BOOKMARK) {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    rv = history->RowToResult(aRow, aOptions, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t nodeType;
    node->GetType(&nodeType);
    if ((nodeType == nsINavHistoryResultNode::RESULT_TYPE_QUERY &&
         aOptions->ExcludeQueries()) ||
        (nodeType != nsINavHistoryResultNode::RESULT_TYPE_QUERY &&
         nodeType != nsINavHistoryResultNode::RESULT_TYPE_FOLDER_SHORTCUT &&
         aOptions->ExcludeItems())) {
      return NS_OK;
    }
  }
  else if (itemType == TYPE_FOLDER) {
    // ExcludeReadOnlyFolders currently means "ExcludeLivemarks" (to be fixed in
    // bug 1072833)
    if (aOptions->ExcludeReadOnlyFolders()) {
      if (IsLivemark(id))
        return NS_OK;
    }

    nsAutoCString title;
    bool isNull;
    rv = aRow->GetIsNull(nsNavHistory::kGetInfoIndex_Title, &isNull);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!isNull) {
      rv = aRow->GetUTF8String(nsNavHistory::kGetInfoIndex_Title, title);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    node = new nsNavHistoryFolderResultNode(title, aOptions, id);

    rv = aRow->GetUTF8String(kGetChildrenIndex_Guid, node->mBookmarkGuid);
    NS_ENSURE_SUCCESS(rv, rv);
    node->GetAsFolder()->mTargetFolderGuid = node->mBookmarkGuid;

    rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemDateAdded,
                        reinterpret_cast<int64_t*>(&node->mDateAdded));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemLastModified,
                        reinterpret_cast<int64_t*>(&node->mLastModified));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // This is a separator.
    if (aOptions->ExcludeItems()) {
      return NS_OK;
    }
    node = new nsNavHistorySeparatorResultNode();

    node->mItemId = id;
    rv = aRow->GetUTF8String(kGetChildrenIndex_Guid, node->mBookmarkGuid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemDateAdded,
                        reinterpret_cast<int64_t*>(&node->mDateAdded));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemLastModified,
                        reinterpret_cast<int64_t*>(&node->mLastModified));
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
  int64_t aFolderId,
  mozIStoragePendingStatement** _pendingStmt)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(_pendingStmt);

  // Select all children of a given folder, sorted by position.
  // This is a LEFT JOIN because not all bookmarks types have a place.
  // We construct a result where the first columns exactly match those returned
  // by mDBGetURLPageInfo, and additionally contains columns for position,
  // item_child, and folder_child from moz_bookmarks.
  nsCOMPtr<mozIStorageAsyncStatement> stmt = mDB->GetAsyncStatement(
    "SELECT h.id, h.url, b.title, h.rev_host, h.visit_count, "
           "h.last_visit_date, null, b.id, b.dateAdded, b.lastModified, "
           "b.parent, null, h.frecency, h.hidden, h.guid, null, null, null, "
           "b.guid, b.position, b.type, b.fk "
    "FROM moz_bookmarks b "
    "LEFT JOIN moz_places h ON b.fk = h.id "
    "WHERE b.parent = :parent "
    "ORDER BY b.position ASC"
  );
  NS_ENSURE_STATE(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> pendingStmt;
  rv = stmt->ExecuteAsync(aNode, getter_AddRefs(pendingStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*_pendingStmt = pendingStmt);
  return NS_OK;
}


nsresult
nsNavBookmarks::FetchFolderInfo(int64_t aFolderId,
                                int32_t* _folderCount,
                                nsACString& _guid,
                                int64_t* _parentId)
{
  *_folderCount = 0;
  *_parentId = -1;

  // This query has to always return results, so it can't be written as a join,
  // though a left join of 2 subqueries would have the same cost.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "SELECT count(*), "
            "(SELECT guid FROM moz_bookmarks WHERE id = :parent), "
            "(SELECT parent FROM moz_bookmarks WHERE id = :parent) "
    "FROM moz_bookmarks "
    "WHERE parent = :parent"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

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

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "SELECT 1 FROM moz_bookmarks b "
    "JOIN moz_places h ON b.fk = h.id "
    "WHERE h.url_hash = hash(:page_url) AND h.url = :page_url"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

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

  *_retval = nullptr;

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  int64_t placeId;
  nsAutoCString placeGuid;
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

  nsCString query = nsPrintfCString(
    "SELECT url FROM moz_places WHERE id = ( "
      "SELECT :page_id FROM moz_bookmarks WHERE fk = :page_id "
      "UNION ALL "
      "SELECT COALESCE(grandparent.place_id, parent.place_id) AS r_place_id "
      "FROM moz_historyvisits dest "
      "LEFT JOIN moz_historyvisits parent ON parent.id = dest.from_visit "
                                        "AND dest.visit_type IN (%d, %d) "
      "LEFT JOIN moz_historyvisits grandparent ON parent.from_visit = grandparent.id "
                                             "AND parent.visit_type IN (%d, %d) "
      "WHERE dest.place_id = :page_id "
      "AND EXISTS(SELECT 1 FROM moz_bookmarks WHERE fk = r_place_id) "
      "LIMIT 1 "
    ")",
    nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
    nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY,
    nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
    nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY
  );

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(query);
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), placeId);
  NS_ENSURE_SUCCESS(rv, rv);
  bool hasBookmarkedOrigin;
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasBookmarkedOrigin)) &&
      hasBookmarkedOrigin) {
    nsAutoCString spec;
    rv = stmt->GetUTF8String(0, spec);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = NS_NewURI(_retval, spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If there is no bookmarked origin, we will just return null.
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::ChangeBookmarkURI(int64_t aBookmarkId, nsIURI* aNewURI,
                                  uint16_t aSource)
{
  NS_ENSURE_ARG_MIN(aBookmarkId, 1);
  NS_ENSURE_ARG(aNewURI);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aBookmarkId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG(bookmark.type == TYPE_BOOKMARK);

  mozStorageTransaction transaction(mDB->MainConn(), false);

  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isTagging = bookmark.grandParentId == tagsRootId;
  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  int64_t newPlaceId;
  nsAutoCString newPlaceGuid;
  rv = history->GetOrCreateIdForPage(aNewURI, &newPlaceId, newPlaceGuid);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!newPlaceId)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<mozIStorageStatement> statement = mDB->GetStatement(
    "UPDATE moz_bookmarks SET "
     "fk = :page_id, lastModified = :date, "
     "syncChangeCounter = syncChangeCounter + :delta "
    "WHERE id = :item_id "
  );
  NS_ENSURE_STATE(statement);
  mozStorageStatementScoper scoper(statement);

  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), newPlaceId);
  NS_ENSURE_SUCCESS(rv, rv);
  bookmark.lastModified = RoundedPRNow();
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("date"),
                                  bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), bookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("delta"), syncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  if (isTagging) {
    // For consistency with the tagging service behavior, changing a tag entry's
    // URL bumps the change counter for bookmarks with the old and new URIs.
    rv = AddSyncChangesForBookmarksWithURL(bookmark.url, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AddSyncChangesForBookmarksWithURI(aNewURI, syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = history->UpdateFrecency(newPlaceId);
  NS_ENSURE_SUCCESS(rv, rv);

  // Upon changing the URI for a bookmark, update the frecency for the old
  // place as well.
  rv = history->UpdateFrecency(bookmark.placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString spec;
  rv = aNewURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(bookmark.id,
                                 NS_LITERAL_CSTRING("uri"),
                                 false,
                                 spec,
                                 bookmark.lastModified,
                                 bookmark.type,
                                 bookmark.parentId,
                                 bookmark.guid,
                                 bookmark.parentGuid,
                                 bookmark.url,
                                 aSource));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetFolderIdForItem(int64_t aItemId, int64_t* _parentId)
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
                                           nsTArray<int64_t>& aResult)
{
  NS_ENSURE_ARG(aURI);

  // Double ordering covers possible lastModified ties, that could happen when
  // importing, syncing or due to extensions.
  // Note: not using a JOIN is cheaper in this case.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "/* do not warn (bug 1175249) */ "
    "SELECT b.id "
    "FROM moz_bookmarks b "
    "JOIN moz_bookmarks t on t.id = b.parent "
    "WHERE b.fk = (SELECT id FROM moz_places WHERE url_hash = hash(:page_url) AND url = :page_url) AND "
    "t.parent IS NOT :tags_root "
    "ORDER BY b.lastModified DESC, b.id DESC "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  int64_t tagsRootId;
  nsresult rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("tags_root"), tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool more;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&more))) && more) {
    int64_t bookmarkId;
    rv = stmt->GetInt64(0, &bookmarkId);
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

  // Double ordering covers possible lastModified ties, that could happen when
  // importing, syncing or due to extensions.
  // Note: not using a JOIN is cheaper in this case.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
    "/* do not warn (bug 1175249) */ "
    "SELECT b.id, b.guid, b.parent, b.lastModified, t.guid, t.parent, b.syncStatus "
    "FROM moz_bookmarks b "
    "JOIN moz_bookmarks t on t.id = b.parent "
    "WHERE b.fk = (SELECT id FROM moz_places WHERE url_hash = hash(:page_url) AND url = :page_url) "
    "ORDER BY b.lastModified DESC, b.id DESC "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId;
  rv = GetTagsFolder(&tagsRootId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool more;
  nsAutoString tags;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&more))) && more) {
    // Skip tags.
    int64_t grandParentId;
    nsresult rv = stmt->GetInt64(5, &grandParentId);
    NS_ENSURE_SUCCESS(rv, rv);
    if (grandParentId == tagsRootId) {
      continue;
    }

    BookmarkData bookmark;
    bookmark.grandParentId = grandParentId;
    rv = stmt->GetInt64(0, &bookmark.id);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetUTF8String(1, bookmark.guid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetInt64(2, &bookmark.parentId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetInt64(3, reinterpret_cast<int64_t*>(&bookmark.lastModified));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetUTF8String(4, bookmark.parentGuid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetInt32(6, &bookmark.syncStatus);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(aBookmarks.AppendElement(bookmark), NS_ERROR_OUT_OF_MEMORY);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkIdsForURI(nsIURI* aURI, uint32_t* aCount,
                                     int64_t** aBookmarks)
{
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_ARG_POINTER(aBookmarks);

  *aCount = 0;
  *aBookmarks = nullptr;
  nsTArray<int64_t> bookmarks;

  // Get the information from the DB as a TArray
  nsresult rv = GetBookmarkIdsForURITArray(aURI, bookmarks);
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the results into a new array for output
  if (bookmarks.Length()) {
    *aBookmarks =
      static_cast<int64_t*>(moz_xmalloc(sizeof(int64_t) * bookmarks.Length()));
    if (!*aBookmarks)
      return NS_ERROR_OUT_OF_MEMORY;
    for (uint32_t i = 0; i < bookmarks.Length(); i ++)
      (*aBookmarks)[i] = bookmarks[i];
  }

  *aCount = bookmarks.Length();
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemIndex(int64_t aItemId, int32_t* _index)
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
nsNavBookmarks::SetItemIndex(int64_t aItemId,
                             int32_t aNewIndex,
                             uint16_t aSource)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_MIN(aNewIndex, 0);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  // Ensure we are not going out of range.
  int32_t folderCount;
  int64_t grandParentId;
  nsAutoCString folderGuid;
  rv = FetchFolderInfo(bookmark.parentId, &folderCount, folderGuid, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(aNewIndex < folderCount, NS_ERROR_INVALID_ARG);
  // Check the parent's guid is the expected one.
  MOZ_ASSERT(bookmark.parentGuid == folderGuid);

  mozStorageTransaction transaction(mDB->MainConn(), false);

  {
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "UPDATE moz_bookmarks SET "
       "position = :item_index "
      "WHERE id = :item_id"
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_index"), aNewIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  {
    // Sync stores child indices in the parent's record, so we only need to
    // bump the parent's change counter.
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "UPDATE moz_bookmarks SET "
       "syncChangeCounter = syncChangeCounter + :delta "
      "WHERE id = :parent_id"
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent_id"),
                               bookmark.parentId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("delta"),
                               syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = AdjustSeparatorsSyncCounter(bookmark.parentId, aNewIndex, syncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = PreventSyncReparenting(bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

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
                               bookmark.parentGuid,
                               aSource));

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetKeywordForBookmark(int64_t aBookmarkId,
                                      const nsAString& aUserCasedKeyword,
                                      uint16_t aSource)
{
  NS_ENSURE_ARG_MIN(aBookmarkId, 1);

  // This also ensures the bookmark is valid.
  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aBookmarkId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), bookmark.url);
  NS_ENSURE_SUCCESS(rv, rv);

  // Shortcuts are always lowercased internally.
  nsAutoString keyword(aUserCasedKeyword);
  ToLowerCase(keyword);

  // The same URI can be associated to more than one keyword, provided the post
  // data differs.  Check if there are already keywords associated to this uri.
  nsTArray<nsString> oldKeywords;
  {
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT keyword FROM moz_keywords WHERE place_id = :place_id"
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("place_id"), bookmark.placeId);
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasMore;
    while (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
      nsString oldKeyword;
      rv = stmt->GetString(0, oldKeyword);
      NS_ENSURE_SUCCESS(rv, rv);
      oldKeywords.AppendElement(oldKeyword);
    }
  }

  // Trying to remove a non-existent keyword is a no-op.
  if (keyword.IsEmpty() && oldKeywords.Length() == 0) {
    return NS_OK;
  }

  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  if (keyword.IsEmpty()) {
    mozStorageTransaction removeTxn(mDB->MainConn(), false);

    // We are removing the existing keywords.
    for (uint32_t i = 0; i < oldKeywords.Length(); ++i) {
      nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
        "DELETE FROM moz_keywords WHERE keyword = :old_keyword"
      );
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);
      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("old_keyword"),
                                  oldKeywords[i]);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsTArray<BookmarkData> bookmarks;
    rv = GetBookmarksForURI(uri, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    if (syncChangeDelta && !bookmarks.IsEmpty()) {
      // Build a query to update all bookmarks in a single statement.
      nsAutoCString changedIds;
      changedIds.AppendInt(bookmarks[0].id);
      for (uint32_t i = 1; i < bookmarks.Length(); ++i) {
        changedIds.Append(',');
        changedIds.AppendInt(bookmarks[i].id);
      }
      // Update the sync change counter for all bookmarks with the removed
      // keyword.
      nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
        NS_LITERAL_CSTRING(
        "UPDATE moz_bookmarks SET "
         "syncChangeCounter = syncChangeCounter + :delta "
        "WHERE id IN (") + changedIds + NS_LITERAL_CSTRING(")")
      );
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);

      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("delta"),
                                 syncChangeDelta);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = removeTxn.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    for (uint32_t i = 0; i < bookmarks.Length(); ++i) {
      NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                       nsINavBookmarkObserver,
                       OnItemChanged(bookmarks[i].id,
                                     NS_LITERAL_CSTRING("keyword"),
                                     false,
                                     EmptyCString(),
                                     bookmarks[i].lastModified,
                                     TYPE_BOOKMARK,
                                     bookmarks[i].parentId,
                                     bookmarks[i].guid,
                                     bookmarks[i].parentGuid,
                                     // Abusing oldVal to pass out the url.
                                     bookmark.url,
                                     aSource));
    }

    return NS_OK;
  }

  // A keyword can only be associated to a single URI.  Check if the requested
  // keyword was already associated, in such a case we will need to notify about
  // the change.
  nsAutoCString oldSpec;
  nsCOMPtr<nsIURI> oldUri;
  {
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT url "
      "FROM moz_keywords "
      "JOIN moz_places h ON h.id = place_id "
      "WHERE keyword = :keyword"
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("keyword"), keyword);
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasMore;
    if (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
      rv = stmt->GetUTF8String(0, oldSpec);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = NS_NewURI(getter_AddRefs(oldUri), oldSpec);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // If another uri is using the new keyword, we must update the keyword entry.
  // Note we cannot use INSERT OR REPLACE cause it wouldn't invoke the delete
  // trigger.
  mozStorageTransaction updateTxn(mDB->MainConn(), false);

  nsCOMPtr<mozIStorageStatement> stmt;
  if (oldUri) {
    // In both cases, notify about the change.
    nsTArray<BookmarkData> bookmarks;
    rv = GetBookmarksForURI(oldUri, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);
    for (uint32_t i = 0; i < bookmarks.Length(); ++i) {
      NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                       nsINavBookmarkObserver,
                       OnItemChanged(bookmarks[i].id,
                                     NS_LITERAL_CSTRING("keyword"),
                                     false,
                                     EmptyCString(),
                                     bookmarks[i].lastModified,
                                     TYPE_BOOKMARK,
                                     bookmarks[i].parentId,
                                     bookmarks[i].guid,
                                     bookmarks[i].parentGuid,
                                     // Abusing oldVal to pass out the url.
                                     oldSpec,
                                     aSource));
    }

    stmt = mDB->GetStatement(
      "UPDATE moz_keywords SET place_id = :place_id WHERE keyword = :keyword"
    );
    NS_ENSURE_STATE(stmt);
  }
  else {
    stmt = mDB->GetStatement(
      "INSERT INTO moz_keywords (keyword, place_id) "
      "VALUES (:keyword, :place_id)"
    );
  }
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("place_id"), bookmark.placeId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("keyword"), keyword);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<BookmarkData> bookmarks;
  rv = GetBookmarksForURI(uri, bookmarks);
  NS_ENSURE_SUCCESS(rv, rv);

  if (syncChangeDelta && !bookmarks.IsEmpty()) {
    // Build a query to update all bookmarks in a single statement.
    nsAutoCString changedIds;
    changedIds.AppendInt(bookmarks[0].id);
    for (uint32_t i = 1; i < bookmarks.Length(); ++i) {
      changedIds.Append(',');
      changedIds.AppendInt(bookmarks[i].id);
    }
    // Update the sync change counter for all bookmarks with the new keyword.
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      NS_LITERAL_CSTRING(
      "UPDATE moz_bookmarks SET "
       "syncChangeCounter = syncChangeCounter + :delta "
      "WHERE id IN (") + changedIds + NS_LITERAL_CSTRING(")")
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("delta"), syncChangeDelta);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = updateTxn.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // In both cases, notify about the change.
  for (uint32_t i = 0; i < bookmarks.Length(); ++i) {
    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavBookmarkObserver,
                     OnItemChanged(bookmarks[i].id,
                                   NS_LITERAL_CSTRING("keyword"),
                                   false,
                                   NS_ConvertUTF16toUTF8(keyword),
                                   bookmarks[i].lastModified,
                                   TYPE_BOOKMARK,
                                   bookmarks[i].parentId,
                                   bookmarks[i].guid,
                                   bookmarks[i].parentGuid,
                                   // Abusing oldVal to pass out the url.
                                   bookmark.url,
                                   aSource));
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetKeywordForBookmark(int64_t aBookmarkId, nsAString& aKeyword)
{
  NS_ENSURE_ARG_MIN(aBookmarkId, 1);
  aKeyword.Truncate(0);

  // We can have multiple keywords for the same uri, here we'll just return the
  // last created one.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(NS_LITERAL_CSTRING(
    "SELECT k.keyword "
    "FROM moz_bookmarks b "
    "JOIN moz_keywords k ON k.place_id = b.fk "
    "WHERE b.id = :item_id "
    "ORDER BY k.ROWID DESC "
    "LIMIT 1"
  ));
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"),
                             aBookmarkId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore;
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
    nsAutoString keyword;
    rv = stmt->GetString(0, keyword);
    NS_ENSURE_SUCCESS(rv, rv);
    aKeyword = keyword;
    return NS_OK;
  }

  aKeyword.SetIsVoid(true);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RunInBatchMode(nsINavHistoryBatchCallback* aCallback,
                               nsISupports* aUserData) {
  AUTO_PROFILER_LABEL("nsNavBookmarks::RunInBatchMode", OTHER);

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

  if (NS_WARN_IF(!mCanNotify))
    return NS_ERROR_UNEXPECTED;

  return mObservers.AppendWeakElement(aObserver, aOwnsWeak);
}


NS_IMETHODIMP
nsNavBookmarks::RemoveObserver(nsINavBookmarkObserver* aObserver)
{
  return mObservers.RemoveWeakElement(aObserver);
}

NS_IMETHODIMP
nsNavBookmarks::GetObservers(uint32_t* _count,
                             nsINavBookmarkObserver*** _observers)
{
  NS_ENSURE_ARG_POINTER(_count);
  NS_ENSURE_ARG_POINTER(_observers);

  *_count = 0;
  *_observers = nullptr;

  if (!mCanNotify)
    return NS_OK;

  nsCOMArray<nsINavBookmarkObserver> observers;

  // First add the category cache observers.
  mCacheObservers.GetEntries(observers);

  // Then add the other observers.
  for (uint32_t i = 0; i < mObservers.Length(); ++i) {
    const nsCOMPtr<nsINavBookmarkObserver> &observer = mObservers.ElementAt(i).GetValue();
    // Skip nullified weak observers.
    if (observer)
      observers.AppendElement(observer);
  }

  if (observers.Count() == 0)
    return NS_OK;

  *_count = observers.Count();
  observers.Forget(_observers);

  return NS_OK;
}

void
nsNavBookmarks::NotifyItemVisited(const ItemVisitData& aData)
{
  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), aData.bookmark.url));
  // Notify the visit only if we have a valid uri, otherwise the observer
  // couldn't gather enough data from the notification.
  // This should be false only if there's a bug in the code preceding us.
  if (uri) {
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
                                 aData.bookmark.parentGuid,
                                 aData.oldValue,
                                 // We specify the default source here because
                                 // this method is only called for history
                                 // visits, and we don't track sources in
                                 // history.
                                 SOURCE_DEFAULT));
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsNavBookmarks::Observe(nsISupports *aSubject, const char *aTopic,
                        const char16_t *aData)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  if (strcmp(aTopic, TOPIC_PLACES_SHUTDOWN) == 0) {
    // Stop Observing annotations.
    nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
    if (annosvc) {
      annosvc->RemoveObserver(this);
    }
  }
  else if (strcmp(aTopic, TOPIC_PLACES_CONNECTION_CLOSED) == 0) {
    // Don't even try to notify observers from this point on, the category
    // cache would init services that could try to use our APIs.
    mCanNotify = false;
    mObservers.Clear();
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
  }

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver, OnEndUpdateBatch());
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnVisit(nsIURI* aURI, int64_t aVisitId, PRTime aTime,
                        int64_t aSessionID, int64_t aReferringID,
                        uint32_t aTransitionType, const nsACString& aGUID,
                        bool aHidden, uint32_t aVisitCount, uint32_t aTyped,
                        const nsAString& aLastKnownTitle)
{
  NS_ENSURE_ARG(aURI);

  // If the page is bookmarked, notify observers for each associated bookmark.
  ItemVisitData visitData;
  nsresult rv = aURI->GetSpec(visitData.bookmark.url);
  NS_ENSURE_SUCCESS(rv, rv);
  visitData.visitId = aVisitId;
  visitData.time = aTime;
  visitData.transitionType = aTransitionType;

  RefPtr< AsyncGetBookmarksForURI<ItemVisitMethod, ItemVisitData> > notifier =
    new AsyncGetBookmarksForURI<ItemVisitMethod, ItemVisitData>(this, &nsNavBookmarks::NotifyItemVisited, visitData);
  notifier->Init();
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnDeleteURI(nsIURI* aURI,
                            const nsACString& aGUID,
                            uint16_t aReason)
{
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
nsNavBookmarks::OnFrecencyChanged(nsIURI* aURI,
                                  int32_t aNewFrecency,
                                  const nsACString& aGUID,
                                  bool aHidden,
                                  PRTime aLastVisitDate)
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnManyFrecenciesChanged()
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnPageChanged(nsIURI* aURI,
                              uint32_t aChangedAttribute,
                              const nsAString& aNewValue,
                              const nsACString& aGUID)
{
  NS_ENSURE_ARG(aURI);

  nsresult rv;
  if (aChangedAttribute == nsINavHistoryObserver::ATTRIBUTE_FAVICON) {
    ItemChangeData changeData;
    rv = aURI->GetSpec(changeData.bookmark.url);
    NS_ENSURE_SUCCESS(rv, rv);
    changeData.property = NS_LITERAL_CSTRING("favicon");
    changeData.isAnnotation = false;
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
      RefPtr< AsyncGetBookmarksForURI<ItemChangeMethod, ItemChangeData> > notifier =
        new AsyncGetBookmarksForURI<ItemChangeMethod, ItemChangeData>(this, &nsNavBookmarks::NotifyItemChanged, changeData);
      notifier->Init();
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnDeleteVisits(nsIURI* aURI, PRTime aVisitTime,
                               const nsACString& aGUID,
                               uint16_t aReason, uint32_t aTransitionType)
{
  NS_ENSURE_ARG(aURI);

  // Notify "cleartime" only if all visits to the page have been removed.
  if (!aVisitTime) {
    // If the page is bookmarked, notify observers for each associated bookmark.
    ItemChangeData changeData;
    nsresult rv = aURI->GetSpec(changeData.bookmark.url);
    NS_ENSURE_SUCCESS(rv, rv);
    changeData.property = NS_LITERAL_CSTRING("cleartime");
    changeData.isAnnotation = false;
    changeData.bookmark.lastModified = 0;
    changeData.bookmark.type = TYPE_BOOKMARK;

    RefPtr< AsyncGetBookmarksForURI<ItemChangeMethod, ItemChangeData> > notifier =
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
nsNavBookmarks::OnItemAnnotationSet(int64_t aItemId, const nsACString& aName,
                                    uint16_t aSource)
{
  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  bookmark.lastModified = RoundedPRNow();
  rv = SetItemDateInternal(LAST_MODIFIED, DetermineSyncChangeDelta(aSource),
                           bookmark.id, bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(bookmark.id,
                                 aName,
                                 true,
                                 EmptyCString(),
                                 bookmark.lastModified,
                                 bookmark.type,
                                 bookmark.parentId,
                                 bookmark.guid,
                                 bookmark.parentGuid,
                                 EmptyCString(),
                                 aSource));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnPageAnnotationRemoved(nsIURI* aPage, const nsACString& aName)
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnItemAnnotationRemoved(int64_t aItemId, const nsACString& aName,
                                        uint16_t aSource)
{
  // As of now this is doing the same as OnItemAnnotationSet, so just forward
  // the call.
  nsresult rv = OnItemAnnotationSet(aItemId, aName, aSource);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}
