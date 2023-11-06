/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNavBookmarks_h_
#define nsNavBookmarks_h_

#include "nsINavBookmarksService.h"
#include "nsNavHistory.h"
#include "nsToolkitCompsCID.h"
#include "nsCategoryCache.h"
#include "nsTHashtable.h"
#include "mozilla/Attributes.h"
#include "prtime.h"

class nsNavBookmarks;

namespace mozilla {
namespace places {

enum BookmarkStatementId {
  DB_FIND_REDIRECTED_BOOKMARK = 0,
  DB_GET_BOOKMARKS_FOR_URI
};

struct BookmarkData {
  int64_t id = -1;
  nsCString url;
  nsCString title;
  int32_t position = -1;
  int64_t placeId = -1;
  int64_t parentId = -1;
  int64_t grandParentId = -1;
  int32_t type = 0;
  int32_t syncStatus = nsINavBookmarksService::SYNC_STATUS_UNKNOWN;
  nsCString serviceCID;
  PRTime dateAdded = 0;
  PRTime lastModified = 0;
  nsCString guid;
  nsCString parentGuid;
};

struct ItemVisitData {
  BookmarkData bookmark;
  int64_t visitId;
  uint32_t transitionType;
  PRTime time;
};

struct TombstoneData {
  nsCString guid;
  PRTime dateRemoved;
};

typedef void (nsNavBookmarks::*ItemVisitMethod)(const ItemVisitData&);

enum BookmarkDate { LAST_MODIFIED };

}  // namespace places
}  // namespace mozilla

