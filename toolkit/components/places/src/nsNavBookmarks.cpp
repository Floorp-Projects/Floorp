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

#include "nsNavBookmarks.h"
#include "nsNavHistory.h"
#include "mozStorageHelper.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"

const PRInt32 nsNavBookmarks::kFindBookmarksIndex_ItemChild = 0;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_FolderChild = 1;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Parent = 2;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Position = 3;

const PRInt32 nsNavBookmarks::kGetFolderInfoIndex_FolderID = 0;
const PRInt32 nsNavBookmarks::kGetFolderInfoIndex_Title = 1;

// These columns sit to the right of the kGetInfoIndex_* columns.
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Position = 6;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_ItemChild = 7;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_FolderChild = 8;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_FolderTitle = 9;

nsNavBookmarks* nsNavBookmarks::sInstance = nsnull;

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
  history->AddObserver(this); // allows us to notify on title changes
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRBool exists = PR_FALSE;
  dbConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_assoc"), &exists);
  if (!exists) {
    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks_assoc (item_child INTEGER, folder_child INTEGER, parent INTEGER, position INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  dbConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_containers"), &exists);
  if (!exists) {
    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks_containers (id INTEGER PRIMARY KEY, name LONGVARCHAR)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT id, name FROM moz_bookmarks_containers WHERE id = ?1"),
                               getter_AddRefs(mDBGetFolderInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT folder_child FROM moz_bookmarks_assoc WHERE parent IS NULL"),
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

  if (mRoot) {
    // Locate the bookmarks and toolbar folders
    buffer.AssignLiteral("SELECT folder_child FROM moz_bookmarks_assoc a JOIN moz_bookmarks_containers c ON a.folder_child = c.id WHERE c.name = ?1 AND a.parent = ");
    buffer.AppendInt(mRoot);

    nsCOMPtr<mozIStorageStatement> locateChild;
    rv = dbConn->CreateStatement(buffer, getter_AddRefs(locateChild));
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLString bookmarksMenuName;
    mBundle->GetStringFromName(NS_LITERAL_STRING("bookmarksMenuName").get(),
      getter_Copies(bookmarksMenuName));
    rv = locateChild->BindStringParameter(0, bookmarksMenuName);
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool results;
    rv = locateChild->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (results) {
      mBookmarksRoot = locateChild->AsInt64(0);
    }
    rv = locateChild->Reset();
    NS_ENSURE_SUCCESS(rv, rv);
    nsXPIDLString bookmarksToolbarName;
    mBundle->GetStringFromName(NS_LITERAL_STRING("bookmarksToolbarName").get(),
      getter_Copies(bookmarksToolbarName));
    rv = locateChild->BindStringParameter(0, bookmarksToolbarName);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = locateChild->ExecuteStep(&results);
    if (results) {
      mToolbarRoot = locateChild->AsInt64(0);
    }
  }

  if (!mRoot) {
    // Create the root Places container
    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_containers (name) VALUES (NULL)"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dbConn->GetLastInsertRowID(&mRoot);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(mRoot != 0, "row id must be non-zero!");

    buffer.AssignLiteral("INSERT INTO moz_bookmarks_assoc (folder_child) VALUES(");
    buffer.AppendInt(mRoot);
    buffer.AppendLiteral(")");

    rv = dbConn->ExecuteSimpleSQL(buffer);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (!mBookmarksRoot) {
    nsXPIDLString bookmarksMenuName;
    mBundle->GetStringFromName(NS_LITERAL_STRING("bookmarksMenuName").get(),
      getter_Copies(bookmarksMenuName));
    CreateFolder(mRoot, bookmarksMenuName, 0, &mBookmarksRoot);
    NS_ASSERTION(mBookmarksRoot != 0, "row id must be non-zero!");
  }
  if (!mToolbarRoot) {
    nsXPIDLString bookmarksToolbarName;
    mBundle->GetStringFromName(NS_LITERAL_STRING("bookmarksToolbarName").get(),
      getter_Copies(bookmarksToolbarName));
    CreateFolder(mRoot, bookmarksToolbarName, 1, &mToolbarRoot);
    NS_ASSERTION(mToolbarRoot != 0, "row id must be non-zero!");
  }

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT a.* FROM moz_bookmarks_assoc a, moz_history h WHERE h.url = ?1 AND a.item_child = h.id"),
                               getter_AddRefs(mDBFindURIBookmarks));
  NS_ENSURE_SUCCESS(rv, rv);

  // Construct a result where the first columns exactly match those returned by
  // mDBGetVisitPageInfo, and additionally contains columns for position,
  // item_child, and folder_child from moz_bookmarks_assoc.  This selects only
  // _item_ children which are in moz_history.
  NS_NAMED_LITERAL_CSTRING(selectItemChildren,
                           "SELECT h.id, h.url, h.title, h.visit_count, MAX(fullv.visit_date), h.rev_host, a.position, a.item_child, a.folder_child, null FROM moz_bookmarks_assoc a JOIN moz_history h ON a.item_child = h.id LEFT JOIN moz_historyvisit v ON h.id = v.page_id LEFT JOIN moz_historyvisit fullv ON h.id = fullv.page_id WHERE a.parent = ?1 AND a.position >= ?2 AND a.position <= ?3 GROUP BY h.id");

  // Construct a result where the first columns are padded out to the width
  // of mDBGetVisitPageInfo, containing additional columns for position,
  // item_child, and folder_child from moz_bookmarks_assoc, and name from
  // moz_bookmarks_containers.  This selects only _folder_ children which are
  // in moz_bookmarks_containers.
  NS_NAMED_LITERAL_CSTRING(selectFolderChildren,
                           "SELECT null, null, null, null, null, null, a.position, a.item_child, a.folder_child, c.name FROM moz_bookmarks_assoc a JOIN moz_bookmarks_containers c ON c.id = a.folder_child WHERE a.parent = ?1 AND a.position >= ?2 AND a.position <= ?3");

  NS_NAMED_LITERAL_CSTRING(orderByPosition, " ORDER BY a.position");

  // Select all children of a given folder, sorted by position
  rv = dbConn->CreateStatement(selectItemChildren +
                               NS_LITERAL_CSTRING(" UNION ALL ") +
                               selectFolderChildren + orderByPosition,
                               getter_AddRefs(mDBGetChildren));
  NS_ENSURE_SUCCESS(rv, rv);

  // Returns only the item children in a folder
  rv = dbConn->CreateStatement(selectItemChildren + orderByPosition,
                               getter_AddRefs(mDBGetItemChildren));
  NS_ENSURE_SUCCESS(rv, rv);

  // Returns only the folder children in a folder
  rv = dbConn->CreateStatement(selectFolderChildren + orderByPosition,
                               getter_AddRefs(mDBGetFolderChildren));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT COUNT(*) FROM moz_bookmarks_assoc WHERE parent = ?1"),
                               getter_AddRefs(mDBFolderCount));
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

nsresult
nsNavBookmarks::AdjustIndices(PRInt64 aFolder,
                              PRInt32 aStartIndex, PRInt32 aEndIndex,
                              PRInt32 aDelta)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCAutoString buffer;
  buffer.AssignLiteral("UPDATE moz_bookmarks_assoc SET position = position + ");
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

  // If we have any observers that want all details, we'll need to notify them
  // about the renumbering.
  nsCOMArray<nsINavBookmarkObserver> detailObservers;
  PRInt32 i;
  for (i = 0; i < mObservers.Count(); ++i) {
    PRBool wantDetails;
    rv = mObservers[i]->GetWantAllDetails(&wantDetails);
    if (NS_SUCCEEDED(rv) && wantDetails) {
      if (!detailObservers.AppendObject(mObservers[i])) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  if (detailObservers.Count() == 0) {
    return transaction.Commit();
  }

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
      item->position = mDBGetChildren->AsInt32(2);
      if (!items->AppendElement(item)) {
        delete item;
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  UpdateBatcher batch;

  for (i = 0; i < detailObservers.Count(); ++i) {
    for (PRInt32 j = 0; j < items->Count(); ++j) {
      RenumberItem *item = NS_STATIC_CAST(RenumberItem*, (*items)[j]);
      PRInt32 newPosition = item->position;
      PRInt32 oldPosition = newPosition - aDelta;
      if (item->itemURI) {
        nsIURI *uri = item->itemURI;
        detailObservers[i]->OnItemMoved(uri, aFolder, oldPosition, newPosition);
      } else {
        detailObservers[i]->OnFolderMoved(item->folderChild,
                                          aFolder, oldPosition,
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

  PRInt32 index = (aIndex == -1) ? FolderCount(aFolder) : aIndex;

  rv = AdjustIndices(aFolder, index, PR_INT32_MAX, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString buffer;
  buffer.AssignLiteral("INSERT INTO moz_bookmarks_assoc (item_child, parent, position) VALUES (");
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

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnItemAdded(aItem, aFolder, index);
  }

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
    buffer.AssignLiteral("SELECT position FROM moz_bookmarks_assoc WHERE parent = ");
    buffer.AppendInt(aFolder);
    buffer.AppendLiteral(" AND item_child = ");
    buffer.AppendInt(childID);

    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(buffer, getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = statement->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);

    // We _should_ always have a result here, but maybe we don't if the table
    // has become corrupted.  Just silently skip adjusting indices.
    childIndex = results ? statement->AsInt32(0) : -1;
  }

  buffer.AssignLiteral("DELETE FROM moz_bookmarks_assoc WHERE parent = ");
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

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnItemRemoved(aItem, aFolder, childIndex);
  }

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

  rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_containers (name) VALUES (\"") +
                                NS_ConvertUTF16toUTF8(aName) +
                                NS_LITERAL_CSTRING("\")"));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 child;
  rv = dbConn->GetLastInsertRowID(&child);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString buffer;
  buffer.AssignLiteral("INSERT INTO moz_bookmarks_assoc (folder_child, parent, position) VALUES (");
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

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnFolderAdded(child, aParent, index);
  }

  *aNewFolder = child;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RemoveFolder(PRInt64 aFolder)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCAutoString buffer;
  buffer.AssignLiteral("SELECT parent, position FROM moz_bookmarks_assoc WHERE folder_child = ");
  buffer.AppendInt(aFolder);

  nsresult rv;
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

  buffer.AssignLiteral("DELETE FROM moz_bookmarks_containers WHERE id = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  buffer.AssignLiteral("DELETE FROM moz_bookmarks_assoc WHERE folder_child = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AdjustIndices(parent, index + 1, PR_INT32_MAX, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnFolderRemoved(aFolder, parent, index);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::MoveFolder(PRInt64 aFolder, PRInt64 aNewParent, PRInt32 aIndex)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCAutoString buffer;
  buffer.AssignLiteral("SELECT parent, position FROM moz_bookmarks_assoc WHERE folder_child = ");
  buffer.AppendInt(aFolder);

  nsresult rv;
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

  if (aNewParent == parent && aIndex == index) {
    // Nothing to do!
    return NS_OK;
  }

  if (parent == aNewParent) {
    // We can optimize the updates if moving within the same container
    if (index > aIndex) {
      rv = AdjustIndices(parent, aIndex, index - 1, 1);
    } else {
      rv = AdjustIndices(parent, index + 1, aIndex, -1);
    }
  } else {
    rv = AdjustIndices(parent, index + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AdjustIndices(aNewParent, aIndex, PR_INT32_MAX, 1);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  buffer.AssignLiteral("UPDATE moz_bookmarks_assoc SET parent = ");
  buffer.AppendInt(aNewParent);
  buffer.AppendLiteral(", position = ");
  buffer.AppendInt(aIndex);
  buffer.AppendLiteral(" WHERE id = ");
  buffer.AppendInt(aFolder);

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnFolderMoved(aFolder, parent, index, aNewParent, aIndex);
  }

  return NS_OK;
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
  nsCAutoString buffer;
  buffer.AssignLiteral("UPDATE moz_bookmarks_container SET title = ");
  AppendUTF16toUTF8(aTitle, buffer);
  buffer.AppendLiteral(" WHERE id = ");
  buffer.AppendInt(aFolder);

  nsresult rv = DBConn()->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnFolderChanged(aFolder, NS_LITERAL_CSTRING("title"));
  }

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
nsNavFolderResultNode::BuildChildren(PRUint32 aOptions)
{
  if (mBuiltChildren) {
    return NS_OK;
  }

  nsresult rv = nsNavBookmarks::sInstance->QueryFolderChildren(mID, aOptions,
                                                               &mChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  mBuiltChildren = PR_TRUE;
  return NS_OK;
}

nsresult
nsNavBookmarks::ResultNodeForFolder(PRInt64 aID,
                                    nsNavHistoryResultNode **aNode)
{
  mozStorageStatementScoper scope(mDBGetFolderInfo);
  mDBGetFolderInfo->BindInt64Parameter(0, aID);

  PRBool results;
  nsresult rv = mDBGetFolderInfo->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(results, "ResultNodeForFolder expects a valid folder id");

  nsAutoString title;
  rv = mDBGetFolderInfo->GetString(kGetFolderInfoIndex_Title, title);
  NS_ENSURE_SUCCESS(rv, rv);

  nsNavFolderResultNode *node = new nsNavFolderResultNode();
  NS_ENSURE_TRUE(node, NS_ERROR_OUT_OF_MEMORY);

  node->mID = aID;
  node->mType = nsINavHistoryResultNode::RESULT_TYPE_FOLDER;
  node->mTitle = title;
  node->mAccessCount = 0;
  node->mTime = 0;

  NS_ADDREF(*aNode = node);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetBookmarks(nsINavHistoryResult **aResult)
{
  *aResult = nsnull;

  nsRefPtr<nsNavHistoryResult> result(
                                History()->NewHistoryResult(nsnull, 0, nsnull));
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = result->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // Fill the result with a single result node for the bookmarks root.
  nsCOMPtr<nsNavHistoryResultNode> topNode;
  rv = ResultNodeForFolder(mRoot, getter_AddRefs(topNode));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(result->GetTopLevel()->AppendObject(topNode), 
                 NS_ERROR_OUT_OF_MEMORY);

  result->FilledAllResults();

  NS_STATIC_CAST(nsRefPtr<nsINavHistoryResult>, result).swap(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetFolderChildren(PRInt64 aFolder, PRUint32 aOptions,
                                  nsINavHistoryResult **aChildren)
{
  *aChildren = nsnull;

  nsRefPtr<nsNavHistoryResult> result(
                                History()->NewHistoryResult(nsnull, 0, nsnull));
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  result->SetBookmarkOptions(aOptions);

  nsresult rv = QueryFolderChildren(aFolder, aOptions, result->GetTopLevel());
  NS_ENSURE_SUCCESS(rv, rv);

  result->FilledAllResults();

  NS_STATIC_CAST(nsRefPtr<nsINavHistoryResult>, result).swap(*aChildren);
  return NS_OK;
}

// Special flag OR'd with *_CHILDREN to specify that we do _not_ want to
// build children for folders.

#define BUILD_CHILDREN_NO_RECURSE (1 << 31)

nsresult
nsNavBookmarks::QueryFolderChildren(PRInt64 aFolder, PRUint32 aOptions,
                                    nsCOMArray<nsNavHistoryResultNode>* aChildren)
{
  mozStorageTransaction transaction(DBConn(), PR_FALSE);
  PRBool noRecurse = (aOptions & BUILD_CHILDREN_NO_RECURSE) != 0;
  aOptions &= ~BUILD_CHILDREN_NO_RECURSE;

  mozIStorageStatement *statement;
  if (aOptions == ALL_CHILDREN) {
    aOptions = PR_UINT32_MAX;
  }
  PRBool includeQueries = (aOptions & QUERY_CHILDREN) != 0;
  PRBool includeItems = (aOptions & ITEM_CHILDREN) != 0;
  PRBool includeFolders = (aOptions & FOLDER_CHILDREN) != 0;

  if (includeFolders && (includeQueries || includeItems)) {
    statement = mDBGetChildren;
  } else if (includeItems || includeQueries) {
    statement = mDBGetItemChildren;
  } else if (includeFolders) {
    statement = mDBGetFolderChildren;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  {
    mozStorageStatementScoper scope(statement);

    rv = statement->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindInt64Parameter(1, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindInt64Parameter(2, PR_INT32_MAX);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    while (NS_SUCCEEDED(statement->ExecuteStep(&results)) && results) {
      PRBool isFolder = !statement->IsNull(kGetChildrenIndex_FolderChild);
      nsCOMPtr<nsNavHistoryResultNode> node;
      if (isFolder) {
        PRInt64 folder = statement->AsInt64(kGetChildrenIndex_FolderChild);
        rv = ResultNodeForFolder(folder, getter_AddRefs(node));
      } else {
        rv = History()->RowToResult(statement, PR_FALSE, getter_AddRefs(node));
        PRBool isQuery = IsQueryURI(node->URL());
        if ((isQuery && !includeQueries) || (!isQuery && !includeItems)) {
          continue;
        }
      }

      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(aChildren->AppendObject(node), NS_ERROR_OUT_OF_MEMORY);
    }
  }

  // Now build children for any folder children we just created.  This is
  // pretty cheap (only go down 1 level) and allows us to know whether
  // to draw a twisty.
  if (!noRecurse) {
    for (PRInt32 i = 0; i < aChildren->Count(); ++i) {
      nsNavHistoryResultNode *child = (*aChildren)[i];
      if (child->Type() == nsINavHistoryResultNode::RESULT_TYPE_FOLDER) {
        rv = child->BuildChildren(aOptions | BUILD_CHILDREN_NO_RECURSE);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
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
nsNavBookmarks::GetBookmarkCategories(nsIURI *aURI, PRUint32 *aCount,
                                      PRInt64 **aCategories)
{
  *aCount = 0;
  *aCategories = nsnull;

  mozStorageStatementScoper scope(mDBFindURIBookmarks);
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  nsresult rv = BindStatementURI(mDBFindURIBookmarks, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 arraySize = 8;
  PRInt64 *categories = NS_STATIC_CAST(PRInt64*,
                                       nsMemory::Alloc(arraySize * sizeof(PRInt64)));
  NS_ENSURE_TRUE(categories, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 count = 0;
  PRBool more;

  while (NS_SUCCEEDED((rv = mDBFindURIBookmarks->ExecuteStep(&more))) && more) {
    if (count >= arraySize) {
      arraySize <<= 1;
      PRInt64 *res = NS_STATIC_CAST(PRInt64*, nsMemory::Realloc(categories,
                                                 arraySize * sizeof(PRInt64)));
      if (!res) {
        delete categories;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      categories = res;
    }
    categories[count++] =
      mDBFindURIBookmarks->AsInt64(kFindBookmarksIndex_Parent);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = count;
  *aCategories = categories;

  return transaction.Commit();
}

NS_IMETHODIMP
nsNavBookmarks::BeginUpdateBatch()
{
  if (mBatchLevel++ == 0) {
    for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
      mObservers[i]->OnBeginUpdateBatch();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::EndUpdateBatch()
{
  if (--mBatchLevel == 0) {
    for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
      mObservers[i]->OnEndUpdateBatch();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::AddObserver(nsINavBookmarkObserver *aObserver)
{
  NS_ENSURE_TRUE(mObservers.AppendObject(aObserver), NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveObserver(nsINavBookmarkObserver *aObserver)
{
  mObservers.RemoveObject(aObserver);
  return NS_OK;
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
nsNavBookmarks::GetWantAllDetails(PRBool *aWant)
{
  *aWant = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnAddURI(nsIURI *aURI, PRTime aTime)
{
  // A new URI won't yet be bookmarked, so don't notify.
#ifdef DEBUG
  PRBool bookmarked;
  IsBookmarked(aURI, &bookmarked);
  NS_ASSERTION(!bookmarked, "New URI shouldn't be bookmarked!");
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnDeleteURI(nsIURI *aURI)
{
  // A deleted URI shouldn't be bookmarked.
#ifdef DEBUG
  PRBool bookmarked;
  IsBookmarked(aURI, &bookmarked);
  NS_ASSERTION(!bookmarked, "Deleted URI shouldn't be bookmarked!");
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnClearHistory()
{
  // Nothing being cleared should be bookmarked.
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnPageChanged(nsIURI *aURI, PRUint32 aWhat,
                              const nsAString &aValue)
{
  if (aWhat == ATTRIBUTE_TITLE) {
    for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
      mObservers[i]->OnItemChanged(aURI, NS_LITERAL_CSTRING("title"));
    }
  }

  return NS_OK;
}
