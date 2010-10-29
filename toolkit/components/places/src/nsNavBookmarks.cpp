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

#define BOOKMARKS_TO_KEYWORDS_INITIAL_CACHE_SIZE 64

const PRInt32 nsNavBookmarks::kFindBookmarksIndex_ID = 0;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Type = 1;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_PlaceID = 2;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Parent = 3;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Position = 4;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Title = 5;

// These columns sit to the right of the kGetInfoIndex_* columns.
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Position = 13;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Type = 14;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_PlaceID = 15;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_ServiceContractId = 16;

const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_ID = 0;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_URI = 1;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_Title = 2;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_Position = 3;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_PlaceID = 4;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_Parent = 5;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_Type = 6;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_ServiceContractId = 7;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_DateAdded = 8;
const PRInt32 nsNavBookmarks::kGetItemPropertiesIndex_LastModified = 9;

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

} // Anonymous namespace.


nsNavBookmarks::nsNavBookmarks() : mItemCount(0)
                                 , mRoot(0)
                                 , mBookmarksRoot(0)
                                 , mTagRoot(0)
                                 , mToolbarFolder(0)
                                 , mBatchLevel(0)
                                 , mBatchDBTransaction(nsnull)
                                 , mCanNotify(false)
                                 , mCacheObservers("bookmark-observers")
                                 , mShuttingDown(false)
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


NS_IMPL_ISUPPORTS3(nsNavBookmarks,
                   nsINavBookmarksService,
                   nsINavHistoryObserver,
                   nsIAnnotationObserver)


