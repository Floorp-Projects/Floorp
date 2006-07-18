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
#include "nsIAnnotationService.h"
#include "nsIBookmarksContainer.h"

const PRInt32 nsNavBookmarks::kFindBookmarksIndex_ItemChild = 0;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_FolderChild = 1;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Parent = 2;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Position = 3;

const PRInt32 nsNavBookmarks::kGetFolderInfoIndex_FolderID = 0;
const PRInt32 nsNavBookmarks::kGetFolderInfoIndex_Title = 1;
const PRInt32 nsNavBookmarks::kGetFolderInfoIndex_Type = 2;

// These columns sit to the right of the kGetInfoIndex_* columns.
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Position = 9;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_ItemChild = 10;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_FolderChild = 11;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_FolderTitle = 12;

nsNavBookmarks* nsNavBookmarks::sInstance = nsnull;

#define BOOKMARKS_ANNO_PREFIX "bookmarks/"
#define ANNO_FOLDER_READONLY BOOKMARKS_ANNO_PREFIX "readonly"

struct UpdateBatcher
{
  UpdateBatcher() { nsNavBookmarks::GetBookmarksService()->BeginUpdateBatch(); }
  ~UpdateBatcher() { nsNavBookmarks::GetBookmarksService()->EndUpdateBatch(); }
};

nsNavBookmarks::nsNavBookmarks()
  : mRoot(0), mBookmarksRoot(0), mToolbarRoot(0), mBatchLevel(0)
{
  NS_ASSERTION(!sInstance, "Multiple nsNavBookmarks instances!");
  sInstance = this;
}

nsNavBookmarks::~nsNavBookmarks()
{
  NS_ASSERTION(sInstance == this, "Expected sInstance == this");
  sInstance = nsnull;
}

NS_IMPL_ISUPPORTS2(nsNavBookmarks,
                   nsINavBookmarksService, nsINavHistoryObserver)

