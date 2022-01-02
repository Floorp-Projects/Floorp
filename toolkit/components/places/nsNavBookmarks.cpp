/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNavBookmarks.h"

#include "nsNavHistory.h"
#include "nsPlacesMacros.h"
#include "Helpers.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsITaggingService.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"
#include "nsQueryObject.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/storage.h"
#include "mozilla/dom/PlacesBookmarkAddition.h"
#include "mozilla/dom/PlacesBookmarkRemoved.h"
#include "mozilla/dom/PlacesBookmarkTags.h"
#include "mozilla/dom/PlacesBookmarkTime.h"
#include "mozilla/dom/PlacesBookmarkTitle.h"
#include "mozilla/dom/PlacesObservers.h"
#include "mozilla/dom/PlacesVisit.h"

using namespace mozilla;

// These columns sit to the right of the kGetInfoIndex_* columns.
const int32_t nsNavBookmarks::kGetChildrenIndex_Guid = 18;
const int32_t nsNavBookmarks::kGetChildrenIndex_Position = 19;
const int32_t nsNavBookmarks::kGetChildrenIndex_Type = 20;
const int32_t nsNavBookmarks::kGetChildrenIndex_PlaceID = 21;
const int32_t nsNavBookmarks::kGetChildrenIndex_SyncStatus = 22;

using namespace mozilla::places;

extern "C" {

// Returns the total number of Sync changes recorded since Places startup for
// all bookmarks. This function uses C linkage because it's called from the
// Rust synced bookmarks mirror, on the storage thread. Using `get_service` to
// access the bookmarks service from Rust trips a thread-safety assertion, so
// we can't use `nsNavBookmarks::GetTotalSyncChanges`.
int64_t NS_NavBookmarksTotalSyncChanges() {
  return nsNavBookmarks::sTotalSyncChanges;
}

}  // extern "C"

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsNavBookmarks, gBookmarksService)

namespace {

// Returns the sync change counter increment for a change source constant.
inline int64_t DetermineSyncChangeDelta(uint16_t aSource) {
  return aSource == nsINavBookmarksService::SOURCE_SYNC ? 0 : 1;
}

// Returns the sync status for a new item inserted by a change source.
inline int32_t DetermineInitialSyncStatus(uint16_t aSource) {
  if (aSource == nsINavBookmarksService::SOURCE_SYNC) {
    return nsINavBookmarksService::SYNC_STATUS_NORMAL;
  }
  if (aSource == nsINavBookmarksService::SOURCE_RESTORE_ON_STARTUP) {
    return nsINavBookmarksService::SYNC_STATUS_UNKNOWN;
  }
  return nsINavBookmarksService::SYNC_STATUS_NEW;
}

// Indicates whether an item has been uploaded to the server and
// needs a tombstone on deletion.
inline bool NeedsTombstone(const BookmarkData& aBookmark) {
  return aBookmark.syncStatus == nsINavBookmarksService::SYNC_STATUS_NORMAL;
}

inline nsresult GetTags(nsIURI* aURI, nsTArray<nsString>& aResult) {
  nsresult rv;
  nsCOMPtr<nsITaggingService> taggingService =
      do_GetService("@mozilla.org/browser/tagging-service;1", &rv);

  if (NS_FAILED(rv)) {
    return rv;
  }

  return taggingService->GetTagsForURI(aURI, aResult);
}

}  // namespace

nsNavBookmarks::nsNavBookmarks() : mCanNotify(false) {
  NS_ASSERTION(!gBookmarksService,
               "Attempting to create two instances of the service!");
  gBookmarksService = this;
}

nsNavBookmarks::~nsNavBookmarks() {
  NS_ASSERTION(gBookmarksService == this,
               "Deleting a non-singleton instance of the service");
  if (gBookmarksService == this) gBookmarksService = nullptr;
}

NS_IMPL_ISUPPORTS(nsNavBookmarks, nsINavBookmarksService, nsIObserver,
                  nsISupportsWeakReference)

Atomic<int64_t> nsNavBookmarks::sLastInsertedItemId(0);

void  // static
nsNavBookmarks::StoreLastInsertedId(const nsACString& aTable,
                                    const int64_t aLastInsertedId) {
  MOZ_ASSERT(aTable.EqualsLiteral("moz_bookmarks"));
  sLastInsertedItemId = aLastInsertedId;
}

Atomic<int64_t> nsNavBookmarks::sTotalSyncChanges(0);

void  // static
nsNavBookmarks::NoteSyncChange() {
  sTotalSyncChanges++;
}

nsresult nsNavBookmarks::Init() {
  mDB = Database::GetDatabase();
  NS_ENSURE_STATE(mDB);

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    (void)os->AddObserver(this, TOPIC_PLACES_CONNECTION_CLOSED, true);
  }

  mCanNotify = true;

  // DO NOT PUT STUFF HERE that can fail. See observer comment above.

  return NS_OK;
}