nsresult
nsNavBookmarks::Init()
{
  NS_TIME_FUNCTION;

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  mDBConn = history->GetStorageConnection();
  NS_ENSURE_STATE(mDBConn);

  // TODO: we could consider roots changes as schema changes, and init them
  // only if the database has been created/updated, history.databaseStatus
  // can tell us that.
  nsresult rv = InitRoots();
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
  PRBool exists;
  nsresult rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
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
  }

  // moz_bookmarks_roots
  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_roots"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    rv = aDBConn->ExecuteSimpleSQL(CREATE_MOZ_BOOKMARKS_ROOTS);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // moz_keywords
  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_keywords"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
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
  RETURN_IF_STMT(mDBFindURIBookmarks, NS_LITERAL_CSTRING(
    "SELECT b.id "
    "FROM moz_bookmarks b "
    "WHERE b.type = :item_type AND b.fk = ( "
      "SELECT id FROM moz_places_temp "
      "WHERE url = :page_url "
      "UNION "
      "SELECT id FROM moz_places "
      "WHERE url = :page_url "
      "LIMIT 1 "
    ") "
    "ORDER BY b.lastModified DESC, b.id DESC "));

  // Select all children of a given folder, sorted by position.
  // This is a LEFT OUTER JOIN with moz_places since folders does not have
  // a reference into that table.
  // We construct a result where the first columns exactly match those returned
  // by mDBGetURLPageInfo, and additionally contains columns for position,
  // item_child, and folder_child from moz_bookmarks.
  RETURN_IF_STMT(mDBGetChildren, NS_LITERAL_CSTRING(
    "SELECT IFNULL(h_t.id, h.id), IFNULL(h_t.url, h.url), "
           "COALESCE(b.title, h_t.title, h.title), "
           "IFNULL(h_t.rev_host, h.rev_host), "
           "IFNULL(h_t.visit_count, h.visit_count), "
           "IFNULL(h_t.last_visit_date, h.last_visit_date), "
           "f.url, null, b.id, b.dateAdded, b.lastModified, b.parent, null, "
           "b.position, b.type, b.fk, b.folder_type "
    "FROM moz_bookmarks b "
    "LEFT JOIN moz_places_temp h_t ON b.fk = h_t.id "
    "LEFT JOIN moz_places h ON b.fk = h.id "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE b.parent = :parent "
    "ORDER BY b.position ASC"));

  // Count all of the children of a given folder and checks that it exists.
  RETURN_IF_STMT(mDBFolderCount, NS_LITERAL_CSTRING(
    "SELECT COUNT(*), "
    "(SELECT id FROM moz_bookmarks WHERE id = :parent) "
    "FROM moz_bookmarks WHERE parent = :parent"));

  RETURN_IF_STMT(mDBGetChildAt, NS_LITERAL_CSTRING(
    "SELECT id, fk, type FROM moz_bookmarks "
    "WHERE parent = :parent AND position = :item_index"));

  // Get bookmark/folder/separator properties.
  RETURN_IF_STMT(mDBGetItemProperties, NS_LITERAL_CSTRING(
    "SELECT b.id, "
           "IFNULL((SELECT url FROM moz_places_temp WHERE id = b.fk), "
                  "(SELECT url FROM moz_places WHERE id = b.fk)), "
           "b.title, b.position, b.fk, b.parent, b.type, b.folder_type, "
           "b.dateAdded, b.lastModified "
    "FROM moz_bookmarks b "
    "WHERE b.id = :item_id"));

  RETURN_IF_STMT(mDBGetItemIdForGUID, NS_LITERAL_CSTRING(
    "SELECT item_id FROM moz_items_annos "
    "WHERE content = :guid "
    "LIMIT 1"));

  RETURN_IF_STMT(mDBInsertBookmark, NS_LITERAL_CSTRING(
    "INSERT INTO moz_bookmarks "
      "(id, fk, type, parent, position, title, folder_type, "
       "dateAdded, lastModified) "
    "VALUES (:item_id, :page_id, :item_type, :parent, :item_index, "
            ":item_title, :folder_type, :date_added, :last_modified)"));

  // Just select position since it's just an int32 and may be faster.
  // We don't actually care about the data, just whether there is any.
  RETURN_IF_STMT(mDBIsBookmarkedInDatabase, NS_LITERAL_CSTRING(
    "SELECT 1 FROM moz_bookmarks WHERE fk = :page_id"));

  RETURN_IF_STMT(mDBIsURIBookmarkedInDatabase, NS_LITERAL_CSTRING(
    "SELECT 1 FROM moz_bookmarks WHERE fk = ("
      "SELECT id FROM moz_places_temp WHERE url = :page_url "
      "UNION ALL "
      "SELECT id FROM moz_places WHERE url = :page_url "
      "LIMIT 1"
    ")"));

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
      ") "
    "LIMIT 1"));

  RETURN_IF_STMT(mDBGetLastBookmarkID, NS_LITERAL_CSTRING(
    "SELECT id "
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

  // Get keyword text for bookmarked URI.
  RETURN_IF_STMT(mDBGetKeywordForURI, NS_LITERAL_CSTRING(
    "SELECT k.keyword "
    "FROM ( "
      "SELECT id FROM moz_places_temp "
      "WHERE url = :page_url "
      "UNION ALL "
      "SELECT id FROM moz_places "
      "WHERE url = :page_url "
      "LIMIT 1 "
    ") AS h "
    "JOIN moz_bookmarks b ON b.fk = h.id "
    "JOIN moz_keywords k ON k.id = b.keyword_id"));

  RETURN_IF_STMT(mDBAdjustPosition, NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks SET position = position + :delta "
    "WHERE parent = :parent "
      "AND position >= :from_index AND position <= :to_index"));

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
  // Moreover this query does not mix-up all possible cases of disk and temp
  // tables.  This is because we expect a redirects chain to be completely on
  // disk or completely in memory.  We never bring back visits from disk to
  // memory, we sync visits on a timer (the chained visits have narrow times),
  // or on bookmarks changes.  The likely possiblity that we break a chain in
  // the middle is so much smaller than the perf and readability hit we would
  // get making complete crossing joins.
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
    "SELECT IFNULL( "
      "(SELECT url FROM moz_places_temp WHERE id = :page_id), "
      "(SELECT url FROM moz_places WHERE id = :page_id) "
    ") "
    "FROM moz_bookmarks b "
    "WHERE b.fk = :page_id "
    "UNION ALL " // Not directly bookmarked.
    "SELECT IFNULL( "
      "(SELECT url FROM moz_places_temp WHERE id = " COALESCE_PLACEID "), "
      "(SELECT url FROM moz_places WHERE id = " COALESCE_PLACEID ") "
    ") "
    "FROM moz_historyvisits_temp self "
    "JOIN moz_bookmarks b ON b.fk = " COALESCE_PLACEID
    "LEFT JOIN moz_historyvisits_temp parent ON parent.id = self.from_visit "
    "LEFT JOIN moz_historyvisits_temp grandparent ON parent.from_visit = grandparent.id "
      "AND parent.visit_type IN (") + redirectsFragment + NS_LITERAL_CSTRING(") "
    "LEFT JOIN moz_historyvisits_temp greatgrandparent ON grandparent.from_visit = greatgrandparent.id "
      "AND grandparent.visit_type IN (") + redirectsFragment + NS_LITERAL_CSTRING(") "
    "WHERE self.visit_type IN (") + redirectsFragment + NS_LITERAL_CSTRING(") "
      "AND self.place_id = :page_id "
    "UNION ALL " // Not in the temp table.
    "SELECT IFNULL( "
      "(SELECT url FROM moz_places_temp WHERE id = " COALESCE_PLACEID "), "
      "(SELECT url FROM moz_places WHERE id = " COALESCE_PLACEID ") "
    ") "
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
    mDBFolderCount,
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

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(stmts); i++) {
    nsresult rv = nsNavHistory::FinalizeStatement(stmts[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsNavBookmarks::InitRoots
//
//    This locates and creates if necessary the root items in the bookmarks
//    folder hierarchy. These items are stored in a special roots table that
//    maps short predefined names to folder IDs.
//
//    Normally, these folders will exist already and we will save their IDs
//    which are exposed through the bookmark service interface.
//
//    If the root does not exist, a folder is created for it and the ID is
//    saved in the root table. No user-visible name is given to these folders
//    and they have no parent or other attributes.
//
//    These attributes are set when the default_places.html file is imported.
//    It defines the hierarchy, and has special attributes that tell us when
//    a folder is one of our well-known roots. We then insert the root in the
//    defined point in the hierarchy and set its attributes from this.
//
//    This should be called as the last part of the init process so that
//    all of the statements are set up and the service is ready to use.

nsresult
nsNavBookmarks::InitRoots()
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  nsCOMPtr<mozIStorageStatement> getRootStatement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT folder_id FROM moz_bookmarks_roots WHERE root_name = :root_name"),
    getter_AddRefs(getRootStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool createdPlacesRoot = PR_FALSE;
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("places"),
                  &mRoot, 0, &createdPlacesRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("menu"),
                  &mBookmarksRoot, mRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool createdToolbarFolder;
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("toolbar"),
                  &mToolbarFolder, mRoot, &createdToolbarFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  // Once toolbar was not a root, we may need to move over the items and
  // delete the custom folder
  if (!createdPlacesRoot && createdToolbarFolder) {
    nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
    NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);

    nsTArray<PRInt64> folders;
    rv = annosvc->GetItemsWithAnnotationTArray(BOOKMARKS_TOOLBAR_FOLDER_ANNO,
                                               &folders);
    NS_ENSURE_SUCCESS(rv, rv);
    if (folders.Length() > 0) {
      nsCOMPtr<mozIStorageStatement> moveItems;
      rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "UPDATE moz_bookmarks SET parent = :new_parent "
        "WHERE parent = :old_parent"
      ), getter_AddRefs(moveItems));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = moveItems->BindInt64ByName(NS_LITERAL_CSTRING("new_parent"),
                                      mToolbarFolder);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = moveItems->BindInt64ByName(NS_LITERAL_CSTRING("old_parent"),
                                      folders[0]);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = moveItems->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
      rv = RemoveFolder(folders[0]);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("tags"),
                  &mTagRoot, mRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("unfiled"),
                  &mUnfiledRoot, mRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set titles for special folders
  // We cannot rely on createdPlacesRoot due to Fx3beta->final migration path
  PRUint16 databaseStatus = nsINavHistoryService::DATABASE_STATUS_OK;
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  rv = history->GetDatabaseStatus(&databaseStatus);
  if (NS_FAILED(rv) ||
      databaseStatus != nsINavHistoryService::DATABASE_STATUS_OK) {
    rv = InitDefaults();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavBookmarks::InitDefaults
//
// Initializes default bookmarks and containers.
// Pulls from places.propertes for l10n.
// Replaces the old default_places.html file.
nsresult
nsNavBookmarks::InitDefaults()
{
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  nsIStringBundle* bundle = history->GetBundle();
  NS_ENSURE_TRUE(bundle, NS_ERROR_OUT_OF_MEMORY);

  // Bookmarks Menu
  nsXPIDLString bookmarksTitle;
  nsresult rv = bundle->GetStringFromName(
    NS_LITERAL_STRING("BookmarksMenuFolderTitle").get(),
    getter_Copies(bookmarksTitle));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetItemTitle(mBookmarksRoot, NS_ConvertUTF16toUTF8(bookmarksTitle));
  NS_ENSURE_SUCCESS(rv, rv);

  // Bookmarks Toolbar
  nsXPIDLString toolbarTitle;
  rv = bundle->GetStringFromName(
    NS_LITERAL_STRING("BookmarksToolbarFolderTitle").get(),
    getter_Copies(toolbarTitle));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetItemTitle(mToolbarFolder, NS_ConvertUTF16toUTF8(toolbarTitle));
  NS_ENSURE_SUCCESS(rv, rv);

  // Unsorted Bookmarks
  nsXPIDLString unfiledTitle;
  rv = bundle->GetStringFromName(
    NS_LITERAL_STRING("UnsortedBookmarksFolderTitle").get(),
    getter_Copies(unfiledTitle));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetItemTitle(mUnfiledRoot, NS_ConvertUTF16toUTF8(unfiledTitle));
  NS_ENSURE_SUCCESS(rv, rv);

  // Tags
  nsXPIDLString tagsTitle;
  rv = bundle->GetStringFromName(
    NS_LITERAL_STRING("TagsFolderTitle").get(),
    getter_Copies(tagsTitle));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetItemTitle(mTagRoot, NS_ConvertUTF16toUTF8(tagsTitle));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavBookmarks::CreateRoot
//
//    This gets or creates a root folder of the given type. aWasCreated
//    (optional) is true if the folder had to be created, false if we just used
//    an old one. The statement that gets a folder ID from a root name is
//    passed in so the DB only needs to parse the statement once, and we don't
//    have to have a global for this. Creation is less optimized because it
//    happens rarely.

nsresult
nsNavBookmarks::CreateRoot(mozIStorageStatement* aGetRootStatement,
                           const nsCString& name, PRInt64* aID,
                           PRInt64 aParentID, PRBool* aWasCreated)
{
  NS_ENSURE_STATE(aGetRootStatement);
  mozStorageStatementScoper scoper(aGetRootStatement);
  PRBool hasResult = PR_FALSE;
  nsresult rv = aGetRootStatement->BindUTF8StringByName(NS_LITERAL_CSTRING("root_name"), name);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aGetRootStatement->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasResult) {
    if (aWasCreated)
      *aWasCreated = PR_FALSE;
    rv = aGetRootStatement->GetInt64(0, aID);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(*aID != 0, "Root is 0 for some reason, folders can't have 0 ID");
    return NS_OK;
  }
  if (aWasCreated)
    *aWasCreated = PR_TRUE;

  // create folder with no name or attributes
  nsCOMPtr<mozIStorageStatement> insertStatement;
  rv = CreateFolder(aParentID, EmptyCString(), nsINavBookmarksService::DEFAULT_INDEX, aID);
  NS_ENSURE_SUCCESS(rv, rv);

  // save root ID
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_bookmarks_roots (root_name, folder_id) "
    "VALUES (:root_name, :item_id)"
  ), getter_AddRefs(insertStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = insertStatement->BindUTF8StringByName(NS_LITERAL_CSTRING("root_name"), name);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = insertStatement->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), *aID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = insertStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


PRBool
nsNavBookmarks::IsRealBookmark(PRInt64 aPlaceId)
{
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBIsRealBookmark);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlaceId);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Binding failed");
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_type"), TYPE_BOOKMARK);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Binding failed");
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"),
                                  NS_LITERAL_CSTRING(LMANNO_FEEDURI));
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Binding failed");

  // If we get any rows, then there exists at least one bookmark corresponding
  // to aPlaceId that is not a livemark item.
  PRBool isBookmark;
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
                                       PRBool* aIsBookmarked)
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
  *aRoot = mBookmarksRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetToolbarFolder(PRInt64* aFolderId)
{
  *aFolderId = mToolbarFolder;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetTagsFolder(PRInt64* aRoot)
{
  *aRoot = mTagRoot;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetUnfiledBookmarksFolder(PRInt64* aRoot)
{
  *aRoot = mUnfiledRoot;
  return NS_OK;
}


nsresult
nsNavBookmarks::InsertBookmarkInDB(PRInt64 aItemId,
                                   PRInt64 aPlaceId,
                                   enum ItemType aItemType,
                                   PRInt64 aParentId,
                                   PRInt32 aIndex,
                                   const nsACString& aTitle,
                                   PRTime aDateAdded,
                                   PRTime aLastModified,
                                   const nsAString& aServiceContractId,
                                   PRInt64* _newItemId)
{
  NS_ASSERTION(_newItemId, "Null pointer passed to InsertBookmarkInDB!");

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBInsertBookmark);
  nsresult rv;
  if (aItemId && aItemId != -1)
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  else
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("item_id"));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aPlaceId && aPlaceId != -1)
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

  if (!aItemId || aItemId == -1) {
    // Get the new inserted item id.
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(lastInsertIdStmt, mDBGetLastBookmarkID);
    PRBool hasResult;
    rv = lastInsertIdStmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(hasResult, NS_ERROR_UNEXPECTED);
    rv = lastInsertIdStmt->GetInt64(0, _newItemId);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    *_newItemId = aItemId;
  }

  // Update last modified date of the parent folder.
  // XXX TODO: This should be done recursively for all ancestors, that would
  //           be slow without a nested tree though.  See bug 408991.
  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           aParentId, aDateAdded);
  NS_ENSURE_SUCCESS(rv, rv);

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

  // You can pass -1 to indicate append, but no other negative number is allowed
  if (aIndex < nsINavBookmarksService::DEFAULT_INDEX)
    return NS_ERROR_INVALID_ARG;

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  // This is really a place ID
  PRInt64 childID;
  nsresult rv = history->GetUrlIdFor(aURI, &childID, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 index;
  PRInt32 folderCount;
  rv = FolderCount(aFolder, &folderCount);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      aIndex >= folderCount) {
    index = folderCount;
  }
  else {
    index = aIndex;
    rv = AdjustIndices(aFolder, index, PR_INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = InsertBookmarkInDB(-1, childID, BOOKMARK, aFolder, index,
                          aTitle, PR_Now(), nsnull, EmptyString(),
                          aNewBookmarkId);
  NS_ENSURE_SUCCESS(rv, rv);

  // XXX
  // 0n import / fx 2 migration, is the frecency work going to slow us down?
  // We might want to skip this stuff, as well as the frecency work
  // caused by GetUrlIdFor() which calls InternalAddNewPage().
  // If we do skip this, after import, we will
  // need to call FixInvalidFrecenciesForExcludedPlaces().
  // We might need to call it anyways, if items aren't properly annotated
  // as livemarks feeds yet.

  nsCAutoString url;
  rv = aURI->GetSpec(url);
  NS_ENSURE_SUCCESS(rv, rv);

  // prevent place: queries from showing up in the URL bar autocomplete results
  PRBool isBookmark = !IsQueryURI(url);

  if (isBookmark) {
    // if it is a livemark item (the parent is a livemark), 
    // we pass in false for isBookmark.  otherwise, unvisited livemark 
    // items will appear in URL autocomplete before we visit them.
    PRBool parentIsLivemark;
    nsCOMPtr<nsILivemarkService> lms = 
      do_GetService(NS_LIVEMARKSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = lms->IsLivemark(aFolder, &parentIsLivemark);
    NS_ENSURE_SUCCESS(rv, rv);
 
    isBookmark = !parentIsLivemark;
  }
  
  // when we created the moz_place entry for the new bookmark 
  // (a side effect of calling GetUrlIdFor()) frecency -1;
  // now we re-calculate the frecency for this moz_place entry. 
  rv = history->UpdateFrecency(childID, isBookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemAdded(*aNewBookmarkId, aFolder, index, TYPE_BOOKMARK,
                               aURI));

  // If the bookmark has been added to a tag container, notify all
  // bookmark-folder result nodes which contain a bookmark for the new
  // bookmark's url
  PRInt64 grandParentId;
  rv = GetFolderIdForItem(aFolder, &grandParentId);
  NS_ENSURE_SUCCESS(rv, rv);
  if (grandParentId == mTagRoot) {
    // query for all bookmarks for that URI, notify for each
    nsTArray<PRInt64> bookmarks;
    rv = GetBookmarkIdsForURITArray(aURI, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    if (bookmarks.Length()) {
      for (PRUint32 i = 0; i < bookmarks.Length(); i++) {
        NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                         nsINavBookmarkObserver,
                         OnItemChanged(bookmarks[i], NS_LITERAL_CSTRING("tags"),
                                       PR_FALSE, EmptyCString(), 0,
                                       TYPE_BOOKMARK));
      }
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RemoveItem(PRInt64 aItemId)
{
  NS_ENSURE_TRUE(aItemId != mRoot, NS_ERROR_INVALID_ARG);

  nsresult rv;
  PRInt32 childIndex;
  PRInt64 placeId, folderId;
  PRInt32 itemType;
  nsCAutoString buffer;
  nsCAutoString spec;

  { // scoping to ensure the statement gets reset
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(getInfoStmt, mDBGetItemProperties);
    rv = getInfoStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = getInfoStmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasResult)
      return NS_ERROR_INVALID_ARG; // invalid bookmark id

    rv = getInfoStmt->GetInt32(kGetItemPropertiesIndex_Position, &childIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = getInfoStmt->GetInt64(kGetItemPropertiesIndex_PlaceID, &placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = getInfoStmt->GetInt64(kGetItemPropertiesIndex_Parent, &folderId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = getInfoStmt->GetInt32(kGetItemPropertiesIndex_Type, &itemType);
    NS_ENSURE_SUCCESS(rv, rv);
    if (itemType == TYPE_BOOKMARK) {
      rv = getInfoStmt->GetUTF8String(kGetItemPropertiesIndex_URI, spec);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (itemType == TYPE_FOLDER) {
    rv = RemoveFolder(aItemId);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnBeforeItemRemoved(aItemId, itemType));

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // First, remove item annotations
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  rv = annosvc->RemoveItemAnnotations(aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  {
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBRemoveItem);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (childIndex != -1) {
    rv = AdjustIndices(folderId, childIndex + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           folderId, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  if (itemType == TYPE_BOOKMARK) {
    // UpdateFrecency needs to know whether placeId is still bookmarked.
    // Although we removed aItemId, placeId may still be bookmarked elsewhere;
    // IsRealBookmark will know.
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    rv = history->UpdateFrecency(placeId, IsRealBookmark(placeId));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = UpdateKeywordsHashForRemovedBookmark(aItemId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemRemoved(aItemId, folderId, childIndex, itemType));

  if (itemType == TYPE_BOOKMARK) {
    // If the removed bookmark was a child of a tag container, notify all
    // bookmark-folder result nodes which contain a bookmark for the removed
    // bookmark's url.
    PRInt64 grandParentId;
    rv = GetFolderIdForItem(folderId, &grandParentId);
    NS_ENSURE_SUCCESS(rv, rv);
    if (grandParentId == mTagRoot) {
      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), spec);
      NS_ENSURE_SUCCESS(rv, rv);
      nsTArray<PRInt64> bookmarks;

      rv = GetBookmarkIdsForURITArray(uri, bookmarks);
      NS_ENSURE_SUCCESS(rv, rv);

      if (bookmarks.Length()) {
        for (PRUint32 i = 0; i < bookmarks.Length(); i++) {
          NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                           nsINavBookmarkObserver,
                           OnItemChanged(bookmarks[i],
                                         NS_LITERAL_CSTRING("tags"), PR_FALSE,
                                         EmptyCString(), 0, TYPE_BOOKMARK));
        }
      }
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
nsNavBookmarks::GetFolderReadonly(PRInt64 aFolder, PRBool* aResult)
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
nsNavBookmarks::SetFolderReadonly(PRInt64 aFolder, PRBool aReadOnly)
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
    PRBool hasAnno;
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
                                      const nsACString& aName,
                                      const nsAString& aContractId,
                                      PRBool aIsBookmarkFolder,
                                      PRInt32* aIndex,
                                      PRInt64* aNewFolder)
{
  // You can pass -1 to indicate append, but no other negative number is allowed
  if (*aIndex < -1)
    return NS_ERROR_INVALID_ARG;

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  PRInt32 index;
  PRInt32 folderCount;
  nsresult rv = FolderCount(aParent, &folderCount);
  NS_ENSURE_SUCCESS(rv, rv);
  if (*aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      *aIndex >= folderCount) {
    index = folderCount;
  } else {
    index = *aIndex;
    rv = AdjustIndices(aParent, index, PR_INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  ItemType containerType = aIsBookmarkFolder ? FOLDER
                                             : DYNAMIC_CONTAINER;
  rv = InsertBookmarkInDB(aItemId, nsnull, containerType, aParent, index,
                          aName, PR_Now(), nsnull, aContractId, aNewFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemAdded(*aNewFolder, aParent, index, containerType,
                               nsnull));

  *aIndex = index;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::InsertSeparator(PRInt64 aParent,
                                PRInt32 aIndex,
                                PRInt64* aNewItemId)
{
  NS_ENSURE_ARG_MIN(aParent, 1);
  // -1 means "append", but no other negative value is allowed.
  NS_ENSURE_ARG_MIN(aIndex, -1);
  NS_ENSURE_ARG_POINTER(aNewItemId);

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  PRInt32 index;
  PRInt32 folderCount;
  nsresult rv = FolderCount(aParent, &folderCount);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      aIndex >= folderCount) {
    index = folderCount;
  }
  else {
    index = aIndex;
    rv = AdjustIndices(aParent, index, PR_INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set a NULL title, not an empty title.
  nsCString voidString;
  voidString.SetIsVoid(PR_TRUE);
  rv = InsertBookmarkInDB(-1, nsnull, SEPARATOR, aParent, index,
                          voidString, PR_Now(), nsnull, EmptyString(),
                          aNewItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemAdded(*aNewItemId, aParent, index, TYPE_SEPARATOR,
                               nsnull));

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
  PRBool found;
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

    PRBool found;
    rv = stmt->ExecuteStep(&found);
    NS_ENSURE_SUCCESS(rv, rv);
    if (found) {
      rv = stmt->GetInt64(0, aItemId);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}


nsresult 
nsNavBookmarks::GetParentAndIndexOfFolder(PRInt64 aFolderId,
                                          PRInt64* _aParent,
                                          PRInt32* _aIndex)
{
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(hasResult, NS_ERROR_INVALID_ARG);

  rv = stmt->GetInt64(kGetItemPropertiesIndex_Parent, _aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt32(kGetItemPropertiesIndex_Position, _aIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsNavBookmarks::RemoveFolder(PRInt64 aFolderId)
{
  NS_ENSURE_TRUE(aFolderId != mRoot, NS_ERROR_INVALID_ARG);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnBeforeItemRemoved(aFolderId, TYPE_FOLDER));

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  nsresult rv;
  PRInt64 parent;
  PRInt32 index, type;
  nsCAutoString folderType;
  {
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(getInfoStmt, mDBGetItemProperties);
    rv = getInfoStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aFolderId);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = getInfoStmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasResult) {
      return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
    }

    rv = getInfoStmt->GetInt32(kGetItemPropertiesIndex_Type, &type);
    NS_ENSURE_SUCCESS(rv, rv);
     rv = getInfoStmt->GetInt64(kGetItemPropertiesIndex_Parent, &parent);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = getInfoStmt->GetInt32(kGetItemPropertiesIndex_Position, &index);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = getInfoStmt->GetUTF8String(kGetItemPropertiesIndex_ServiceContractId,
                                    folderType);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Ensure this is really a folder.
  NS_ENSURE_TRUE(type == TYPE_FOLDER, NS_ERROR_INVALID_ARG);

  // First, remove item annotations
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  rv = annosvc->RemoveItemAnnotations(aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  // If this is a container bookmark, try to notify its service.
  if (folderType.Length() > 0) {
    // There is a type associated with this folder.
    nsCOMPtr<nsIDynamicContainer> bmcServ = do_GetService(folderType.get());
    if (bmcServ) {
      rv = bmcServ->OnContainerRemoving(aFolderId);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                       "Remove folder container notification failed.");
    }
  }

  // Remove all of the folder's children
  rv = RemoveFolderChildren(aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  {
    // Remove the folder from its parent.
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBRemoveItem);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aFolderId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = AdjustIndices(parent, index + 1, PR_INT32_MAX, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           parent, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  if (aFolderId == mToolbarFolder) {
    mToolbarFolder = 0;
  }

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemRemoved(aFolderId, parent, index, TYPE_FOLDER));

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
                                      PRInt64 aGrandParentId,
                                      nsTArray<folderChildrenInfo>& aFolderChildrenArray) {
  // New children will be added from this index on.
  PRUint32 startIndex = aFolderChildrenArray.Length();
  nsresult rv;
  {
    // Collect children informations.
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetChildren);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    while (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
      folderChildrenInfo child;
      rv = stmt->GetInt64(nsNavHistory::kGetInfoIndex_ItemId, &child.itemId);
      NS_ENSURE_SUCCESS(rv, rv);
      child.parentId = aFolderId;
      child.grandParentId = aGrandParentId;
      PRInt32 itemType;
      rv = stmt->GetInt32(kGetChildrenIndex_Type, &itemType);
      child.itemType = (PRUint16)itemType;
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->GetInt64(kGetChildrenIndex_PlaceID, &child.placeId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->GetInt32(kGetChildrenIndex_Position, &child.index);
      NS_ENSURE_SUCCESS(rv, rv);

      if (child.itemType == TYPE_BOOKMARK) {
        nsCAutoString URIString;
        rv = stmt->GetUTF8String(nsNavHistory::kGetInfoIndex_URL, URIString);
        NS_ENSURE_SUCCESS(rv, rv);
        child.url = URIString;
      }
      else if (child.itemType == TYPE_FOLDER) {
        nsCAutoString folderType;
        rv = stmt->GetUTF8String(kGetChildrenIndex_ServiceContractId,
                                 folderType);
        NS_ENSURE_SUCCESS(rv, rv);
        child.folderType = folderType;
      }
      // Append item to children's array.
      aFolderChildrenArray.AppendElement(child);
    }
  }

  // Recursively call GetDescendantChildren for added folders.
  // We start at startIndex since previous folders are checked
  // by previous calls to this method.
  PRUint32 childCount = aFolderChildrenArray.Length();
  for (PRUint32 i = startIndex; i < childCount; i++) {
    if (aFolderChildrenArray[i].itemType == TYPE_FOLDER) {
      GetDescendantChildren(aFolderChildrenArray[i].itemId,
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

  nsresult rv;
  PRInt32 itemType;
  PRInt64 grandParentId;
  {
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(getInfoStmt, mDBGetItemProperties);
    rv = getInfoStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aFolderId);
    NS_ENSURE_SUCCESS(rv, rv);

    // Sanity check: ensure that item exists.
    PRBool folderExists;
    if (NS_FAILED(getInfoStmt->ExecuteStep(&folderExists)) || !folderExists)
      return NS_ERROR_INVALID_ARG;

    // Sanity check: ensure that this is a folder.
    rv = getInfoStmt->GetInt32(kGetItemPropertiesIndex_Type, &itemType);
    NS_ENSURE_SUCCESS(rv, rv);
    if (itemType != TYPE_FOLDER)
      return NS_ERROR_INVALID_ARG;

    // Get the grandParent.
    // We have to do this only once since recursion will give us other
    // grandParents without the need of additional queries.
    rv = getInfoStmt->GetInt64(kGetItemPropertiesIndex_Parent, &grandParentId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Fill folder children array recursively.
  nsTArray<folderChildrenInfo> folderChildrenArray;
  rv = GetDescendantChildren(aFolderId, grandParentId, folderChildrenArray);
  NS_ENSURE_SUCCESS(rv, rv);

  // Build a string of folders whose children will be removed.
  nsCString foldersToRemove;
  for (PRUint32 i = 0; i < folderChildrenArray.Length(); i++) {
    folderChildrenInfo child = folderChildrenArray[i];

    // Notify observers that we are about to remove this child.
    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavBookmarkObserver,
                     OnBeforeItemRemoved(child.itemId, child.itemType));

    if (child.itemType == TYPE_FOLDER) {
      foldersToRemove.AppendLiteral(",");
      foldersToRemove.AppendInt(child.itemId);

      // If this is a dynamic container, try to notify its service that we
      // are going to remove it.
      // XXX (bug 484094) this should use a bookmark observer!
      if (child.folderType.Length() > 0) {
        nsCOMPtr<nsIDynamicContainer> bmcServ =
          do_GetService(child.folderType.get());
        if (bmcServ) {
          rv = bmcServ->OnContainerRemoving(child.itemId);
          if (NS_FAILED(rv))
            NS_WARNING("Remove folder container notification failed.");
        }
      }
    }
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
  rv = deleteStatement->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
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
                           aFolderId, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < folderChildrenArray.Length(); i++) {
    folderChildrenInfo child = folderChildrenArray[i];
    if (child.itemType == TYPE_BOOKMARK) {
      PRInt64 placeId = child.placeId;

      // UpdateFrecency needs to know whether placeId is still bookmarked.
      // Although we removed a child of aFolderId that bookmarked it, it may
      // still be bookmarked elsewhere; IsRealBookmark will know.
      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
      rv = history->UpdateFrecency(placeId, IsRealBookmark(placeId));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = UpdateKeywordsHashForRemovedBookmark(child.itemId);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // Call observers in reverse order to serve children before their parent.
  for (PRInt32 i = folderChildrenArray.Length() - 1; i >= 0 ; i--) {
    folderChildrenInfo child = folderChildrenArray[i];

    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavBookmarkObserver,
                     OnItemRemoved(child.itemId, child.parentId, child.index,
                                   child.itemType));

    if (child.itemType == TYPE_BOOKMARK) {
      // If the removed bookmark was a child of a tag container, notify all
      // bookmark-folder result nodes which contain a bookmark for the removed
      // bookmark's url.

      if (child.grandParentId == mTagRoot) {
        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), child.url);
        NS_ENSURE_SUCCESS(rv, rv);

        nsTArray<PRInt64> bookmarks;
        rv = GetBookmarkIdsForURITArray(uri, bookmarks);
        NS_ENSURE_SUCCESS(rv, rv);

        if (bookmarks.Length()) {
          for (PRUint32 i = 0; i < bookmarks.Length(); i++) {
            NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                             nsINavBookmarkObserver,
                             OnItemChanged(bookmarks[i],
                                           NS_LITERAL_CSTRING("tags"), PR_FALSE,
                                           EmptyCString(), 0, TYPE_BOOKMARK));
          }
        }
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

  // get item properties
  nsresult rv;
  PRInt64 oldParent;
  PRInt32 oldIndex;
  PRInt32 itemType;
  nsCAutoString folderType;
  {
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(getInfoStmt, mDBGetItemProperties);
    rv = getInfoStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = getInfoStmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasResult) {
      return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
    }

    rv = getInfoStmt->GetInt64(kGetItemPropertiesIndex_Parent, &oldParent);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = getInfoStmt->GetInt32(kGetItemPropertiesIndex_Position, &oldIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = getInfoStmt->GetInt32(kGetItemPropertiesIndex_Type, &itemType);
    NS_ENSURE_SUCCESS(rv, rv);
    if (itemType == TYPE_FOLDER) {
      rv = getInfoStmt->GetUTF8String(kGetItemPropertiesIndex_ServiceContractId,
                                      folderType);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // if parent and index are the same, nothing to do
  if (oldParent == aNewParent && oldIndex == aIndex)
    return NS_OK;

  // Make sure aNewParent is not aFolder or a subfolder of aFolder
  if (itemType == TYPE_FOLDER) {
    PRInt64 ancestorId = aNewParent;

    while (ancestorId) {
      if (ancestorId == aItemId) {
        return NS_ERROR_INVALID_ARG;
      }

      DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(getInfoStmt, mDBGetItemProperties);
      rv = getInfoStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"),
                                        ancestorId);
      NS_ENSURE_SUCCESS(rv, rv);

      PRBool hasResult;
      rv = getInfoStmt->ExecuteStep(&hasResult);
      NS_ENSURE_SUCCESS(rv, rv);
      if (hasResult) {
        rv = getInfoStmt->GetInt64(kGetItemPropertiesIndex_Parent, &ancestorId);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else {
        break;
      }
    }
  }

  // calculate new index
  PRInt32 newIndex;
  PRInt32 folderCount;
  rv = FolderCount(aNewParent, &folderCount);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aIndex == nsINavBookmarksService::DEFAULT_INDEX ||
      aIndex >= folderCount) {
    newIndex = folderCount;
    // If the parent remains the same, then the folder is really being moved
    // to count - 1 (since it's being removed from the old position)
    if (oldParent == aNewParent) {
      --newIndex;
    }
  } else {
    newIndex = aIndex;

    if (oldParent == aNewParent && newIndex > oldIndex) {
      // when an item is being moved lower in the same folder, the new index
      // refers to the index before it was removed. Removal causes everything
      // to shift up.
      --newIndex;
    }
  }

  // this is like the previous check, except this covers if
  // the specified index was -1 (append), and the calculated
  // new index is the same as the existing index
  if (aNewParent == oldParent && newIndex == oldIndex) {
    // Nothing to do!
    return NS_OK;
  }

  // adjust indices to account for the move
  // do this before we update the parent/index fields
  // or we'll re-adjust the index for the item we are moving
  if (oldParent == aNewParent) {
    // We can optimize the updates if moving within the same container.
    // We only shift the items between the old and new positions, since the
    // insertion will offset the deletion.
    if (oldIndex > newIndex) {
      rv = AdjustIndices(oldParent, newIndex, oldIndex - 1, 1);
    }
    else {
      rv = AdjustIndices(oldParent, oldIndex + 1, newIndex, -1);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // We're moving between containers, so this happens in two steps.
    // First, fill the hole from the removal from the old parent.
    rv = AdjustIndices(oldParent, oldIndex + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
    // Now, make room in the new parent for the insertion.
    rv = AdjustIndices(aNewParent, newIndex, PR_INT32_MAX, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  {
    // Update parent and position.
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBMoveItem);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aNewParent);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_index"), newIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRTime now = PR_Now();
  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           oldParent, now);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           aNewParent, now);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemMoved(aItemId, oldParent, oldIndex, aNewParent,
                               newIndex, itemType));

  // notify dynamic container provider if there is one
  if (!folderType.IsEmpty()) {
    nsCOMPtr<nsIDynamicContainer> container =
      do_GetService(folderType.get(), &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = container->OnContainerMoved(aItemId, aNewParent, newIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}


nsresult
nsNavBookmarks::SetItemDateInternal(mozIStorageStatement* aStatement,
                                    PRInt64 aItemId,
                                    PRTime aValue)
{
  NS_ENSURE_STATE(aStatement);
  mozStorageStatementScoper scoper(aStatement);

  nsresult rv = aStatement->BindInt64ByName(NS_LITERAL_CSTRING("date"), aValue);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStatement->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // note, we are not notifying the observers
  // that the item has changed.

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemDateAdded(PRInt64 aItemId, PRTime aDateAdded)
{
  // GetItemType also ensures that aItemId points to a valid item.
  PRUint16 itemType;
  nsresult rv = GetItemType(aItemId, &itemType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetItemDateInternal(GetStatement(mDBSetItemDateAdded),
                           aItemId, aDateAdded);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note: mDBSetItemDateAdded also sets lastModified to aDateAdded.
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(aItemId, NS_LITERAL_CSTRING("dateAdded"),
                                 PR_FALSE,
                                 nsPrintfCString(16, "%lld", aDateAdded),
                                 aDateAdded, itemType));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemDateAdded(PRInt64 aItemId, PRTime* _dateAdded)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(_dateAdded);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(hasResult, NS_ERROR_INVALID_ARG); // Invalid itemId.

  rv = stmt->GetInt64(kGetItemPropertiesIndex_DateAdded, _dateAdded);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemLastModified(PRInt64 aItemId, PRTime aLastModified)
{
  // GetItemType also ensures that aItemId points to a valid item.
  PRUint16 itemType;
  nsresult rv = GetItemType(aItemId, &itemType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           aItemId, aLastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(aItemId, NS_LITERAL_CSTRING("lastModified"),
                                 PR_FALSE,
                                 nsPrintfCString(16, "%lld", aLastModified),
                                 aLastModified, itemType));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemLastModified(PRInt64 aItemId, PRTime* aLastModified)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(aLastModified);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasResult)
    return NS_ERROR_INVALID_ARG; // invalid item id

  rv = stmt->GetInt64(kGetItemPropertiesIndex_LastModified, aLastModified);
  NS_ENSURE_SUCCESS(rv, rv);
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

  PRBool hasMore = PR_FALSE;
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
  // GetItemType also ensures that aItemId points to a valid item.
  PRUint16 itemType;
  nsresult rv = GetItemType(aItemId, &itemType);
  NS_ENSURE_SUCCESS(rv, rv);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(statement, mDBSetItemTitle);
  // Support setting a null title, we support this in insertBookmark.
  if (aTitle.IsVoid())
    rv = statement->BindNullByName(NS_LITERAL_CSTRING("item_title"));
  else
    rv = statement->BindUTF8StringByName(NS_LITERAL_CSTRING("item_title"), aTitle);
  NS_ENSURE_SUCCESS(rv, rv);
  PRTime lastModified = PR_Now();
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("date"), lastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(aItemId, NS_LITERAL_CSTRING("title"), PR_FALSE,
                                 aTitle, lastModified, itemType));
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemTitle(PRInt64 aItemId, nsACString& aTitle)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasResult)
    return NS_ERROR_INVALID_ARG; // invalid bookmark id

  rv = stmt->GetUTF8String(kGetItemPropertiesIndex_Title, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetBookmarkURI(PRInt64 aItemId, nsIURI** aURI)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(aURI);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasResult)
    return NS_ERROR_INVALID_ARG; // invalid bookmark id

  PRInt32 type;
  rv = stmt->GetInt32(kGetItemPropertiesIndex_Type, &type);
  NS_ENSURE_SUCCESS(rv, rv);
  // Ensure this is a bookmark.
  NS_ENSURE_TRUE(type == TYPE_BOOKMARK, NS_ERROR_INVALID_ARG);

  nsCAutoString spec;
  rv = stmt->GetUTF8String(kGetItemPropertiesIndex_URI, spec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewURI(aURI, spec);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetItemType(PRInt64 aItemId, PRUint16* _type)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(_type);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasResult) {
    return NS_ERROR_INVALID_ARG; // invalid bookmark id
  }

  PRInt32 itemType;
  rv = stmt->GetInt32(kGetItemPropertiesIndex_Type, &itemType);
  NS_ENSURE_SUCCESS(rv, rv);
  *_type = itemType;

  return NS_OK;
}


nsresult
nsNavBookmarks::GetFolderType(PRInt64 aFolder, nsACString& aType)
{
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasResult) {
    return NS_ERROR_INVALID_ARG;
  }

  return stmt->GetUTF8String(kGetItemPropertiesIndex_ServiceContractId, aType);
}


nsresult
nsNavBookmarks::ResultNodeForContainer(PRInt64 aID,
                                       nsNavHistoryQueryOptions* aOptions,
                                       nsNavHistoryResultNode** aNode)
{
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aID);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(hasResult, NS_ERROR_INVALID_ARG);

  nsCAutoString title;
  rv = stmt->GetUTF8String(kGetItemPropertiesIndex_Title, title);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 itemType;
  rv = stmt->GetInt32(kGetItemPropertiesIndex_Type, &itemType);
  NS_ENSURE_SUCCESS(rv, rv);
  if (itemType == TYPE_DYNAMIC_CONTAINER) {
    // contract id
    nsCAutoString contractId;
    rv = stmt->GetUTF8String(kGetItemPropertiesIndex_ServiceContractId,
                             contractId);
    NS_ENSURE_SUCCESS(rv, rv);
    *aNode = new nsNavHistoryContainerResultNode(EmptyCString(), title,
                                                 EmptyCString(),
                                                 nsINavHistoryResultNode::RESULT_TYPE_DYNAMIC_CONTAINER,
                                                 PR_TRUE, contractId, aOptions);
    (*aNode)->mItemId = aID;
  }
  else { // TYPE_FOLDER
    *aNode = new nsNavHistoryFolderResultNode(title, aOptions, aID, EmptyCString());
  }
  if (!*aNode)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = stmt->GetInt64(kGetItemPropertiesIndex_DateAdded,
                      &(*aNode)->mDateAdded);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt64(kGetItemPropertiesIndex_LastModified,
                      &(*aNode)->mLastModified);
  NS_ENSURE_SUCCESS(rv, rv);

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
  PRBool hasResult;
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
      PRBool readOnly;
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
nsNavBookmarks::FolderCount(PRInt64 aFolderId, PRInt32* _folderCount)
{
  *_folderCount = 0;
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBFolderCount);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("parent"), aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(hasResult, NS_ERROR_UNEXPECTED);

  // Ensure that the folder we are looking for exists.
  PRInt64 confirmFolderId;
  rv = stmt->GetInt64(1, &confirmFolderId);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(confirmFolderId == aFolderId, NS_ERROR_INVALID_ARG);

  rv = stmt->GetInt32(0, _folderCount);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::IsBookmarked(nsIURI* aURI, PRBool* aBookmarked)
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
  nsresult rv = history->GetUrlIdFor(aURI, &placeId, PR_FALSE);
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
  PRBool hasBookmarkedOrigin;
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

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  PRInt64 placeId;
  nsresult rv = history->GetUrlIdFor(aNewURI, &placeId, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!placeId)
    return NS_ERROR_INVALID_ARG;

  // We need the bookmark's current corresponding places ID below, so get it now
  // before we change it.  GetBookmarkURI will fail if aBookmarkId is bad.
  nsCOMPtr<nsIURI> oldURI;
  PRInt64 oldPlaceId;
  rv = GetBookmarkURI(aBookmarkId, getter_AddRefs(oldURI));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = history->GetUrlIdFor(oldURI, &oldPlaceId, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(statement, mDBChangeBookmarkURI);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), placeId);
  NS_ENSURE_SUCCESS(rv, rv);
  PRTime lastModified = PR_Now();
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("date"), lastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aBookmarkId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // Upon changing the URI for a bookmark, update the frecency for the new place.
  // UpdateFrecency needs to know whether placeId is bookmarked (as opposed
  // to a livemark item).  Bookmarking it is exactly what we did above.
  rv = history->UpdateFrecency(placeId, PR_TRUE /* isBookmarked */);
  NS_ENSURE_SUCCESS(rv, rv);

  // Upon changing the URI for a bookmark, update the frecency for the old place.
  // UpdateFrecency again needs to know whether oldPlaceId is bookmarked.  It may
  // no longer be, so we need to figure out whether it still is.  Our strategy
  // is: find all bookmarks corresponding to oldPlaceId that are not livemark
  // items, i.e., whose parents are not livemarks.  If any such bookmarks exist,
  // oldPlaceId is still bookmarked.

  rv = history->UpdateFrecency(oldPlaceId, IsRealBookmark(oldPlaceId));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  rv = aNewURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // Pass the new URI to OnItemChanged.
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(aBookmarkId, NS_LITERAL_CSTRING("uri"),
                                 PR_FALSE, spec, lastModified, TYPE_BOOKMARK));

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetFolderIdForItem(PRInt64 aItemId, PRInt64* aFolderId)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_POINTER(aFolderId);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasResult)
    return NS_ERROR_INVALID_ARG; // invalid item id

  rv = stmt->GetInt64(kGetItemPropertiesIndex_Parent, aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  // this should not happen, but see bug #400448 for details
  NS_ENSURE_TRUE(aItemId != *aFolderId, NS_ERROR_UNEXPECTED);
  return NS_OK;
}


nsresult
nsNavBookmarks::GetBookmarkIdsForURITArray(nsIURI* aURI,
                                           nsTArray<PRInt64>& aResult)
{
  NS_ENSURE_ARG(aURI);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBFindURIBookmarks);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_type"), TYPE_BOOKMARK);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool more;
  while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&more))) && more) {
    PRInt64 bookmarkId;
    rv = stmt->GetInt64(kFindBookmarksIndex_ID, &bookmarkId);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(aResult.AppendElement(bookmarkId), NS_ERROR_OUT_OF_MEMORY);
  }
  NS_ENSURE_SUCCESS(rv, rv);

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
  nsresult rv = GetBookmarkIdsForURITArray(aURI, bookmarks);
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

  *_index = -1;

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasResult)
    return NS_OK;

  rv = stmt->GetInt32(kGetItemPropertiesIndex_Position, _index);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemIndex(PRInt64 aItemId, PRInt32 aNewIndex)
{
  NS_ENSURE_ARG_MIN(aItemId, 1);
  NS_ENSURE_ARG_MIN(aNewIndex, 0);

  nsresult rv;
  PRInt32 oldIndex = 0;
  PRInt64 parent = 0;
  PRInt32 itemType;

  {
    mozIStorageStatement* getInfoStmt(mDBGetItemProperties);
    NS_ENSURE_STATE(getInfoStmt);
    mozStorageStatementScoper scoper(getInfoStmt);
    rv = getInfoStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = getInfoStmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasResult)
      return NS_OK;

    rv = getInfoStmt->GetInt32(kGetItemPropertiesIndex_Position, &oldIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = getInfoStmt->GetInt32(kGetItemPropertiesIndex_Type, &itemType);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = getInfoStmt->GetInt64(kGetItemPropertiesIndex_Parent, &parent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Ensure we are not going out of range.
  PRInt32 folderCount;
  rv = FolderCount(parent, &folderCount);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(aNewIndex < folderCount, NS_ERROR_INVALID_ARG);

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBSetItemIndex);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("item_index"), aNewIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemMoved(aItemId, parent, oldIndex, parent, aNewIndex,
                               itemType));

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

  nsresult rv = EnsureKeywordsHash();
  NS_ENSURE_SUCCESS(rv, rv);

  // Shortcuts are always lowercased internally.
  nsAutoString keyword(aUserCasedKeyword);
  ToLowerCase(keyword);

  // Check if bookmark was already associated to a keyword.
  nsAutoString oldKeyword;
  rv = GetKeywordForBookmark(aBookmarkId, oldKeyword);
  NS_ENSURE_SUCCESS(rv, rv);

  // Trying to set the same value or to remove a nonexistent keyword is a no-op.
  if (keyword.Equals(oldKeyword) || (keyword.IsEmpty() && oldKeyword.IsEmpty()))
    return NS_OK;

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
    mBookmarkToKeywordHash.Remove(aBookmarkId);
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
      mBookmarkToKeywordHash.Remove(aBookmarkId);
    mBookmarkToKeywordHash.Put(aBookmarkId, keyword);
    rv = updateBookmarkStmt->BindStringByName(NS_LITERAL_CSTRING("keyword"), keyword);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  PRTime lastModified = PR_Now();
  rv = updateBookmarkStmt->BindInt64ByName(NS_LITERAL_CSTRING("date"),
                                           lastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = updateBookmarkStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"),
                                           aBookmarkId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = updateBookmarkStmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(aBookmarkId, NS_LITERAL_CSTRING("keyword"),
                                 PR_FALSE, NS_ConvertUTF16toUTF8(keyword),
                                 lastModified, TYPE_BOOKMARK));

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

  PRBool hasMore = PR_FALSE;
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

  PRBool hasMore;
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


// See RunInBatchMode
nsresult
nsNavBookmarks::BeginUpdateBatch()
{
  if (mBatchLevel++ == 0) {
    mBatchDBTransaction = new mozStorageTransaction(mDBConn, PR_FALSE);

    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavBookmarkObserver, OnBeginUpdateBatch());
  }
  return NS_OK;
}


nsresult
nsNavBookmarks::EndUpdateBatch()
{
  if (--mBatchLevel == 0) {
    if (mBatchDBTransaction) {
      nsresult rv = mBatchDBTransaction->Commit();
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Batch failed to commit transaction");
      delete mBatchDBTransaction;
      mBatchDBTransaction = nsnull;
    }

    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavBookmarkObserver, OnEndUpdateBatch());
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RunInBatchMode(nsINavHistoryBatchCallback* aCallback,
                               nsISupports* aUserData) {
  NS_ENSURE_ARG(aCallback);

  BeginUpdateBatch();
  nsresult rv = aCallback->RunBatched(aUserData);
  EndUpdateBatch();

  return rv;
}


NS_IMETHODIMP
nsNavBookmarks::AddObserver(nsINavBookmarkObserver* aObserver,
                            PRBool aOwnsWeak)
{
  NS_ENSURE_ARG(aObserver);
  return mObservers.AppendWeakElement(aObserver, aOwnsWeak);
}


NS_IMETHODIMP
nsNavBookmarks::RemoveObserver(nsINavBookmarkObserver* aObserver)
{
  return mObservers.RemoveWeakElement(aObserver);
}


// nsNavBookmarks::nsINavHistoryObserver

NS_IMETHODIMP
nsNavBookmarks::OnBeginUpdateBatch()
{
  // These aren't passed through to bookmark observers currently.
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnEndUpdateBatch()
{
  // These aren't passed through to bookmark observers currently.
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnVisit(nsIURI* aURI, PRInt64 aVisitID, PRTime aTime,
                        PRInt64 aSessionID, PRInt64 aReferringID,
                        PRUint32 aTransitionType, PRUint32* aAdded)
{
  // If the page is bookmarked, we need to notify observers
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    // query for all bookmarks for that URI, notify for each
    nsTArray<PRInt64> bookmarks;

    nsresult rv = GetBookmarkIdsForURITArray(aURI, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    if (bookmarks.Length()) {
      for (PRUint32 i = 0; i < bookmarks.Length(); i++)
        NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                         nsINavBookmarkObserver,
                         OnItemVisited(bookmarks[i], aVisitID, aTime));
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnBeforeDeleteURI(nsIURI* aURI)
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnDeleteURI(nsIURI* aURI)
{
  // If the page is bookmarked, we need to notify observers
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    // query for all bookmarks for that URI, notify for each 
    nsTArray<PRInt64> bookmarks;

    nsresult rv = GetBookmarkIdsForURITArray(aURI, bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    if (bookmarks.Length()) {
      for (PRUint32 i = 0; i < bookmarks.Length(); i ++)
        NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                         nsINavBookmarkObserver,
                         OnItemChanged(bookmarks[i],
                                       NS_LITERAL_CSTRING("cleartime"),
                                       PR_FALSE, EmptyCString(), 0,
                                       TYPE_BOOKMARK));
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnClearHistory()
{
  // TODO(bryner): we should notify on visited-time change for all URIs
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle)
{
  // NOOP. We don't consume page titles from moz_places anymore.
  // Title-change notifications are sent from SetItemTitle.
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnPageChanged(nsIURI* aURI, PRUint32 aWhat,
                              const nsAString& aValue)
{
  nsresult rv;
  if (aWhat == nsINavHistoryObserver::ATTRIBUTE_FAVICON) {
    // Favicons may be set to either pure URIs or to folder URIs
    PRBool isPlaceURI;
    rv = aURI->SchemeIs("place", &isPlaceURI);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isPlaceURI) {
      nsCAutoString spec;
      rv = aURI->GetSpec(spec);
      NS_ENSURE_SUCCESS(rv, rv);

      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  
      nsCOMArray<nsNavHistoryQuery> queries;
      nsCOMPtr<nsNavHistoryQueryOptions> options;
      rv = history->QueryStringToQueryArray(spec, &queries, getter_AddRefs(options));
      NS_ENSURE_SUCCESS(rv, rv);

      NS_ENSURE_STATE(queries.Count() == 1);
      NS_ENSURE_STATE(queries[0]->Folders().Length() == 1);

      NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                       nsINavBookmarkObserver,
                       OnItemChanged(queries[0]->Folders()[0],
                                     NS_LITERAL_CSTRING("favicon"),
                                     PR_FALSE,
                                     NS_ConvertUTF16toUTF8(aValue),
                                     0, TYPE_BOOKMARK));
    }
    else {
      // query for all bookmarks for that URI, notify for each 
      nsTArray<PRInt64> bookmarks;
      rv = GetBookmarkIdsForURITArray(aURI, bookmarks);
      NS_ENSURE_SUCCESS(rv, rv);

      if (bookmarks.Length()) {
        for (PRUint32 i = 0; i < bookmarks.Length(); i ++)
          NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                           nsINavBookmarkObserver,
                           OnItemChanged(bookmarks[i],
                                         NS_LITERAL_CSTRING("favicon"),
                                         PR_FALSE,
                                         NS_ConvertUTF16toUTF8(aValue),
                                         0, TYPE_BOOKMARK));
      }
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::OnDeleteVisits(nsIURI* aURI, PRTime aVisitTime)
{
  // pages that are bookmarks shouldn't expire, so we don't need to handle it
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
  // GetItemType also ensures that aItemId points to a valid item.
  PRUint16 itemType;
  nsresult rv = GetItemType(aItemId, &itemType);
  NS_ENSURE_SUCCESS(rv, rv);

  PRTime lastModified = PR_Now();
  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           aItemId, lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(aItemId, aName, PR_TRUE, EmptyCString(),
                                 lastModified, itemType));

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
  // GetItemType also ensures that aItemId points to a valid item.
  PRUint16 itemType;
  nsresult rv = GetItemType(aItemId, &itemType);
  NS_ENSURE_SUCCESS(rv, rv);

  PRTime lastModified = PR_Now();
  rv = SetItemDateInternal(GetStatement(mDBSetItemLastModified),
                           aItemId, lastModified);
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavBookmarkObserver,
                   OnItemChanged(aItemId, aName, PR_TRUE, EmptyCString(),
                                 lastModified, itemType));

  return NS_OK;
}


PRBool
nsNavBookmarks::ItemExists(PRInt64 aItemId) {
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemProperties);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aItemId);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  return hasResult;
}