nsresult
nsNavBookmarks::Init()
{
  nsresult rv;

  nsNavHistory *history = History();
  NS_ENSURE_TRUE(history, NS_ERROR_UNEXPECTED);
  history->AddObserver(this, PR_FALSE); // allows us to notify on title changes
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRBool exists = PR_FALSE;
  dbConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks"), &exists);
  if (!exists) {
    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks ("
        "item_child INTEGER, "
        "folder_child INTEGER, "
        "parent INTEGER, "
        "position INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // moz_bookmarks_folders
  dbConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_folders"), &exists);
  if (!exists) {
    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks_folders ("
        "id INTEGER PRIMARY KEY, "
        "name LONGVARCHAR, "
        "type LONGVARCHAR)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // moz_bookmarks_roots
  dbConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_roots"), &exists);
  if (!exists) {
    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks_roots ("
        "root_name VARCHAR(16) UNIQUE, "
        "folder_id INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT id, name, type FROM moz_bookmarks_folders WHERE id = ?1"),
                               getter_AddRefs(mDBGetFolderInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT folder_child FROM moz_bookmarks WHERE parent IS NULL"),
                                 getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = statement->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (results) {
      mRoot = statement->AsInt64(0);
    }
  }

  nsCAutoString buffer;

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bundleService->CreateBundle(
      "chrome://browser/locale/places/places.properties",
      getter_AddRefs(mBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBFindURIBookmarks
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT a.* "
      "FROM moz_bookmarks a, moz_history h "
      "WHERE h.url = ?1 AND a.item_child = h.id"),
    getter_AddRefs(mDBFindURIBookmarks));
  NS_ENSURE_SUCCESS(rv, rv);

  // Construct a result where the first columns exactly match those returned by
  // mDBGetURLPageInfo, and additionally contains columns for position,
  // item_child, and folder_child from moz_bookmarks.  This selects only
  // _item_ children which are in moz_history.
  // Results are kGetInfoIndex_*
  NS_NAMED_LITERAL_CSTRING(selectItemChildren,
    "SELECT h.id, h.url, h.title, h.user_title, h.rev_host, h.visit_count, "
      "(SELECT MAX(visit_date) FROM moz_historyvisit WHERE page_id = h.id), "
      "f.url, null, a.position, a.item_child, a.folder_child, null "
    "FROM moz_bookmarks a "
    "JOIN moz_history h ON a.item_child = h.id "
    "LEFT OUTER JOIN moz_favicon f ON h.favicon = f.id "
    "WHERE a.parent = ?1 AND a.position >= ?2 AND a.position <= ?3 ");

  // Construct a result where the first columns are padded out to the width
  // of mDBGetVisitPageInfo, containing additional columns for position,
  // item_child, and folder_child from moz_bookmarks, and name from
  // moz_bookmarks_folders.  This selects only _folder_ children which are
  // in moz_bookmarks_folders. Results are kGetInfoIndex_* kGetChildrenIndex_*
  NS_NAMED_LITERAL_CSTRING(selectFolderChildren,
    "SELECT null, null, null, null, null, null, null, null, null, a.position, a.item_child, a.folder_child, c.name "
    "FROM moz_bookmarks a "
    "JOIN moz_bookmarks_folders c ON c.id = a.folder_child "
    "WHERE a.parent = ?1 AND a.position >= ?2 AND a.position <= ?3");

  NS_NAMED_LITERAL_CSTRING(orderByPosition, " ORDER BY a.position");

  // mDBGetFolderChildren: select only folder children of a given folder (unsorted)
  rv = dbConn->CreateStatement(selectFolderChildren,
                               getter_AddRefs(mDBGetFolderChildren));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetChildren: select all children of a given folder, sorted by position
  rv = dbConn->CreateStatement(selectItemChildren +
                               NS_LITERAL_CSTRING(" UNION ALL ") +
                               selectFolderChildren + orderByPosition,
                               getter_AddRefs(mDBGetChildren));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBFolderCount: count all of the children of a given folder
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT COUNT(*) FROM moz_bookmarks WHERE parent = ?1"),
                               getter_AddRefs(mDBFolderCount));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBIndexOfItem: find the position of an item within a folder
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT position FROM moz_bookmarks WHERE item_child = ?1 AND parent = ?2"),
                               getter_AddRefs(mDBIndexOfItem));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT position FROM moz_bookmarks WHERE folder_child = ?1 AND parent = ?2"),
                               getter_AddRefs(mDBIndexOfFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = InitRoots();
  NS_ENSURE_SUCCESS(rv, rv);

  return transaction.Commit();
}

struct RenumberItem {
  PRInt64 folderChild;
  nsCOMPtr<nsIURI> itemURI;
  PRInt32 position;
};

struct RenumberItemsArray {
  nsVoidArray items;
  ~RenumberItemsArray();
};

RenumberItemsArray::~RenumberItemsArray()
{
  for (PRInt32 i = 0; i < items.Count(); ++i) {
    delete NS_STATIC_CAST(RenumberItem*, items[i]);
  }
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
  nsresult rv;
  nsCOMPtr<mozIStorageStatement> getRootStatement;
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("SELECT folder_id FROM moz_bookmarks_roots WHERE root_name = ?1"),
                                 getter_AddRefs(getRootStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool importDefaults = PR_FALSE;
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("places"), &mRoot, &importDefaults);
  NS_ENSURE_SUCCESS(rv, rv);

  getRootStatement->Reset();
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("menu"), &mBookmarksRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  getRootStatement->Reset();
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("toolbar"), &mToolbarRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  if (importDefaults) {
    // when there is no places root, we should define the hierarchy by
    // importing the default one.
    nsCOMPtr<nsIURI> defaultPlaces;
    rv = NS_NewURI(getter_AddRefs(defaultPlaces),
                   NS_LITERAL_CSTRING("chrome://browser/locale/places/default_places.html"),
                   nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ImportBookmarksHTMLInternal(defaultPlaces, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // migrate the user's old bookmarks
    // FIXME: move somewhere else to some profile migrator thingy
    nsCOMPtr<nsIFile> bookmarksFile;
    rv = NS_GetSpecialDirectory(NS_APP_BOOKMARKS_50_FILE,
                                getter_AddRefs(bookmarksFile));
    if (bookmarksFile) {
      PRBool bookmarksFileExists;
      rv = bookmarksFile->Exists(&bookmarksFileExists);
      if (NS_SUCCEEDED(rv) && bookmarksFileExists) {
        nsCOMPtr<nsIIOService> ioservice = do_GetService(
                                    "@mozilla.org/network/io-service;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIURI> bookmarksFileURI;
        rv = ioservice->NewFileURI(bookmarksFile,
                                   getter_AddRefs(bookmarksFileURI));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = ImportBookmarksHTMLInternal(bookmarksFileURI, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
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
                           PRBool* aWasCreated)
{
  PRBool hasResult = PR_FALSE;
  nsresult rv = aGetRootStatement->BindUTF8StringParameter(0, name);
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
  rv = CreateFolder(0, NS_LITERAL_STRING(""), -1, aID);
  NS_ENSURE_SUCCESS(rv, rv);

  // save root ID
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_roots (root_name,folder_id) VALUES (?1, ?2)"),
                                 getter_AddRefs(insertStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = insertStatement->BindUTF8StringParameter(0, name);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = insertStatement->BindInt64Parameter(1, *aID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = insertStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavBookmarks::AdjustIndices(PRInt64 aFolder,
                              PRInt32 aStartIndex, PRInt32 aEndIndex,
                              PRInt32 aDelta)
{
  NS_ASSERTION(aStartIndex <= aEndIndex, "start index must be <= end index");

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCAutoString buffer;
  buffer.AssignLiteral("UPDATE moz_bookmarks SET position = position + ");
  buffer.AppendInt(aDelta);
  buffer.AppendLiteral(" WHERE parent = ");
  buffer.AppendInt(aFolder);

  if (aStartIndex != 0) {
    buffer.AppendLiteral(" AND position >= ");
    buffer.AppendInt(aStartIndex);
  }
  if (aEndIndex != PR_INT32_MAX) {
    buffer.AppendLiteral(" AND position <= ");
    buffer.AppendInt(aEndIndex);
  }

  nsresult rv = DBConn()->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  RenumberItemsArray itemsArray;
  nsVoidArray *items = &itemsArray.items;
  {
    mozStorageStatementScoper scope(mDBGetChildren);
 
    rv = mDBGetChildren->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBGetChildren->BindInt32Parameter(1, aStartIndex + aDelta);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBGetChildren->BindInt32Parameter(2, aEndIndex + aDelta);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    while (NS_SUCCEEDED(mDBGetChildren->ExecuteStep(&results)) && results) {
      RenumberItem *item = new RenumberItem();
      if (!item) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      if (mDBGetChildren->IsNull(kGetChildrenIndex_ItemChild)) {
        item->folderChild = mDBGetChildren->AsInt64(kGetChildrenIndex_FolderChild);
      } else {
        nsCAutoString spec;
        mDBGetChildren->GetUTF8String(nsNavHistory::kGetInfoIndex_URL, spec);
        rv = NS_NewURI(getter_AddRefs(item->itemURI), spec, nsnull);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      item->position = mDBGetChildren->AsInt32(kGetChildrenIndex_Position);
      if (!items->AppendElement(item)) {
        delete item;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  UpdateBatcher batch;

  for (PRUint32 j = 0; j < mObservers.Length(); ++j) {
    const nsCOMPtr<nsINavBookmarkObserver> &obs = mObservers[j];
    if (!obs) {
      continue;
    }

    for (PRInt32 k = 0; k < items->Count(); ++k) {
      RenumberItem *item = NS_STATIC_CAST(RenumberItem*, (*items)[k]);
      PRInt32 newPosition = item->position;
      PRInt32 oldPosition = newPosition - aDelta;
      if (item->itemURI) {
        nsIURI *uri = item->itemURI;
        obs->OnItemMoved(uri, aFolder, oldPosition, newPosition);
      } else {
        obs->OnFolderMoved(item->folderChild, aFolder, oldPosition,
                           aFolder, newPosition);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetPlacesRoot(PRInt64 *aRoot)
{
  *aRoot = mRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarksRoot(PRInt64 *aRoot)
{
  *aRoot = mBookmarksRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetToolbarRoot(PRInt64 *aRoot)
{
  *aRoot = mToolbarRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::InsertItem(PRInt64 aFolder, nsIURI *aItem, PRInt32 aIndex)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 childID;
  nsresult rv = History()->GetUrlIdFor(aItem, &childID, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // see if this item is already in the folder
  PRBool hasItem = PR_FALSE;
  PRInt32 previousIndex = -1;
  { // scope the mDBIndexOfItem statement
    mozStorageStatementScoper scoper(mDBIndexOfItem);
    rv = mDBIndexOfItem->BindInt64Parameter(0, childID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBIndexOfItem->BindInt64Parameter(1, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBIndexOfItem->ExecuteStep(&hasItem);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasItem)
      previousIndex = mDBIndexOfItem->AsInt32(0);
  }

  PRInt32 index = (aIndex == -1) ? FolderCount(aFolder) : aIndex;

  if (hasItem && index == previousIndex)
    return NS_OK; // item already at its desired position: nothing to do

  if (hasItem) {
    // remove any old items
    rv = RemoveItem(aFolder, aItem);
    NS_ENSURE_SUCCESS(rv, rv);

    // since we just removed the item, everything after it shifts back by one
    if (index > previousIndex)
      index --;
  }

  rv = AdjustIndices(aFolder, index, PR_INT32_MAX, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString buffer;
  buffer.AssignLiteral("INSERT INTO moz_bookmarks (item_child, parent, position) VALUES (");
  buffer.AppendInt(childID);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(aFolder);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(index);
  buffer.AppendLiteral(")");

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemAdded(aItem, aFolder, index))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveItem(PRInt64 aFolder, nsIURI *aItem)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 childID;
  nsresult rv = History()->GetUrlIdFor(aItem, &childID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (childID == 0) {
    return NS_OK; // the item isn't in history at all
  }

  PRInt32 childIndex;
  nsCAutoString buffer;
  {
    mozStorageStatementScoper scope(mDBIndexOfItem);
    mDBIndexOfItem->BindInt64Parameter(0, childID);
    mDBIndexOfItem->BindInt64Parameter(1, aFolder);

    PRBool results;
    rv = mDBIndexOfItem->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);

    // We _should_ always have a result here, but maybe we don't if the table
    // has become corrupted.  Just silently skip adjusting indices.
    childIndex = results ? mDBIndexOfItem->AsInt32(0) : -1;
  }

  buffer.AssignLiteral("DELETE FROM moz_bookmarks WHERE parent = ");
  buffer.AppendInt(aFolder);
  buffer.AppendLiteral(" AND item_child = ");
  buffer.AppendInt(childID);

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  if (childIndex != -1) {
    rv = AdjustIndices(aFolder, childIndex + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemRemoved(aItem, aFolder, childIndex))

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::ReplaceItem(PRInt64 aFolder, nsIURI *aItem, nsIURI *aNewItem)
{
  mozIStorageConnection *dbConn = DBConn();
  nsNavHistory *history = History();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 childID;
  nsresult rv = history->GetUrlIdFor(aItem, &childID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (childID == 0) {
    return NS_ERROR_INVALID_ARG; // the item isn't in history at all
  }

  PRInt64 newChildID;
  rv = history->GetUrlIdFor(aNewItem, &newChildID, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(newChildID != 0, "must have an item id");

  nsCAutoString buffer;
  buffer.AssignLiteral("UPDATE moz_bookmarks SET item_child = ");
  buffer.AppendInt(newChildID);
  buffer.AppendLiteral(" WHERE item_child = ");
  buffer.AppendInt(childID);
  buffer.AppendLiteral(" AND parent = ");
  buffer.AppendInt(aFolder);

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemReplaced(aFolder, aItem, aNewItem))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::CreateFolder(PRInt64 aParent, const nsAString &aName,
                             PRInt32 aIndex, PRInt64 *aNewFolder)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt32 index = (aIndex == -1) ? FolderCount(aParent) : aIndex;

  nsresult rv = AdjustIndices(aParent, index, PR_INT32_MAX, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_folders (name, type) VALUES (?1, null)"),
                                 getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindStringParameter(0, aName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRInt64 child;
  rv = dbConn->GetLastInsertRowID(&child);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString buffer;
  buffer.AssignLiteral("INSERT INTO moz_bookmarks (folder_child, parent, position) VALUES (");
  buffer.AppendInt(child);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(aParent);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(index);
  buffer.AppendLiteral(")");
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnFolderAdded(child, aParent, index))

  *aNewFolder = child;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::CreateContainer(PRInt64 aParent, const nsAString &aName,
                                PRInt32 aIndex, const nsAString &aType,
                                PRInt64 *aNewFolder)
{
  // Containers are wrappers around read-only folders, with a specific type.
  nsresult rv = CreateFolder(aParent, aName, aIndex, aNewFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the type.
  nsCOMPtr<mozIStorageStatement> statement;
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_bookmarks_folders SET type = ?2 WHERE id = ?1"),
                                 getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, *aNewFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(1, aType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RemoveFolder(PRInt64 aFolder)
{
  // If this is a container bookmark, try to notify its service.
  nsresult rv;
  nsAutoString folderType;
  rv = GetFolderType(aFolder, folderType);
  NS_ENSURE_SUCCESS(rv, rv);
  if (folderType.Length() > 0) {
    // There is a type associated with this folder; it's a livemark.
    nsCOMPtr<nsIBookmarksContainer> bmcServ = do_GetService(NS_ConvertUTF16toUTF8(folderType).get());
    if (bmcServ) {
      rv = bmcServ->OnContainerRemoving(aFolder);
      if (NS_FAILED(rv))
        NS_WARNING("Remove folder container notification failed.");
    }
  }

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCAutoString buffer;
  buffer.AssignLiteral("SELECT parent, position FROM moz_bookmarks WHERE folder_child = ");
  buffer.AppendInt(aFolder);

  PRInt64 parent;
  PRInt32 index;
  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(buffer, getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = statement->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!results) {
      return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
    }

    parent = statement->AsInt64(0);
    index = statement->AsInt32(1);
  }

  // Remove all of the folder's children
  RemoveFolderChildren(aFolder);

  // Remove the folder from its parent
  buffer.AssignLiteral("DELETE FROM moz_bookmarks WHERE folder_child = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  // And remove the folder from the folder table
  buffer.AssignLiteral("DELETE FROM moz_bookmarks_folders WHERE id = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AdjustIndices(parent, index + 1, PR_INT32_MAX, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnFolderRemoved(aFolder, parent, index))

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RemoveFolderChildren(PRInt64 aFolder)
{
  nsresult rv;
  nsCAutoString buffer;
  mozIStorageConnection *dbConn = DBConn();

  // Now locate all of the folder children, so we can recursively remove them.
  nsTArray<PRInt64> folderChildren;
  {
    mozStorageStatementScoper scope(mDBGetFolderChildren);
    rv = mDBGetChildren->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetChildren->BindInt32Parameter(1, 0);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetChildren->BindInt32Parameter(2, PR_INT32_MAX);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool more;
    while (NS_SUCCEEDED(rv = mDBGetChildren->ExecuteStep(&more)) && more) {
      folderChildren.AppendElement(mDBGetChildren->AsInt64(kGetChildrenIndex_FolderChild));
    }
  }

  for (PRUint32 i = 0; i < folderChildren.Length(); ++i) {
    RemoveFolder(folderChildren[i]);
  }

  // Now remove the remaining items
  buffer.AssignLiteral("DELETE FROM moz_bookmarks WHERE parent = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::MoveFolder(PRInt64 aFolder, PRInt64 aNewParent, PRInt32 aIndex)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT parent, position FROM moz_bookmarks WHERE folder_child = ?1"),
                                        getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 parent;
  PRInt32 oldIndex;
  {
    mozStorageStatementScoper scope(statement);
    rv = statement->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = statement->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!results) {
      return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
    }

    parent = statement->AsInt64(0);
    oldIndex = statement->AsInt32(1);
  }

  // Make sure aNewParent is not aFolder or a subfolder of aFolder
  {
    mozStorageStatementScoper scope(statement);
    PRInt64 p = aNewParent;

    while (p) {
      if (p == aFolder) {
        return NS_ERROR_INVALID_ARG;
      }

      rv = statement->BindInt64Parameter(0, p);
      NS_ENSURE_SUCCESS(rv, rv);

      PRBool results;
      rv = statement->ExecuteStep(&results);
      NS_ENSURE_SUCCESS(rv, rv);
      p = results ? statement->AsInt64(0) : 0;
    }
  }

  PRInt32 newIndex;
  if (aIndex == -1) {
    newIndex = FolderCount(aNewParent);
    // If the parent remains the same, then the folder is really being moved
    // to count - 1 (since it's being removed from the old position)
    if (parent == aNewParent) {
      --newIndex;
    }
  } else {
    newIndex = aIndex;

    if (parent == aNewParent && newIndex > oldIndex) {
      // when an item is being moved lower in the same folder, the new index
      // refers to the index before it was removed. Removal causes everything
      // to shift up.
      --newIndex;
    }
  }

  if (aNewParent == parent && newIndex == oldIndex) {
    // Nothing to do!
    return NS_OK;
  }

  // First we remove the item from its old position.
  nsCAutoString buffer;
  buffer.AssignLiteral("DELETE FROM moz_bookmarks WHERE folder_child = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now renumber to account for the move
  if (parent == aNewParent) {
    // We can optimize the updates if moving within the same container.
    // We only shift the items between the old and new positions, since the
    // insertion will offset the deletion.
    if (oldIndex > newIndex) {
      rv = AdjustIndices(parent, newIndex, oldIndex - 1, 1);
    } else {
      rv = AdjustIndices(parent, oldIndex + 1, newIndex, -1);
    }
  } else {
    // We're moving between containers, so this happens in two steps.
    // First, fill the hole from the deletion.
    rv = AdjustIndices(parent, oldIndex + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
    // Now, make room in the new parent for the insertion.
    rv = AdjustIndices(aNewParent, newIndex, PR_INT32_MAX, 1);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
         "INSERT INTO moz_bookmarks (folder_child, parent, position) "
         "VALUES (?1, ?2, ?3)"),
                                 getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt64Parameter(1, aNewParent);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt32Parameter(2, newIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnFolderMoved(aFolder, parent, oldIndex,
                                    aNewParent, newIndex))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetChildFolder(PRInt64 aFolder, const nsAString& aSubFolder,
                               PRInt64* _result)
{
  // note: we allow empty folder names
  nsresult rv;
  if (aFolder == 0)
    return NS_ERROR_INVALID_ARG;

  // If this gets used a lot, we'll want a precompiled statement
  nsCOMPtr<mozIStorageStatement> statement;
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("SELECT c.id FROM moz_bookmarks a JOIN moz_bookmarks_folders c ON a.folder_child = c.id WHERE a.parent = ?1 AND c.name = ?2"),
                                 getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  statement->BindInt64Parameter(0, aFolder);
  statement->BindStringParameter(1, aSubFolder);

  PRBool hasResult = PR_FALSE;
  rv = statement->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (! hasResult) {
    // item not found
    *_result = 0;
    return NS_OK;
  }

  return statement->GetInt64(0, _result);
}

NS_IMETHODIMP
nsNavBookmarks::SetItemTitle(nsIURI *aURI, const nsAString &aTitle)
{
  return History()->SetPageTitle(aURI, aTitle);
}


NS_IMETHODIMP
nsNavBookmarks::GetItemTitle(nsIURI *aURI, nsAString &aTitle)
{
  mozIStorageStatement *statement = DBGetURLPageInfo();
  nsresult rv = BindStatementURI(statement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageStatementScoper scope(statement);

  PRBool results;
  rv = statement->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    return NS_ERROR_INVALID_ARG;
  }

  return statement->GetString(nsNavHistory::kGetInfoIndex_Title, aTitle);
}

NS_IMETHODIMP
nsNavBookmarks::SetFolderTitle(PRInt64 aFolder, const nsAString &aTitle)
{
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_bookmarks_folders SET name = ?2 WHERE id = ?1"),
                                 getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(1, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnFolderChanged(aFolder, NS_LITERAL_CSTRING("title")))

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetFolderTitle(PRInt64 aFolder, nsAString &aTitle)
{
  mozStorageStatementScoper scope(mDBGetFolderInfo);
  nsresult rv = mDBGetFolderInfo->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetFolderInfo->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    return NS_ERROR_INVALID_ARG;
  }

  return mDBGetFolderInfo->GetString(kGetFolderInfoIndex_Title, aTitle);
}


nsresult
nsNavBookmarks::GetFolderType(PRInt64 aFolder, nsAString &aType)
{
  mozStorageStatementScoper scope(mDBGetFolderInfo);
  nsresult rv = mDBGetFolderInfo->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetFolderInfo->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    return NS_ERROR_INVALID_ARG;
  }

  return mDBGetFolderInfo->GetString(kGetFolderInfoIndex_Type, aType);
}

nsresult
nsNavBookmarks::ResultNodeForFolder(PRInt64 aID,
                                    nsNavHistoryQueryOptions *aOptions,
                                    nsNavHistoryResultNode **aNode)
{
  mozStorageStatementScoper scope(mDBGetFolderInfo);
  mDBGetFolderInfo->BindInt64Parameter(0, aID);

  PRBool results;
  nsresult rv = mDBGetFolderInfo->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(results, "ResultNodeForFolder expects a valid folder id");

  // type (empty for normal ones, nonempty for container providers)
  nsCAutoString folderType;
  rv = mDBGetFolderInfo->GetUTF8String(kGetFolderInfoIndex_Type, folderType);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  nsCAutoString title;
  rv = mDBGetFolderInfo->GetUTF8String(kGetFolderInfoIndex_Title, title);

  if (folderType.IsEmpty()) {
    *aNode = new nsNavHistoryFolderResultNode(title, 0, 0, aOptions, aID);
    if (! *aNode)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aNode);
  } else {
    // a container API thing
    // FIXME(brettw)
    //NS_NOTREACHED("FIXME: Not implemented");
    *aNode = nsnull;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetFolderURI(PRInt64 aFolder, nsIURI **aURI)
{
  // Create a query for the folder; the URI is the querystring from that
  // query. We could create a proper query and serialize it, which might
  // make it less prone to breakage since we'd only have one code path.
  // However, this gets called a lot (every time we make a folder node)
  // and constructing fake queries and options each time just to
  // serialize them would be a waste. Therefore, we just synthesize the
  // correct string here.
  //
  // If you change this, change IsSimpleFolderURI which detects this string.
  nsCAutoString spec("place:folders=");
  spec.AppendInt(aFolder);
  spec.AppendLiteral("&group=3"); // GROUP_BY_FOLDER
  return NS_NewURI(aURI, spec);
}

NS_IMETHODIMP
nsNavBookmarks::GetFolderReadonly(PRInt64 aFolder, PRBool *aResult)
{
  // Ask the folder's nsIBookmarksContainer for the readonly property.
  *aResult = PR_FALSE;
  nsAutoString type;
  nsresult rv = GetFolderType(aFolder, type);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!type.IsEmpty()) {
    nsCOMPtr<nsIBookmarksContainer> container =
      do_GetService(NS_ConvertUTF16toUTF8(type).get(), &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = container->GetChildrenReadOnly(aResult);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
nsNavBookmarks::QueryFolderChildren(PRInt64 aFolderId,
                                    nsNavHistoryQueryOptions *aOptions,
                                    nsCOMArray<nsNavHistoryResultNode> *aChildren)
{
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  nsresult rv;
  mozStorageStatementScoper scope(mDBGetChildren);

  rv = mDBGetChildren->BindInt64Parameter(0, aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBGetChildren->BindInt32Parameter(1, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBGetChildren->BindInt32Parameter(2, PR_INT32_MAX);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;

  nsCOMPtr<nsNavHistoryQueryOptions> options = do_QueryInterface(aOptions, &rv);
  while (NS_SUCCEEDED(mDBGetChildren->ExecuteStep(&results)) && results) {
    PRBool isFolder = !mDBGetChildren->IsNull(kGetChildrenIndex_FolderChild);
    nsCOMPtr<nsNavHistoryResultNode> node;
    if (isFolder) {
      PRInt64 folder = mDBGetChildren->AsInt64(kGetChildrenIndex_FolderChild);
      rv = ResultNodeForFolder(folder, aOptions, getter_AddRefs(node));
      if (NS_FAILED(rv))
        continue;
    } else {
      rv = History()->RowToResult(mDBGetChildren, options,
                                  getter_AddRefs(node));
      NS_ENSURE_SUCCESS(rv, rv);

      PRUint32 nodeType;
      node->GetType(&nodeType);
      if ((nodeType == nsINavHistoryResultNode::RESULT_TYPE_QUERY &&
           aOptions->ExcludeQueries()) ||
          (nodeType != nsINavHistoryResultNode::RESULT_TYPE_QUERY &&
           nodeType != nsINavHistoryResultNode::RESULT_TYPE_FOLDER &&
           aOptions->ExcludeItems())) {
        continue;
      }
    }

    NS_ENSURE_TRUE(aChildren->AppendObject(node), NS_ERROR_OUT_OF_MEMORY);
  }
  return transaction.Commit();
}

PRInt32
nsNavBookmarks::FolderCount(PRInt64 aFolder)
{
  mozStorageStatementScoper scope(mDBFolderCount);

  nsresult rv = mDBFolderCount->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, 0);

  PRBool results;
  rv = mDBFolderCount->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  return mDBFolderCount->AsInt32(0);
}

NS_IMETHODIMP
nsNavBookmarks::IsBookmarked(nsIURI *aURI, PRBool *aBookmarked)
{
  *aBookmarked = PR_FALSE;

  mozStorageStatementScoper scope(mDBFindURIBookmarks);

  nsresult rv = BindStatementURI(mDBFindURIBookmarks, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBFindURIBookmarks->ExecuteStep(aBookmarked);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkFolders(nsIURI *aURI, PRUint32 *aCount,
                                   PRInt64 **aFolders)
{
  *aCount = 0;
  *aFolders = nsnull;

  mozStorageStatementScoper scope(mDBFindURIBookmarks);
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  nsresult rv = BindStatementURI(mDBFindURIBookmarks, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<PRInt64> folders;
  PRBool more;
  while (NS_SUCCEEDED((rv = mDBFindURIBookmarks->ExecuteStep(&more))) && more) {
    if (! folders.AppendElement(
        mDBFindURIBookmarks->AsInt64(kFindBookmarksIndex_Parent)))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  if (folders.Length()) {
    *aFolders = NS_STATIC_CAST(PRInt64*,
                           nsMemory::Alloc(sizeof(PRInt64) * folders.Length()));
    if (! aFolders)
      return NS_ERROR_OUT_OF_MEMORY;
    for (PRUint32 i = 0; i < folders.Length(); i ++)
      (*aFolders)[i] = folders[i];
  }
  *aCount = folders.Length();
  return transaction.Commit();
}

NS_IMETHODIMP
nsNavBookmarks::IndexOfItem(PRInt64 aFolder, nsIURI *aItem, PRInt32 *aIndex)
{
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  PRInt64 id;
  nsresult rv = History()->GetUrlIdFor(aItem, &id, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (id == 0) {
    *aIndex = -1;
    return NS_OK;
  }

  mozStorageStatementScoper scope(mDBIndexOfItem);
  mDBIndexOfItem->BindInt64Parameter(0, id);
  mDBIndexOfItem->BindInt64Parameter(1, aFolder);
  PRBool results;
  rv = mDBIndexOfItem->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!results) {
    *aIndex = -1;
    return NS_OK;
  }

  *aIndex = mDBIndexOfItem->AsInt32(0);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::IndexOfFolder(PRInt64 aParent,
                              PRInt64 aFolder, PRInt32 *aIndex)
{
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  mozStorageStatementScoper scope(mDBIndexOfFolder);
  mDBIndexOfFolder->BindInt64Parameter(0, aFolder);
  mDBIndexOfFolder->BindInt64Parameter(1, aParent);
  PRBool results;
  nsresult rv = mDBIndexOfFolder->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!results) {
    *aIndex = -1;
    return NS_OK;
  }

  *aIndex = mDBIndexOfFolder->AsInt32(0);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::BeginUpdateBatch()
{
  if (mBatchLevel++ == 0) {
    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnBeginUpdateBatch())
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::EndUpdateBatch()
{
  if (--mBatchLevel == 0) {
    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnEndUpdateBatch())
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::AddObserver(nsINavBookmarkObserver *aObserver,
                            PRBool aOwnsWeak)
{
  return mObservers.AppendWeakElement(aObserver, aOwnsWeak);
}

NS_IMETHODIMP
nsNavBookmarks::RemoveObserver(nsINavBookmarkObserver *aObserver)
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
nsNavBookmarks::OnVisit(nsIURI *aURI, PRInt64 aVisitID, PRTime aTime,
                        PRInt64 aSessionID, PRInt64 aReferringID,
                        PRUint32 aTransitionType)
{
  // If the page is bookmarked, we need to notify observers
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnItemVisited(aURI, aVisitID, aTime))
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnDeleteURI(nsIURI *aURI)
{
  // If the page is bookmarked, we need to notify observers
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnItemChanged(aURI, NS_LITERAL_CSTRING("cleartime"),
                                      EmptyString()))
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
nsNavBookmarks::OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle,
                               const nsAString& aUserTitle,
                               PRBool aIsUserTitleChanged)
{
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    if (aUserTitle.IsVoid()) {
      // use "real" title because the user title is NULL. Either the user title
      // was "unset" or the page title changed.
      ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                          OnItemChanged(aURI, NS_LITERAL_CSTRING("title"),
                                        aPageTitle));
    } else if (aIsUserTitleChanged) {
      // there is a user title and it changed
      ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                          OnItemChanged(aURI, NS_LITERAL_CSTRING("title"),
                                        aUserTitle));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnPageChanged(nsIURI *aURI, PRUint32 aWhat,
                              const nsAString &aValue)
{
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    if (aWhat == nsINavHistoryObserver::ATTRIBUTE_FAVICON) {
      ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                          OnItemChanged(aURI, NS_LITERAL_CSTRING("favicon"),
                                        aValue));
    }
  }
  return NS_OK;
}