nsresult nsNavBookmarks::AdjustIndices(int64_t aFolderId, int32_t aStartIndex,
                                       int32_t aEndIndex, int32_t aDelta) {
  NS_ASSERTION(
      aStartIndex >= 0 && aEndIndex <= INT32_MAX && aStartIndex <= aEndIndex,
      "Bad indices");

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "UPDATE moz_bookmarks SET position = position + :delta "
      "WHERE parent = :parent "
      "AND position BETWEEN :from_index AND :to_index");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt32ByName("delta"_ns, aDelta);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName("parent"_ns, aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName("from_index"_ns, aStartIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName("to_index"_ns, aEndIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsNavBookmarks::AdjustSeparatorsSyncCounter(int64_t aFolderId,
                                                     int32_t aStartIndex,
                                                     int64_t aSyncChangeDelta) {
  MOZ_ASSERT(aStartIndex >= 0, "Bad start position");
  if (!aSyncChangeDelta) {
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "UPDATE moz_bookmarks SET syncChangeCounter = syncChangeCounter + :delta "
      "WHERE parent = :parent AND position >= :start_index "
      "AND type = :item_type ");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName("delta"_ns, aSyncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName("parent"_ns, aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName("start_index"_ns, aStartIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName("item_type"_ns, TYPE_SEPARATOR);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetPlacesRoot(int64_t* aRoot) {
  int64_t id = mDB->GetRootFolderId();
  NS_ENSURE_TRUE(id > 0, NS_ERROR_UNEXPECTED);
  *aRoot = id;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarksMenuFolder(int64_t* aRoot) {
  int64_t id = mDB->GetMenuFolderId();
  NS_ENSURE_TRUE(id > 0, NS_ERROR_UNEXPECTED);
  *aRoot = id;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetToolbarFolder(int64_t* aRoot) {
  int64_t id = mDB->GetToolbarFolderId();
  NS_ENSURE_TRUE(id > 0, NS_ERROR_UNEXPECTED);
  *aRoot = id;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetTagsFolder(int64_t* aRoot) {
  int64_t id = mDB->GetTagsFolderId();
  NS_ENSURE_TRUE(id > 0, NS_ERROR_UNEXPECTED);
  *aRoot = id;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetTotalSyncChanges(int64_t* aTotalSyncChanges) {
  *aTotalSyncChanges = sTotalSyncChanges;
  return NS_OK;
}

nsresult nsNavBookmarks::InsertBookmarkInDB(
    int64_t aPlaceId, enum ItemType aItemType, int64_t aParentId,
    int32_t aIndex, const nsACString& aTitle, PRTime aDateAdded,
    PRTime aLastModified, const nsACString& aParentGuid, int64_t aGrandParentId,
    nsIURI* aURI, uint16_t aSource, int64_t* _itemId, nsACString& _guid) {
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
      ":item_guid, :sync_status, :change_counter)");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv;
  if (*_itemId != -1)
    rv = stmt->BindInt64ByName("item_id"_ns, *_itemId);
  else
    rv = stmt->BindNullByName("item_id"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aPlaceId != -1)
    rv = stmt->BindInt64ByName("page_id"_ns, aPlaceId);
  else
    rv = stmt->BindNullByName("page_id"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32ByName("item_type"_ns, aItemType);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName("parent"_ns, aParentId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName("item_index"_ns, aIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aTitle.IsEmpty())
    rv = stmt->BindNullByName("item_title"_ns);
  else
    rv = stmt->BindUTF8StringByName("item_title"_ns, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt64ByName("date_added"_ns, aDateAdded);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aLastModified) {
    rv = stmt->BindInt64ByName("last_modified"_ns, aLastModified);
  } else {
    rv = stmt->BindInt64ByName("last_modified"_ns, aDateAdded);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Could use IsEmpty because our callers check for GUID validity,
  // but it doesn't hurt.
  bool hasExistingGuid = _guid.Length() == 12;
  if (hasExistingGuid) {
    MOZ_ASSERT(IsValidGUID(_guid));
    rv = stmt->BindUTF8StringByName("item_guid"_ns, _guid);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsAutoCString guid;
    rv = GenerateGUID(guid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindUTF8StringByName("item_guid"_ns, guid);
    NS_ENSURE_SUCCESS(rv, rv);
    _guid.Assign(guid);
  }

  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);
  rv = stmt->BindInt64ByName("change_counter"_ns, syncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  uint16_t syncStatus = DetermineInitialSyncStatus(aSource);
  rv = stmt->BindInt32ByName("sync_status"_ns, syncStatus);
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

  int64_t tagsRootId = TagsRootId();
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
nsNavBookmarks::InsertBookmark(int64_t aFolder, nsIURI* aURI, int32_t aIndex,
                               const nsACString& aTitle,
                               const nsACString& aGUID, uint16_t aSource,
                               int64_t* aNewBookmarkId) {
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(aNewBookmarkId);
  NS_ENSURE_ARG_MIN(aIndex, nsINavBookmarksService::DEFAULT_INDEX);

  if (!aGUID.IsEmpty() && !IsValidGUID(aGUID)) return NS_ERROR_INVALID_ARG;

  mozStorageTransaction transaction(mDB->MainConn(), false);

  // XXX Handle the error, bug 1696133.
  Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));

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
  } else {
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
  int64_t tagsRootId = TagsRootId();
  if (grandParentId != tagsRootId) {
    rv = history->UpdateFrecency(placeId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mCanNotify) {
    return NS_OK;
  }

  Sequence<OwningNonNull<PlacesEvent>> notifications;
  nsAutoCString utf8spec;
  aURI->GetSpec(utf8spec);

  RefPtr<PlacesBookmarkAddition> bookmark = new PlacesBookmarkAddition();
  bookmark->mItemType = TYPE_BOOKMARK;
  bookmark->mId = *aNewBookmarkId;
  bookmark->mParentId = aFolder;
  bookmark->mIndex = index;
  bookmark->mUrl.Assign(NS_ConvertUTF8toUTF16(utf8spec));
  bookmark->mTitle.Assign(NS_ConvertUTF8toUTF16(title));
  bookmark->mDateAdded = dateAdded / 1000;
  bookmark->mGuid.Assign(guid);
  bookmark->mParentGuid.Assign(folderGuid);
  bookmark->mSource = aSource;
  bookmark->mIsTagging = grandParentId == mDB->GetTagsFolderId();
  bool success = !!notifications.AppendElement(bookmark.forget(), fallible);
  MOZ_RELEASE_ASSERT(success);

  // If the bookmark has been added to a tag container, notify all
  // bookmark-folder result nodes which contain a bookmark for the new
  // bookmark's url.
  if (grandParentId == tagsRootId) {
    // Notify a tags change to all bookmarks for this URI.
    nsTArray<BookmarkData> bookmarks;
    rv = GetBookmarksForURI(aURI, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    nsTArray<nsString> tags;
    rv = GetTags(aURI, tags);
    NS_ENSURE_SUCCESS(rv, rv);

    for (uint32_t i = 0; i < bookmarks.Length(); ++i) {
      // Check that bookmarks doesn't include the current tag itemId.
      MOZ_ASSERT(bookmarks[i].id != *aNewBookmarkId);
      RefPtr<PlacesBookmarkTags> tagsChanged = new PlacesBookmarkTags();
      tagsChanged->mId = bookmarks[i].id;
      tagsChanged->mItemType = TYPE_BOOKMARK;
      tagsChanged->mUrl.Assign(NS_ConvertUTF8toUTF16(utf8spec));
      tagsChanged->mGuid = bookmarks[i].guid;
      tagsChanged->mParentGuid = bookmarks[i].parentGuid;
      tagsChanged->mTags.Assign(tags);
      tagsChanged->mLastModified = bookmarks[i].lastModified / 1000;
      tagsChanged->mSource = aSource;
      tagsChanged->mIsTagging = false;
      success = !!notifications.AppendElement(tagsChanged.forget(), fallible);
      MOZ_RELEASE_ASSERT(success);
    }
  }

  PlacesObservers::NotifyListeners(notifications);

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveItem(int64_t aItemId, uint16_t aSource) {
  AUTO_PROFILER_LABEL("nsNavBookmarks::RemoveItem", OTHER);

  NS_ENSURE_ARG(!IsRoot(aItemId));

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mDB->MainConn(), false);

  // XXX Handle the error, bug 1696133.
  Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));

  if (bookmark.type == TYPE_FOLDER) {
    // Remove all of the folder's children.
    rv = RemoveFolderChildren(bookmark.id, aSource);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<mozIStorageStatement> stmt =
      mDB->GetStatement("DELETE FROM moz_bookmarks WHERE id = :item_id");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  rv = stmt->BindInt64ByName("item_id"_ns, bookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Fix indices in the parent.
  if (bookmark.position != DEFAULT_INDEX) {
    rv = AdjustIndices(bookmark.parentId, bookmark.position + 1, INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  // Add a tombstone for synced items.
  if (syncChangeDelta) {
    rv = InsertTombstone(bookmark);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bookmark.lastModified = RoundedPRNow();
  rv = SetItemDateInternal(LAST_MODIFIED, syncChangeDelta, bookmark.parentId,
                           bookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  // Mark all affected separators as changed
  rv = AdjustSeparatorsSyncCounter(bookmark.parentId, bookmark.position,
                                   syncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId = TagsRootId();
  if (bookmark.grandParentId == tagsRootId) {
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

  if (!mCanNotify) {
    return NS_OK;
  }

  Sequence<OwningNonNull<PlacesEvent>> notifications;
  RefPtr<PlacesBookmarkRemoved> bookmarkRef = new PlacesBookmarkRemoved();
  bookmarkRef->mItemType = bookmark.type;
  bookmarkRef->mId = bookmark.id;
  bookmarkRef->mParentId = bookmark.parentId;
  bookmarkRef->mIndex = bookmark.position;
  if (bookmark.type == TYPE_BOOKMARK) {
    bookmarkRef->mUrl.Assign(NS_ConvertUTF8toUTF16(bookmark.url));
  }
  bookmarkRef->mGuid.Assign(bookmark.guid);
  bookmarkRef->mParentGuid.Assign(bookmark.parentGuid);
  bookmarkRef->mSource = aSource;
  bookmarkRef->mIsTagging =
      bookmark.parentId == tagsRootId || bookmark.grandParentId == tagsRootId;
  bookmarkRef->mIsDescendantRemoval = false;
  bool success = !!notifications.AppendElement(bookmarkRef.forget(), fallible);
  MOZ_RELEASE_ASSERT(success);

  if (bookmark.type == TYPE_BOOKMARK && bookmark.grandParentId == tagsRootId &&
      uri) {
    // If the removed bookmark was child of a tag container, notify a tags
    // change to all bookmarks for this URI.
    nsTArray<BookmarkData> bookmarks;
    rv = GetBookmarksForURI(uri, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString utf8spec;
    uri->GetSpec(utf8spec);

    nsTArray<nsString> tags;
    rv = GetTags(uri, tags);
    NS_ENSURE_SUCCESS(rv, rv);

    for (uint32_t i = 0; i < bookmarks.Length(); ++i) {
      RefPtr<PlacesBookmarkTags> tagsChanged = new PlacesBookmarkTags();
      tagsChanged->mId = bookmarks[i].id;
      tagsChanged->mItemType = TYPE_BOOKMARK;
      tagsChanged->mUrl.Assign(NS_ConvertUTF8toUTF16(utf8spec));
      tagsChanged->mGuid = bookmarks[i].guid;
      tagsChanged->mParentGuid = bookmarks[i].parentGuid;
      tagsChanged->mTags.Assign(tags);
      tagsChanged->mLastModified = bookmarks[i].lastModified / 1000;
      tagsChanged->mSource = aSource;
      tagsChanged->mIsTagging = false;
      success = !!notifications.AppendElement(tagsChanged.forget(), fallible);
      MOZ_RELEASE_ASSERT(success);
    }
  }

  PlacesObservers::NotifyListeners(notifications);

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::CreateFolder(int64_t aParent, const nsACString& aTitle,
                             int32_t aIndex, const nsACString& aGUID,
                             uint16_t aSource, int64_t* aNewFolderId) {
  // NOTE: aParent can be null for root creation, so not checked
  NS_ENSURE_ARG_POINTER(aNewFolderId);
  NS_ENSURE_ARG_MIN(aIndex, nsINavBookmarksService::DEFAULT_INDEX);
  if (!aGUID.IsEmpty() && !IsValidGUID(aGUID)) return NS_ERROR_INVALID_ARG;

  // Get the correct index for insertion.  This also ensures the parent exists.
  int32_t index = aIndex, folderCount;
  int64_t grandParentId;
  nsAutoCString folderGuid;
  nsresult rv =
      FetchFolderInfo(aParent, &folderCount, folderGuid, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mDB->MainConn(), false);

  // XXX Handle the error, bug 1696133.
  Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));

  if (aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      aIndex >= folderCount) {
    index = folderCount;
  } else {
    // Create space for the insertion.
    rv = AdjustIndices(aParent, index, INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aNewFolderId = -1;
  PRTime dateAdded = RoundedPRNow();
  nsAutoCString guid(aGUID);
  nsCString title;
  TruncateTitle(aTitle, title);

  rv = InsertBookmarkInDB(-1, FOLDER, aParent, index, title, dateAdded, 0,
                          folderGuid, grandParentId, nullptr, aSource,
                          aNewFolderId, guid);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId = TagsRootId();

  if (mCanNotify) {
    Sequence<OwningNonNull<PlacesEvent>> events;
    RefPtr<PlacesBookmarkAddition> folder = new PlacesBookmarkAddition();
    folder->mItemType = TYPE_FOLDER;
    folder->mId = *aNewFolderId;
    folder->mParentId = aParent;
    folder->mIndex = index;
    folder->mTitle.Assign(NS_ConvertUTF8toUTF16(title));
    folder->mDateAdded = dateAdded / 1000;
    folder->mGuid.Assign(guid);
    folder->mParentGuid.Assign(folderGuid);
    folder->mSource = aSource;
    folder->mIsTagging = aParent == tagsRootId;
    bool success = !!events.AppendElement(folder.forget(), fallible);
    MOZ_RELEASE_ASSERT(success);

    PlacesObservers::NotifyListeners(events);
  }

  return NS_OK;
}

nsresult nsNavBookmarks::GetDescendantChildren(
    int64_t aFolderId, const nsACString& aFolderGuid, int64_t aGrandParentId,
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
        "ORDER BY b.position ASC");
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindInt64ByName("parent"_ns, aFolderId);
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
      GetDescendantChildren(aFolderChildrenArray[i].id, guid, aFolderId,
                            aFolderChildrenArray);
    }
  }

  return NS_OK;
}

nsresult nsNavBookmarks::RemoveFolderChildren(int64_t aFolderId,
                                              uint16_t aSource) {
  AUTO_PROFILER_LABEL("nsNavBookmarks::RemoveFolderChilder", OTHER);

  NS_ENSURE_ARG_MIN(aFolderId, 1);
  int64_t rootId = -1;
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

  // XXX Handle the error, bug 1696133.
  Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));

  nsCOMPtr<mozIStorageStatement> deleteStatement =
      mDB->GetStatement(nsLiteralCString("DELETE FROM moz_bookmarks "
                                         "WHERE parent IN (:parent") +
                        foldersToRemove + ")"_ns);
  NS_ENSURE_STATE(deleteStatement);
  mozStorageStatementScoper deleteStatementScoper(deleteStatement);

  rv = deleteStatement->BindInt64ByName("parent"_ns, folder.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Clean up orphan items annotations.
  nsCOMPtr<mozIStorageConnection> conn = mDB->MainConn();
  if (!conn) {
    return NS_ERROR_UNEXPECTED;
  }
  rv = conn->ExecuteSimpleSQL(
      nsLiteralCString("DELETE FROM moz_items_annos "
                       "WHERE id IN ("
                       "SELECT a.id from moz_items_annos a "
                       "LEFT JOIN moz_bookmarks b ON a.item_id = b.id "
                       "WHERE b.id ISNULL)"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the lastModified date.
  rv = SetItemDateInternal(LAST_MODIFIED, syncChangeDelta, folder.id,
                           RoundedPRNow());
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId = TagsRootId();

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

  Sequence<OwningNonNull<PlacesEvent>> notifications;
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

    if (!mCanNotify) {
      return NS_OK;
    }

    RefPtr<PlacesBookmarkRemoved> bookmark = new PlacesBookmarkRemoved();
    bookmark->mItemType = TYPE_BOOKMARK;
    bookmark->mId = child.id;
    bookmark->mParentId = child.parentId;
    bookmark->mIndex = child.position;
    bookmark->mUrl.Assign(NS_ConvertUTF8toUTF16(child.url));
    bookmark->mGuid.Assign(child.guid);
    bookmark->mParentGuid.Assign(child.parentGuid);
    bookmark->mSource = aSource;
    bookmark->mIsTagging = (child.grandParentId == tagsRootId);
    bookmark->mIsDescendantRemoval = (child.grandParentId != tagsRootId);
    bool success = !!notifications.AppendElement(bookmark.forget(), fallible);
    MOZ_RELEASE_ASSERT(success);

    if (child.type == TYPE_BOOKMARK && child.grandParentId == tagsRootId &&
        uri) {
      // If the removed bookmark was a child of a tag container, notify all
      // bookmark-folder result nodes which contain a bookmark for the removed
      // bookmark's url.
      nsTArray<BookmarkData> bookmarks;
      rv = GetBookmarksForURI(uri, bookmarks);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoCString utf8spec;
      uri->GetSpec(utf8spec);

      nsTArray<nsString> tags;
      rv = GetTags(uri, tags);
      NS_ENSURE_SUCCESS(rv, rv);

      for (uint32_t i = 0; i < bookmarks.Length(); ++i) {
        RefPtr<PlacesBookmarkTags> tagsChanged = new PlacesBookmarkTags();
        tagsChanged->mId = bookmarks[i].id;
        tagsChanged->mItemType = TYPE_BOOKMARK;
        tagsChanged->mUrl.Assign(NS_ConvertUTF8toUTF16(utf8spec));
        tagsChanged->mGuid = bookmarks[i].guid;
        tagsChanged->mParentGuid = bookmarks[i].parentGuid;
        tagsChanged->mTags.Assign(tags);
        tagsChanged->mLastModified = bookmarks[i].lastModified / 1000;
        tagsChanged->mSource = aSource;
        tagsChanged->mIsTagging = false;
        success = !!notifications.AppendElement(tagsChanged.forget(), fallible);
        MOZ_RELEASE_ASSERT(success);
      }
    }
  }

  if (notifications.Length()) {
    PlacesObservers::NotifyListeners(notifications);
  }

  return NS_OK;
}

nsresult nsNavBookmarks::FetchItemInfo(int64_t aItemId,
                                       BookmarkData& _bookmark) {
  // LEFT JOIN since not all bookmarks have an associated place.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT b.id, h.url, b.title, b.position, b.fk, b.parent, b.type, "
      "b.dateAdded, b.lastModified, b.guid, t.guid, t.parent, "
      "b.syncStatus "
      "FROM moz_bookmarks b "
      "LEFT JOIN moz_bookmarks t ON t.id = b.parent "
      "LEFT JOIN moz_places h ON h.id = b.fk "
      "WHERE b.id = :item_id");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName("item_id"_ns, aItemId);
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
  } else {
    _bookmark.grandParentId = -1;
  }
  rv = stmt->GetInt32(12, &_bookmark.syncStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsNavBookmarks::FetchItemInfo(const nsCString& aGUID,
                                       BookmarkData& _bookmark) {
  // LEFT JOIN since not all bookmarks have an associated place.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT b.id, h.url, b.title, b.position, b.fk, b.parent, b.type, "
      "b.dateAdded, b.lastModified, t.guid, t.parent, "
      "b.syncStatus "
      "FROM moz_bookmarks b "
      "LEFT JOIN moz_bookmarks t ON t.id = b.parent "
      "LEFT JOIN moz_places h ON h.id = b.fk "
      "WHERE b.guid = :item_guid");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindUTF8StringByName("item_guid"_ns, aGUID);
  NS_ENSURE_SUCCESS(rv, rv);

  _bookmark.guid = aGUID;

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasResult) {
    return NS_ERROR_INVALID_ARG;
  }

  rv = stmt->GetInt64(0, &_bookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);

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
  // Getting properties of the root would show no parent.
  rv = stmt->GetIsNull(9, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isNull) {
    rv = stmt->GetUTF8String(9, _bookmark.parentGuid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetInt64(10, &_bookmark.grandParentId);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    _bookmark.grandParentId = -1;
  }
  rv = stmt->GetInt32(11, &_bookmark.syncStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsNavBookmarks::SetItemDateInternal(enum BookmarkDate aDateType,
                                             int64_t aSyncChangeDelta,
                                             int64_t aItemId, PRTime aValue) {
  aValue = RoundToMilliseconds(aValue);

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "UPDATE moz_bookmarks SET lastModified = :date, "
      "syncChangeCounter = syncChangeCounter + :delta "
      "WHERE id = :item_id");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName("date"_ns, aValue);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName("item_id"_ns, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName("delta"_ns, aSyncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // note, we are not notifying the observers
  // that the item has changed.

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::SetItemLastModified(int64_t aItemId, PRTime aLastModified,
                                    uint16_t aSource) {
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId = TagsRootId();
  bool isTagging = bookmark.grandParentId == tagsRootId;
  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  // Round here so that we notify with the right value.
  bookmark.lastModified = RoundToMilliseconds(aLastModified);

  if (isTagging) {
    // If we're changing a tag, bump the change counter for all tagged
    // bookmarks. We use a separate code path to avoid a transaction for
    // non-tags.
    mozStorageTransaction transaction(mDB->MainConn(), false);

    // XXX Handle the error, bug 1696133.
    Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));

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

  if (mCanNotify) {
    Sequence<OwningNonNull<PlacesEvent>> events;
    RefPtr<PlacesBookmarkTime> timeChanged = new PlacesBookmarkTime();
    timeChanged->mId = bookmark.id;
    timeChanged->mItemType = bookmark.type;
    timeChanged->mUrl.Assign(NS_ConvertUTF8toUTF16(bookmark.url));
    timeChanged->mGuid = bookmark.guid;
    timeChanged->mParentGuid = bookmark.parentGuid;
    timeChanged->mDateAdded = bookmark.dateAdded / 1000;
    timeChanged->mLastModified = bookmark.lastModified / 1000;
    timeChanged->mSource = aSource;
    timeChanged->mIsTagging =
        bookmark.parentId == tagsRootId || bookmark.grandParentId == tagsRootId;
    bool success = !!events.AppendElement(timeChanged.forget(), fallible);
    MOZ_RELEASE_ASSERT(success);
    PlacesObservers::NotifyListeners(events);
  }

  return NS_OK;
}

nsresult nsNavBookmarks::AddSyncChangesForBookmarksWithURL(
    const nsACString& aURL, int64_t aSyncChangeDelta) {
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

nsresult nsNavBookmarks::AddSyncChangesForBookmarksWithURI(
    nsIURI* aURI, int64_t aSyncChangeDelta) {
  if (NS_WARN_IF(!aURI) || !aSyncChangeDelta) {
    // Ignore sync changes for invalid URIs.
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> statement = mDB->GetStatement(
      "UPDATE moz_bookmarks SET "
      "syncChangeCounter = syncChangeCounter + :delta "
      "WHERE type = :type AND "
      "fk = (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND "
      "url = :url)");
  NS_ENSURE_STATE(statement);
  mozStorageStatementScoper scoper(statement);

  nsresult rv = statement->BindInt64ByName("delta"_ns, aSyncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName("type"_ns,
                                  nsINavBookmarksService::TYPE_BOOKMARK);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = URIBinder::Bind(statement, "url"_ns, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return statement->Execute();
}

nsresult nsNavBookmarks::AddSyncChangesForBookmarksInFolder(
    int64_t aFolderId, int64_t aSyncChangeDelta) {
  if (!aSyncChangeDelta) {
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> statement = mDB->GetStatement(
      "UPDATE moz_bookmarks SET "
      "syncChangeCounter = syncChangeCounter + :delta "
      "WHERE type = :type AND "
      "fk = (SELECT fk FROM moz_bookmarks WHERE parent = :parent)");
  NS_ENSURE_STATE(statement);
  mozStorageStatementScoper scoper(statement);

  nsresult rv = statement->BindInt64ByName("delta"_ns, aSyncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName("type"_ns,
                                  nsINavBookmarksService::TYPE_BOOKMARK);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName("parent"_ns, aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsNavBookmarks::InsertTombstone(const BookmarkData& aBookmark) {
  if (!NeedsTombstone(aBookmark)) {
    return NS_OK;
  }
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "INSERT INTO moz_bookmarks_deleted (guid, dateRemoved) "
      "VALUES (:guid, :date_removed)");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindUTF8StringByName("guid"_ns, aBookmark.guid);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName("date_removed"_ns, RoundedPRNow());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsNavBookmarks::InsertTombstones(
    const nsTArray<TombstoneData>& aTombstones) {
  if (aTombstones.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<mozIStorageConnection> conn = mDB->MainConn();
  NS_ENSURE_STATE(conn);

  int32_t variableLimit = 0;
  nsresult rv = conn->GetVariableLimit(&variableLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  size_t maxRowsPerChunk = variableLimit / 2;
  for (uint32_t startIndex = 0; startIndex < aTombstones.Length();
       startIndex += maxRowsPerChunk) {
    size_t rowsPerChunk =
        std::min(maxRowsPerChunk, aTombstones.Length() - startIndex);

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

    nsCOMPtr<mozIStorageStatement> stmt =
        mDB->GetStatement(nsLiteralCString("INSERT INTO moz_bookmarks_deleted "
                                           "(guid, dateRemoved) ") +
                          tombstonesToInsert);
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    uint32_t paramIndex = 0;
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

nsresult nsNavBookmarks::RemoveTombstone(const nsACString& aGUID) {
  nsCOMPtr<mozIStorageStatement> stmt =
      mDB->GetStatement("DELETE FROM moz_bookmarks_deleted WHERE guid = :guid");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindUTF8StringByName("guid"_ns, aGUID);
  NS_ENSURE_SUCCESS(rv, rv);

  return stmt->Execute();
}

NS_IMETHODIMP
nsNavBookmarks::SetItemTitle(int64_t aItemId, const nsACString& aTitle,
                             uint16_t aSource) {
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId = TagsRootId();
  bool isChangingTagFolder = bookmark.parentId == tagsRootId;
  int64_t syncChangeDelta = DetermineSyncChangeDelta(aSource);

  nsAutoCString title;
  TruncateTitle(aTitle, title);

  if (isChangingTagFolder) {
    // If we're changing the title of a tag folder, bump the change counter
    // for all tagged bookmarks. We use a separate code path to avoid a
    // transaction for non-tags.
    mozStorageTransaction transaction(mDB->MainConn(), false);

    // XXX Handle the error, bug 1696133.
    Unused << NS_WARN_IF(NS_FAILED(transaction.Start()));

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

  if (mCanNotify) {
    Sequence<OwningNonNull<PlacesEvent>> events;
    RefPtr<PlacesBookmarkTitle> titleChanged = new PlacesBookmarkTitle();
    titleChanged->mId = bookmark.id;
    titleChanged->mItemType = bookmark.type;
    titleChanged->mUrl.Assign(NS_ConvertUTF8toUTF16(bookmark.url));
    titleChanged->mGuid = bookmark.guid;
    titleChanged->mParentGuid = bookmark.parentGuid;
    titleChanged->mTitle.Assign(NS_ConvertUTF8toUTF16(title));
    titleChanged->mLastModified = bookmark.lastModified / 1000;
    titleChanged->mSource = aSource;
    titleChanged->mIsTagging =
        bookmark.parentId == tagsRootId || bookmark.grandParentId == tagsRootId;
    bool success = !!events.AppendElement(titleChanged.forget(), fallible);
    MOZ_RELEASE_ASSERT(success);
    PlacesObservers::NotifyListeners(events);
  }

  return NS_OK;
}

nsresult nsNavBookmarks::SetItemTitleInternal(BookmarkData& aBookmark,
                                              const nsACString& aTitle,
                                              int64_t aSyncChangeDelta) {
  nsCOMPtr<mozIStorageStatement> statement = mDB->GetStatement(
      "UPDATE moz_bookmarks SET "
      "title = :item_title, lastModified = :date, "
      "syncChangeCounter = syncChangeCounter + :delta "
      "WHERE id = :item_id");
  NS_ENSURE_STATE(statement);
  mozStorageStatementScoper scoper(statement);

  nsresult rv;
  if (aTitle.IsEmpty()) {
    rv = statement->BindNullByName("item_title"_ns);
  } else {
    rv = statement->BindUTF8StringByName("item_title"_ns, aTitle);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  aBookmark.lastModified = RoundToMilliseconds(RoundedPRNow());
  rv = statement->BindInt64ByName("date"_ns, aBookmark.lastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName("item_id"_ns, aBookmark.id);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName("delta"_ns, aSyncChangeDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetItemTitle(int64_t aItemId, nsACString& _title) {
  NS_ENSURE_ARG_MIN(aItemId, 1);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  _title = bookmark.title;
  return NS_OK;
}

nsresult nsNavBookmarks::GetBookmarkURI(int64_t aItemId, nsIURI** _URI) {
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(_URI);

  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aItemId, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewURI(_URI, bookmark.url);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsNavBookmarks::ResultNodeForContainer(
    const nsCString& aGUID, nsNavHistoryQueryOptions* aOptions,
    nsNavHistoryResultNode** aNode) {
  BookmarkData bookmark;
  nsresult rv = FetchItemInfo(aGUID, bookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  if (bookmark.type == TYPE_FOLDER) {  // TYPE_FOLDER
    *aNode =
        new nsNavHistoryFolderResultNode(bookmark.title, aOptions, bookmark.id);
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  (*aNode)->mDateAdded = bookmark.dateAdded;
  (*aNode)->mLastModified = bookmark.lastModified;
  (*aNode)->mBookmarkGuid = bookmark.guid;
  (*aNode)->GetAsFolder()->mTargetFolderGuid = bookmark.guid;

  NS_ADDREF(*aNode);
  return NS_OK;
}

nsresult nsNavBookmarks::QueryFolderChildren(
    int64_t aFolderId, nsNavHistoryQueryOptions* aOptions,
    nsCOMArray<nsNavHistoryResultNode>* aChildren) {
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
      "AND (NOT :excludeItems OR "
      "b.type = :folder OR "
      "h.url_hash BETWEEN hash('place', 'prefix_lo') AND hash('place', "
      "'prefix_hi')) "
      "ORDER BY b.position ASC");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName("parent"_ns, aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName("folder"_ns, TYPE_FOLDER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName("excludeItems"_ns, aOptions->ExcludeItems());
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

nsresult nsNavBookmarks::ProcessFolderNodeRow(
    mozIStorageValueArray* aRow, nsNavHistoryQueryOptions* aOptions,
    nsCOMArray<nsNavHistoryResultNode>* aChildren, int32_t& aCurrentIndex) {
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
    if (nodeType == nsINavHistoryResultNode::RESULT_TYPE_QUERY &&
        aOptions->ExcludeQueries()) {
      return NS_OK;
    }
  } else if (itemType == TYPE_FOLDER) {
    nsAutoCString title;
    bool isNull;
    rv = aRow->GetIsNull(nsNavHistory::kGetInfoIndex_Title, &isNull);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!isNull) {
      rv = aRow->GetUTF8String(nsNavHistory::kGetInfoIndex_Title, title);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Don't use options from the parent to build the new folder node, it will
    // inherit those later when it's inserted in the result.
    node = new nsNavHistoryFolderResultNode(title,
                                            new nsNavHistoryQueryOptions(), id);

    rv = aRow->GetUTF8String(kGetChildrenIndex_Guid, node->mBookmarkGuid);
    NS_ENSURE_SUCCESS(rv, rv);
    node->GetAsFolder()->mTargetFolderGuid = node->mBookmarkGuid;

    rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemDateAdded,
                        reinterpret_cast<int64_t*>(&node->mDateAdded));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aRow->GetInt64(nsNavHistory::kGetInfoIndex_ItemLastModified,
                        reinterpret_cast<int64_t*>(&node->mLastModified));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // This is a separator.
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

nsresult nsNavBookmarks::QueryFolderChildrenAsync(
    nsNavHistoryFolderResultNode* aNode,
    mozIStoragePendingStatement** _pendingStmt) {
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
      "AND (NOT :excludeItems OR "
      "b.type = :folder OR "
      "h.url_hash BETWEEN hash('place', 'prefix_lo') AND hash('place', "
      "'prefix_hi')) "
      "ORDER BY b.position ASC");
  NS_ENSURE_STATE(stmt);

  nsresult rv = stmt->BindInt64ByName("parent"_ns, aNode->mTargetFolderItemId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName("folder"_ns, TYPE_FOLDER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv =
      stmt->BindInt32ByName("excludeItems"_ns, aNode->mOptions->ExcludeItems());
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> pendingStmt;
  rv = stmt->ExecuteAsync(aNode, getter_AddRefs(pendingStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*_pendingStmt = pendingStmt);
  return NS_OK;
}

nsresult nsNavBookmarks::FetchFolderInfo(int64_t aFolderId,
                                         int32_t* _folderCount,
                                         nsACString& _guid,
                                         int64_t* _parentId) {
  *_folderCount = 0;
  *_parentId = -1;

  // This query has to always return results, so it can't be written as a join,
  // though a left join of 2 subqueries would have the same cost.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT count(*), "
      "(SELECT guid FROM moz_bookmarks WHERE id = :parent), "
      "(SELECT parent FROM moz_bookmarks WHERE id = :parent) "
      "FROM moz_bookmarks "
      "WHERE parent = :parent");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName("parent"_ns, aFolderId);
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
nsNavBookmarks::GetFolderIdForItem(int64_t aItemId, int64_t* _parentId) {
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

nsresult nsNavBookmarks::GetBookmarksForURI(
    nsIURI* aURI, nsTArray<BookmarkData>& aBookmarks) {
  NS_ENSURE_ARG(aURI);

  // Double ordering covers possible lastModified ties, that could happen when
  // importing, syncing or due to extensions.
  // Note: not using a JOIN is cheaper in this case.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "/* do not warn (bug 1175249) */ "
      "SELECT b.id, b.guid, b.parent, b.lastModified, t.guid, t.parent, "
      "b.syncStatus "
      "FROM moz_bookmarks b "
      "JOIN moz_bookmarks t on t.id = b.parent "
      "WHERE b.fk = (SELECT id FROM moz_places WHERE url_hash = "
      "hash(:page_url) AND url = :page_url) "
      "ORDER BY b.lastModified DESC, b.id DESC ");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, "page_url"_ns, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t tagsRootId = TagsRootId();

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

    aBookmarks.AppendElement(bookmark);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::AddObserver(nsINavBookmarkObserver* aObserver, bool aOwnsWeak) {
  NS_ENSURE_ARG(aObserver);

  if (NS_WARN_IF(!mCanNotify)) return NS_ERROR_UNEXPECTED;

  return mObservers.AppendWeakElementUnlessExists(aObserver, aOwnsWeak);
}

NS_IMETHODIMP
nsNavBookmarks::RemoveObserver(nsINavBookmarkObserver* aObserver) {
  return mObservers.RemoveWeakElement(aObserver);
}

NS_IMETHODIMP
nsNavBookmarks::GetObservers(
    nsTArray<RefPtr<nsINavBookmarkObserver>>& aObservers) {
  aObservers.Clear();

  if (!mCanNotify) return NS_OK;

  for (uint32_t i = 0; i < mObservers.Length(); ++i) {
    nsCOMPtr<nsINavBookmarkObserver> observer =
        mObservers.ElementAt(i).GetValue();
    // Skip nullified weak observers.
    if (observer) {
      aObservers.AppendElement(observer.forget());
    }
  }

  return NS_OK;
}

void nsNavBookmarks::NotifyItemChanged(const ItemChangeData& aData) {
  // A guid must always be defined.
  MOZ_ASSERT(!aData.bookmark.guid.IsEmpty());
  // No more supported.
  MOZ_ASSERT(!aData.isAnnotation, "Don't notify item annotation changes");
  PRTime lastModified = aData.bookmark.lastModified;
  if (aData.updateLastModified) {
    lastModified = RoundedPRNow();
    MOZ_ALWAYS_SUCCEEDS(SetItemDateInternal(
        LAST_MODIFIED, DetermineSyncChangeDelta(aData.source),
        aData.bookmark.id, lastModified));
  }

  NOTIFY_BOOKMARKS_OBSERVERS(
      mCanNotify, mObservers,
      OnItemChanged(aData.bookmark.id, aData.property, aData.isAnnotation,
                    aData.newValue, lastModified, aData.bookmark.type,
                    aData.bookmark.parentId, aData.bookmark.guid,
                    aData.bookmark.parentGuid, aData.oldValue, aData.source));
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsNavBookmarks::Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* aData) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  if (strcmp(aTopic, TOPIC_PLACES_CONNECTION_CLOSED) == 0) {
    // Don't even try to notify observers from this point on, the category
    // cache would init services that could try to use our APIs.
    mCanNotify = false;
    mObservers.Clear();
  }

  return NS_OK;
}