class nsNavBookmarks final : public nsINavBookmarksService,
                             public nsIObserver,
                             public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVBOOKMARKSSERVICE
  NS_DECL_NSIOBSERVER

  nsNavBookmarks();

  /**
   * Obtains the service's object.
   */
  static already_AddRefed<nsNavBookmarks> GetSingleton();

  /**
   * Initializes the service's object.  This should only be called once.
   */
  nsresult Init();

  static nsNavBookmarks* GetBookmarksService() {
    if (!gBookmarksService) {
      nsCOMPtr<nsINavBookmarksService> serv =
          do_GetService(NS_NAVBOOKMARKSSERVICE_CONTRACTID);
      NS_ENSURE_TRUE(serv, nullptr);
      NS_ASSERTION(gBookmarksService,
                   "Should have static instance pointer now");
    }
    return gBookmarksService;
  }

  typedef mozilla::places::BookmarkData BookmarkData;
  typedef mozilla::places::ItemVisitData ItemVisitData;
  typedef mozilla::places::BookmarkStatementId BookmarkStatementId;

  nsresult OnVisit(nsIURI* aURI, int64_t aVisitId, PRTime aTime,
                   int64_t aSessionId, int64_t aReferringId,
                   uint32_t aTransitionType, const nsACString& aGUID,
                   bool aHidden, uint32_t aVisitCount, uint32_t aTyped,
                   const nsAString& aLastKnownTitle);

  // Find all the children of a folder, using the given query and options.
  // For each child, a ResultNode is created and added to |children|.
  // The results are ordered by folder position.
  nsresult QueryFolderChildren(int64_t aFolderId,
                               nsNavHistoryQueryOptions* aOptions,
                               nsCOMArray<nsNavHistoryResultNode>* children);

  /**
   * Turns aRow into a node and appends it to aChildren if it is appropriate to
   * do so.
   *
   * @param aRow
   *        A Storage statement (in the case of synchronous execution) or row of
   *        a result set (in the case of asynchronous execution).
   * @param aOptions
   *        The options of the parent folder node. These are the options used
   *        to fill the parent node.
   * @param aChildren
   *        The children of the parent folder node.
   * @param aCurrentIndex
   *        The index of aRow within the results.  When called on the first row,
   *        this should be set to -1.
   */
  nsresult ProcessFolderNodeRow(mozIStorageValueArray* aRow,
                                nsNavHistoryQueryOptions* aOptions,
                                nsCOMArray<nsNavHistoryResultNode>* aChildren,
                                int32_t& aCurrentIndex);

  /**
   * The async version of QueryFolderChildren.
   *
   * @param aNode
   *        The folder node that will receive the children.
   * @param _pendingStmt
   *        The Storage pending statement that will be used to control async
   *        execution.
   */
  nsresult QueryFolderChildrenAsync(nsNavHistoryFolderResultNode* aNode,
                                    mozIStoragePendingStatement** _pendingStmt);

  /**
   * Fetches information about the specified id from the database.
   *
   * @param aItemId
   *        Id of the item to fetch information for.
   * @param aBookmark
   *        BookmarkData to store the information.
   */
  nsresult FetchItemInfo(int64_t aItemId, BookmarkData& _bookmark);

  /**
   * Fetches information about the specified GUID from the database.
   *
   * @param aGUID
   *        GUID of the item to fetch information for.
   * @param aBookmark
   *        BookmarkData to store the information.
   */
  nsresult FetchItemInfo(const nsCString& aGUID, BookmarkData& _bookmark);

  /**
   * Notifies that a bookmark has been visited.
   *
   * @param aItemId
   *        The visited item id.
   * @param aData
   *        Details about the new visit.
   */
  void NotifyItemVisited(const ItemVisitData& aData);

  static const int32_t kGetChildrenIndex_Guid;
  static const int32_t kGetChildrenIndex_Position;
  static const int32_t kGetChildrenIndex_Type;
  static const int32_t kGetChildrenIndex_PlaceID;
  static const int32_t kGetChildrenIndex_SyncStatus;

  static mozilla::Atomic<int64_t> sLastInsertedItemId;
  static void StoreLastInsertedId(const nsACString& aTable,
                                  const int64_t aLastInsertedId);

  static mozilla::Atomic<int64_t> sTotalSyncChanges;
  static void NoteSyncChange();

 private:
  static nsNavBookmarks* gBookmarksService;

  ~nsNavBookmarks();

  nsresult AdjustIndices(int64_t aFolder, int32_t aStartIndex,
                         int32_t aEndIndex, int32_t aDelta);

  nsresult AdjustSeparatorsSyncCounter(int64_t aFolderId, int32_t aStartIndex,
                                       int64_t aSyncChangeDelta);

  /**
   * Fetches properties of a folder.
   *
   * @param aFolderId
   *        Folder to count children for.
   * @param _folderCount
   *        Number of children in the folder.
   * @param _guid
   *        Unique id of the folder.
   * @param _parentId
   *        Id of the parent of the folder.
   *
   * @throws If folder does not exist.
   */
  nsresult FetchFolderInfo(int64_t aFolderId, int32_t* _folderCount,
                           nsACString& _guid, int64_t* _parentId);

  nsresult AddSyncChangesForBookmarksWithURL(const nsACString& aURL,
                                             int64_t aSyncChangeDelta);

  // Bumps the change counter for all bookmarks with |aURI|. This is used to
  // update tagged bookmarks when adding or changing a tag entry.
  nsresult AddSyncChangesForBookmarksWithURI(nsIURI* aURI,
                                             int64_t aSyncChangeDelta);

  // Bumps the change counter for all bookmarked URLs within |aFolderId|. This
  // is used to update tagged bookmarks when changing or removing a tag folder.
  nsresult AddSyncChangesForBookmarksInFolder(int64_t aFolderId,
                                              int64_t aSyncChangeDelta);

  // Inserts a tombstone for a removed synced item.
  nsresult InsertTombstone(const BookmarkData& aBookmark);

  // Inserts tombstones for removed synced items.
  nsresult InsertTombstones(
      const nsTArray<mozilla::places::TombstoneData>& aTombstones);

  // Removes a stale synced bookmark tombstone.
  nsresult RemoveTombstone(const nsACString& aGUID);

  nsresult SetItemTitleInternal(BookmarkData& aBookmark,
                                const nsACString& aTitle,
                                int64_t aSyncChangeDelta);

  /**
   * This is an handle to the Places database.
   */
  RefPtr<mozilla::places::Database> mDB;

  nsresult SetItemDateInternal(enum mozilla::places::BookmarkDate aDateType,
                               int64_t aSyncChangeDelta, int64_t aItemId,
                               PRTime aValue);

  MOZ_CAN_RUN_SCRIPT
  nsresult RemoveFolderChildren(int64_t aFolderId, uint16_t aSource);

  // Recursive method to build an array of folder's children
  nsresult GetDescendantChildren(int64_t aFolderId,
                                 const nsACString& aFolderGuid,
                                 int64_t aGrandParentId,
                                 nsTArray<BookmarkData>& aFolderChildrenArray);

  enum ItemType {
    BOOKMARK = TYPE_BOOKMARK,
    FOLDER = TYPE_FOLDER,
    SEPARATOR = TYPE_SEPARATOR,
  };

  /**
   * Helper to insert a bookmark in the database.
   *
   *  @param aItemId
   *         The itemId to insert, pass -1 to generate a new one.
   *  @param aPlaceId
   *         The placeId to which this bookmark refers to, pass nullptr for
   *         items that don't refer to an URI (eg. folders, separators, ...).
   *  @param aItemType
   *         The type of the new bookmark, see TYPE_* constants.
   *  @param aParentId
   *         The itemId of the parent folder.
   *  @param aIndex
   *         The position inside the parent folder.
   *  @param aTitle
   *         The title for the new bookmark.
   *         Pass a void string to set a NULL title.
   *  @param aDateAdded
   *         The date for the insertion.
   *  @param [optional] aLastModified
   *         The last modified date for the insertion.
   *         It defaults to aDateAdded.
   *
   *  @return The new item id that has been inserted.
   *
   *  @note This will also update last modified date of the parent folder.
   */
  nsresult InsertBookmarkInDB(int64_t aPlaceId, enum ItemType aItemType,
                              int64_t aParentId, int32_t aIndex,
                              const nsACString& aTitle, PRTime aDateAdded,
                              PRTime aLastModified,
                              const nsACString& aParentGuid,
                              int64_t aGrandParentId, nsIURI* aURI,
                              uint16_t aSource, int64_t* _itemId,
                              nsACString& _guid);

  nsresult GetBookmarksForURI(nsIURI* aURI, nsTArray<BookmarkData>& _bookmarks);

  // Used to enable and disable the observer notifications.
  bool mCanNotify;
};

#endif  // nsNavBookmarks_h_
