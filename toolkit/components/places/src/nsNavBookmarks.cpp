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
#include "nsIDynamicContainer.h"
#include "nsUnicharUtils.h"
#include "nsFaviconService.h"
#include "nsAnnotationService.h"
#include "nsPrintfCString.h"
#include "nsAutoLock.h"
#include "nsIUUIDGenerator.h"
#include "prmem.h"
#include "prprf.h"

const PRInt32 nsNavBookmarks::kFindBookmarksIndex_ID = 0;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Type = 1;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_ForeignKey = 2;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Parent = 3;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Position = 4;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Title = 5;

// These columns sit to the right of the kGetInfoIndex_* columns.
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Position = 11;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Type = 12;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_ForeignKey = 13;

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

nsNavBookmarks* nsNavBookmarks::sInstance = nsnull;

#define BOOKMARKS_ANNO_PREFIX "bookmarks/"
#define BOOKMARKS_TOOLBAR_FOLDER_ANNO NS_LITERAL_CSTRING(BOOKMARKS_ANNO_PREFIX "toolbarFolder")
#define GUID_ANNO NS_LITERAL_CSTRING("placesInternal/GUID")
#define READ_ONLY_ANNO NS_LITERAL_CSTRING("placesInternal/READ_ONLY")

nsNavBookmarks::nsNavBookmarks()
  : mItemCount(0), mRoot(0), mBookmarksRoot(0), mTagRoot(0), mToolbarFolder(0), mBatchLevel(0),
    mLock(nsnull), mBatchHasTransaction(PR_FALSE)
{
  NS_ASSERTION(!sInstance, "Multiple nsNavBookmarks instances!");
  sInstance = this;
}

nsNavBookmarks::~nsNavBookmarks()
{
  NS_ASSERTION(sInstance == this, "Expected sInstance == this");
  sInstance = nsnull;
  if (mLock)
    PR_DestroyLock(mLock);
}

NS_IMPL_ISUPPORTS3(nsNavBookmarks,
                   nsINavBookmarksService,
                   nsINavHistoryObserver,
                   nsIAnnotationObserver)

nsresult
nsNavBookmarks::Init()
{
  nsNavHistory *history = History();
  NS_ENSURE_TRUE(history, NS_ERROR_UNEXPECTED);
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);
  nsresult rv;

  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT id FROM moz_bookmarks WHERE type = ?1 AND parent IS NULL"),
                                 getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->BindInt32Parameter(0, TYPE_FOLDER);
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
      "chrome://global/content/places/places.properties",
      getter_AddRefs(mBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBFindURIBookmarks
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT a.id "
      "FROM moz_bookmarks a, moz_places h "
      "WHERE h.url = ?1 AND a.fk = h.id and a.type = ?2 "
      "ORDER BY MAX(COALESCE(a.lastModified, 0), a.dateAdded) DESC"),
    getter_AddRefs(mDBFindURIBookmarks));
  NS_ENSURE_SUCCESS(rv, rv);

  // Construct a result where the first columns exactly match those returned by
  // mDBGetURLPageInfo, and additionally contains columns for position,
  // item_child, and folder_child from moz_bookmarks.
  // Results are kGetInfoIndex_*
  nsCAutoString selectChildren(
    NS_LITERAL_CSTRING("SELECT h.id, h.url, a.title, "
      "h.rev_host, h.visit_count, "
      "(SELECT MAX(visit_date) FROM moz_historyvisits WHERE place_id = h.id), "
      "f.url, null, a.id, "
      "a.dateAdded, a.lastModified, "
      "a.position, a.type, a.fk "
     "FROM moz_bookmarks a "
     "LEFT JOIN moz_places h ON a.fk = h.id "
     "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
     "WHERE a.parent = ?1 AND a.position >= ?2 AND a.position <= ?3"
     " ORDER BY a.position"));

  // mDBGetChildren: select all children of a given folder, sorted by position
  rv = dbConn->CreateStatement(selectChildren,
                               getter_AddRefs(mDBGetChildren));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBFolderCount: count all of the children of a given folder
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT COUNT(*) FROM moz_bookmarks WHERE parent = ?1"),
                               getter_AddRefs(mDBFolderCount));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT position FROM moz_bookmarks WHERE id = ?1"),
                               getter_AddRefs(mDBGetItemIndex));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT id, fk, type FROM moz_bookmarks WHERE parent = ?1 AND position = ?2"),
                               getter_AddRefs(mDBGetChildAt));
  NS_ENSURE_SUCCESS(rv, rv);

  // get bookmark/folder/separator properties 
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT b.id, (SELECT url from moz_places WHERE id = b.fk), b.title, b.position, b.fk, b.parent, b.type, b.folder_type, b.dateAdded, b.lastModified "
      "FROM moz_bookmarks b "
      "WHERE b.id = ?1"),
    getter_AddRefs(mDBGetItemProperties));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT item_id FROM moz_items_annos "
      "WHERE content = ?1 "
      "LIMIT 1"),
    getter_AddRefs(mDBGetItemIdForGUID));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetRedirectDestinations
  // input = page ID, time threshold; output = unique ID input has redirected to
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT dest_v.place_id "
      "FROM moz_historyvisits source_v "
      "LEFT JOIN moz_historyvisits dest_v ON dest_v.from_visit = source_v.id "
      "WHERE source_v.place_id = ?1 "
      "AND source_v.visit_date >= ?2 "
      "AND (dest_v.visit_type = 5 OR dest_v.visit_type = 6) "
      "GROUP BY dest_v.place_id"),
    getter_AddRefs(mDBGetRedirectDestinations));
  NS_ENSURE_SUCCESS(rv, rv);

  FillBookmarksHash();

  // must be last: This may cause bookmarks to be imported, which will exercise
  // most of the bookmark system

  // get keyword text for bookmark id
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT k.keyword FROM moz_bookmarks b "
      "JOIN moz_keywords k ON k.id = b.keyword_id "
      "WHERE b.id = ?1"),
    getter_AddRefs(mDBGetKeywordForBookmark));
  NS_ENSURE_SUCCESS(rv, rv);
  // get keyword text for URI (must be a bookmarked URI)
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT k.keyword " 
      "FROM moz_places p "
      "JOIN moz_bookmarks b ON b.fk = p.id "
      "JOIN moz_keywords k ON k.id = b.keyword_id "
      "WHERE p.url = ?1"),
    getter_AddRefs(mDBGetKeywordForURI));
  NS_ENSURE_SUCCESS(rv, rv);
  // get URI for keyword
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT p.url FROM moz_keywords k "
      "JOIN moz_bookmarks b ON b.keyword_id = k.id "
      "JOIN moz_places p ON b.fk = p.id "
      "WHERE k.keyword = ?1"),
    getter_AddRefs(mDBGetURIForKeyword));
  NS_ENSURE_SUCCESS(rv, rv);

  // generate a new GUID base for this session
  nsCOMPtr<nsIUUIDGenerator> uuidgen = do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsID GUID;
  rv = uuidgen->GenerateUUIDInPlace(&GUID);
  NS_ENSURE_SUCCESS(rv, rv);
  char* GUIDChars = GUID.ToString();
  NS_ENSURE_TRUE(GUIDChars, NS_ERROR_OUT_OF_MEMORY);
  mGUIDBase.Assign(NS_ConvertASCIItoUTF16(GUIDChars));
  PR_Free(GUIDChars);

  rv = InitRoots();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = InitToolbarFolder();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  mLock = PR_NewLock();
  NS_ENSURE_TRUE(mLock, NS_ERROR_OUT_OF_MEMORY);

  // Temporary migration code for bug 396300
  nsCOMPtr<mozIStorageStatement> moveUnfiledBookmarks;
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_bookmarks SET parent = ?1 WHERE type = ?2 AND parent=?3"),
                               getter_AddRefs(moveUnfiledBookmarks));
  rv = moveUnfiledBookmarks->BindInt64Parameter(0, mUnfiledRoot);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moveUnfiledBookmarks->BindInt32Parameter(1, TYPE_BOOKMARK);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moveUnfiledBookmarks->BindInt32Parameter(2, mRoot);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moveUnfiledBookmarks->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);

  // allows us to notify on title changes. MUST BE LAST so it is impossible
  // to fail after this call, or the history service will have a reference to
  // us and we won't go away.
  history->AddObserver(this, PR_FALSE);
  annosvc->AddObserver(this);

  // DO NOT PUT STUFF HERE that can fail. See observer comment above.

  return NS_OK;
}

// nsNavBookmarks::InitTables
//
//    All commands that initialize the schema of the DB go in here. This is
//    called from history init before the dummy DB connection is started that
//    will prevent us from modifying the schema.

nsresult // static
nsNavBookmarks::InitTables(mozIStorageConnection* aDBConn)
{
  PRBool exists;
  nsresult rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
    // The fk column is for "foreign key". It contains ids from moz_places
    // if the row is a bookmark.
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks ("
        "id INTEGER PRIMARY KEY,"
        "type INTEGER, "
        "fk INTEGER DEFAULT NULL, "
        "parent INTEGER, "
        "position INTEGER, "
        "title LONGVARCHAR, "
        "keyword_id INTEGER, "
        "folder_type TEXT, "
        "dateAdded INTEGER, " 
        "lastModified INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);

    // this index will make it faster to determine if a given item is
    // bookmarked (used by history queries and vacuuming, for example)
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE INDEX moz_bookmarks_itemindex ON moz_bookmarks (fk)"));
    NS_ENSURE_SUCCESS(rv, rv);

    // the most common operation is to find the children given a parent
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE INDEX moz_bookmarks_parentindex ON moz_bookmarks (parent)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // moz_bookmarks_roots
  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_roots"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!exists) {
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks_roots ("
        "root_name VARCHAR(16) UNIQUE, "
        "folder_id INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // moz_keywords
  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_keywords"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE TABLE moz_keywords ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "keyword TEXT UNIQUE)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
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
    delete static_cast<RenumberItem*>(items[i]);
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
  nsCOMPtr<mozIStorageStatement> getRootStatement;
  nsresult rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("SELECT folder_id FROM moz_bookmarks_roots WHERE root_name = ?1"),
                                 getter_AddRefs(getRootStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool importDefaults = PR_FALSE;
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("places"), &mRoot, 0, &importDefaults);
  NS_ENSURE_SUCCESS(rv, rv);

  getRootStatement->Reset();
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("menu"), &mBookmarksRoot, mRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  getRootStatement->Reset();
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("tags"), &mTagRoot, mRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  getRootStatement->Reset();
  rv = CreateRoot(getRootStatement, NS_LITERAL_CSTRING("unfiled"), &mUnfiledRoot, mRoot, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  if (importDefaults) {
    // when there is no places root, we should define the hierarchy by
    // importing the default one.
    rv = InitDefaults();
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
  // give bookmarks root folder a title "Bookmarks"
  nsXPIDLString bookmarksTitle;
  nsresult rv = mBundle->GetStringFromName(NS_LITERAL_STRING("PlacesBookmarksRootTitle").get(),
                                           getter_Copies(bookmarksTitle));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetItemTitle(mBookmarksRoot, bookmarksTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  // create toolbar folder, parent bookmarks root, entitled "Bookmarks Toolbar Folder"
  PRInt64 toolbarId;
  nsXPIDLString toolbarTitle;
  rv = mBundle->GetStringFromName(NS_LITERAL_STRING("PlacesBookmarksToolbarTitle").get(),
                                  getter_Copies(toolbarTitle));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = CreateFolder(mBookmarksRoot, toolbarTitle,
                    nsINavBookmarksService::DEFAULT_INDEX, &toolbarId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetToolbarFolder(toolbarId);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/**
 * Initialize the toolbar folder
 */
nsresult
nsNavBookmarks::InitToolbarFolder()
{
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);

  nsTArray<PRInt64> folders;
  nsresult rv = annosvc->GetItemsWithAnnotationTArray(BOOKMARKS_TOOLBAR_FOLDER_ANNO,
                                                      &folders);
  if (NS_FAILED(rv) || folders.Length() == 0)
    mToolbarFolder = -1;
  else
    mToolbarFolder = folders[0];

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
  rv = CreateFolder(aParentID, NS_LITERAL_STRING(""), nsINavBookmarksService::DEFAULT_INDEX, aID);
  NS_ENSURE_SUCCESS(rv, rv);

  // save root ID
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_roots (root_name, folder_id) VALUES (?1, ?2)"),
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


// nsNavBookmarks::FillBookmarksHash
//
//    This initializes the bookmarks hashtable that tells us which bookmark
//    a given URI redirects to. This hashtable includes all URIs that
//    redirect to bookmarks.
//
//    This is called from the bookmark init function and so is wrapped
//    in that transaction (for better performance).

nsresult
nsNavBookmarks::FillBookmarksHash()
{
  PRBool hasMore;

  // first init the hashtable
  NS_ENSURE_TRUE(mBookmarksHash.Init(1024), NS_ERROR_OUT_OF_MEMORY);

  // first populate the table with all bookmarks
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id "
      "FROM moz_bookmarks b "
      "LEFT JOIN moz_places h ON b.fk = h.id where b.type = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(0, TYPE_BOOKMARK);
  NS_ENSURE_SUCCESS(rv, rv);
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
    PRInt64 pageID;
    rv = statement->GetInt64(0, &pageID);
    NS_ENSURE_TRUE(mBookmarksHash.Put(pageID, pageID), NS_ERROR_OUT_OF_MEMORY);
  }

  // Find all pages h2 that have been redirected to from a bookmarked URI:
  //    bookmarked -> url (h1)         url (h2)
  //                    |                 ^
  //                    .                 |
  //                 visit (v1) -> destination visit (v2)
  // This should catch most redirects, which are only one level. More levels of
  // redirection will be handled separately.
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT v1.place_id, v2.place_id "
      "FROM moz_bookmarks b "
      "LEFT JOIN moz_historyvisits v1 on b.fk = v1.place_id "
      "LEFT JOIN moz_historyvisits v2 on v2.from_visit = v1.id "
      "WHERE b.fk IS NOT NULL AND b.type = ?1 "
      "AND v2.visit_type = 5 OR v2.visit_type = 6 " // perm. or temp. RDRs
      "GROUP BY v2.place_id"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(0, TYPE_BOOKMARK);
  NS_ENSURE_SUCCESS(rv, rv);
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
    PRInt64 fromId, toId;
    statement->GetInt64(0, &fromId);
    statement->GetInt64(1, &toId);

    NS_ENSURE_TRUE(mBookmarksHash.Put(toId, fromId), NS_ERROR_OUT_OF_MEMORY);

    // handle redirects deeper than one level
    rv = RecursiveAddBookmarkHash(fromId, toId, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsNavBookmarks::AddBookmarkToHash
//
//    Given a bookmark that was potentially added, this goes through all
//    redirects that this page may have resulted in and adds them to our hash.
//    Note that this takes the ID of the URL in the history system, which we
//    generally have when calling this function and which makes it faster.
//
//    For better performance, this call should be in a DB transaction.
//
//    @see RecursiveAddBookmarkHash

nsresult
nsNavBookmarks::AddBookmarkToHash(PRInt64 aBookmarkId, PRTime aMinTime)
{
  // this function might be called before our hashtable is initialized (for
  // example, on history import), just ignore these, we'll pick up the add when
  // the hashtable is initialized later
  if (! mBookmarksHash.IsInitialized())
    return NS_OK;
  if (! mBookmarksHash.Put(aBookmarkId, aBookmarkId))
    return NS_ERROR_OUT_OF_MEMORY;
  return RecursiveAddBookmarkHash(aBookmarkId, aBookmarkId, aMinTime);
}


// nsNavBookmkars::RecursiveAddBookmarkHash
//
//    Used to add a new level of redirect information to the bookmark hash.
//    Given a source bookmark 'aBookmark' and 'aCurrentSource' that has already
//    been added to the hashtable, this will add all redirect destinations of
//    'aCurrentSource'. Will call itself recursively to walk down the chain.
//
//    'aMinTime' is the minimum time to consider visits from. Visits previous
//    to this will not be considered. This allows the search to be much more
//    efficient if you know something happened recently. Use 0 for the min time
//    to search all history for redirects.

nsresult
nsNavBookmarks::RecursiveAddBookmarkHash(PRInt64 aBookmarkID,
                                         PRInt64 aCurrentSource,
                                         PRTime aMinTime)
{
  nsresult rv;
  nsTArray<PRInt64> found;

  // scope for the DB statement. The statement must be reset by the time we
  // recursively call ourselves again, because our recursive call will use the
  // same statement.
  {
    mozStorageStatementScoper scoper(mDBGetRedirectDestinations);
    rv = mDBGetRedirectDestinations->BindInt64Parameter(0, aCurrentSource);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetRedirectDestinations->BindInt64Parameter(1, aMinTime);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    while (NS_SUCCEEDED(mDBGetRedirectDestinations->ExecuteStep(&hasMore)) &&
           hasMore) {

      // add this newly found redirect destination to the hashtable
      PRInt64 curID;
      rv = mDBGetRedirectDestinations->GetInt64(0, &curID);
      NS_ENSURE_SUCCESS(rv, rv);

      // It is very important we ignore anything already in our hashtable. It
      // is actually pretty common to get loops of redirects. For example,
      // a restricted page will redirect you to a login page, which will
      // redirect you to the restricted page again with the proper cookie.
      PRInt64 alreadyExistingOne;
      if (mBookmarksHash.Get(curID, &alreadyExistingOne))
        continue;

      if (! mBookmarksHash.Put(curID, aBookmarkID))
        return NS_ERROR_OUT_OF_MEMORY;

      // save for recursion later
      found.AppendElement(curID);
    }
  }

  // recurse on each found item now that we're done with the statement
  for (PRUint32 i = 0; i < found.Length(); i ++) {
    rv = RecursiveAddBookmarkHash(aBookmarkID, found[i], aMinTime);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsNavBookmarks::UpdateBookmarkHashOnRemove
//
//    Call this when a bookmark is removed. It will see if the bookmark still
//    exists anywhere in the system, and, if not, remove all references to it
//    in the bookmark hashtable.
//
//    The callback takes a pointer to what bookmark is being removed (as
//    an Int64 history page ID) as the userArg and removes all redirect
//    destinations that reference it.

PR_STATIC_CALLBACK(PLDHashOperator)
RemoveBookmarkHashCallback(nsTrimInt64HashKey::KeyType aKey,
                           PRInt64& aBookmark, void* aUserArg)
{
  const PRInt64* removeThisOne = reinterpret_cast<const PRInt64*>(aUserArg);
  if (aBookmark == *removeThisOne)
    return PL_DHASH_REMOVE;
  return PL_DHASH_NEXT;
}
nsresult
nsNavBookmarks::UpdateBookmarkHashOnRemove(PRInt64 aPlaceId)
{
  // note we have to use the DB version here since the hashtable may be
  // out-of-date
  PRBool inDB;
  nsresult rv = IsBookmarkedInDatabase(aPlaceId, &inDB);
  NS_ENSURE_SUCCESS(rv, rv);
  if (inDB)
    return NS_OK; // bookmark still exists, don't need to update hashtable

  // remove it
  mBookmarksHash.Enumerate(RemoveBookmarkHashCallback,
                           reinterpret_cast<void*>(&aPlaceId));
  return NS_OK;
}


// nsNavBookmarks::IsBookmarkedInDatabase
//
//    This checks to see if the specified URI is actually bookmarked, bypassing
//    our hashtable. Normal IsBookmarked checks just use the hashtable.

nsresult
nsNavBookmarks::IsBookmarkedInDatabase(PRInt64 aPlaceId,
                                       PRBool *aIsBookmarked)
{
  // we'll just select position since it's just an int32 and may be faster.
  // We don't actually care about the data, just whether there is any.
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT position FROM moz_bookmarks WHERE fk = ?1 AND type = ?2"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, aPlaceId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(1, TYPE_BOOKMARK);
  NS_ENSURE_SUCCESS(rv, rv);

  return statement->ExecuteStep(aIsBookmarked);
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

      if (mDBGetChildren->AsInt32(kGetChildrenIndex_Type) == TYPE_BOOKMARK) {
        nsCAutoString spec;
        mDBGetChildren->GetUTF8String(nsNavHistory::kGetInfoIndex_URL, spec);
        rv = NS_NewURI(getter_AddRefs(item->itemURI), spec, nsnull);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else {
        item->folderChild = mDBGetChildren->AsInt64(kGetChildrenIndex_ForeignKey);
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

  nsBookmarksUpdateBatcher batch;

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
nsNavBookmarks::GetToolbarFolder(PRInt64 *aFolderId)
{
  *aFolderId = mToolbarFolder;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::SetToolbarFolder(PRInt64 aFolderId)
{
  // XXX - validate that input is a valid folder id
  if (aFolderId < 0)
    return NS_ERROR_INVALID_ARG;
  if (aFolderId == mToolbarFolder)
    return NS_OK;

  nsresult rv;
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);

  if (mToolbarFolder != 0) {
    rv = annosvc->RemoveItemAnnotation(mToolbarFolder,
                                       BOOKMARKS_TOOLBAR_FOLDER_ANNO);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = annosvc->SetItemAnnotationInt32(aFolderId, BOOKMARKS_TOOLBAR_FOLDER_ANNO,
                                       1, 0, nsIAnnotationService::EXPIRE_NEVER);
  NS_ENSURE_SUCCESS(rv, rv);

  // update local
  mToolbarFolder = aFolderId;

  // notify observers
  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemChanged(mToolbarFolder, NS_LITERAL_CSTRING("became_toolbar_folder"),
                                    PR_FALSE, EmptyCString()));
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetTagRoot(PRInt64 *aRoot)
{
  *aRoot = mTagRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetUnfiledRoot(PRInt64 *aRoot)
{
  *aRoot = mUnfiledRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::InsertBookmark(PRInt64 aFolder, nsIURI *aItem, PRInt32 aIndex,
                               const nsAString& aTitle,
                               PRInt64 *aNewBookmarkId)
{
  // You can pass -1 to indicate append, but no other negative number is allowed
  if (aIndex < nsINavBookmarksService::DEFAULT_INDEX)
    return NS_ERROR_INVALID_ARG;
  NS_ENSURE_ARG_POINTER(aNewBookmarkId);

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 childID;
  nsresult rv = History()->GetUrlIdFor(aItem, &childID, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 index = (aIndex == -1) ? FolderCount(aFolder) : aIndex;

  rv = AdjustIndices(aFolder, index, PR_INT32_MAX, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> statement;
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks "
                               "(fk, type, parent, position, title, dateAdded) "
                               "VALUES (?1, ?2, ?3, ?4, ?5, ?6)"),
                               getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, childID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(1, TYPE_BOOKMARK);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(2, aFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(3, index);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(4, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(5, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // get row id of the new bookmark
  PRInt64 rowId;
  rv = dbConn->GetLastInsertRowID(&rowId);
  NS_ENSURE_SUCCESS(rv, rv);
  *aNewBookmarkId = rowId;

  rv = SetItemLastModified(aFolder, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  AddBookmarkToHash(childID, 0);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemAdded(rowId, aFolder, index))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveItem(PRInt64 aItemId)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt32 childIndex;
  PRInt64 placeId, folderId;
  nsCAutoString buffer;

  // First, remove item annotations
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = annosvc->RemoveItemAnnotations(aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  { // scoping to ensure the statement gets reset
    mozStorageStatementScoper scope(mDBGetItemProperties);
    mDBGetItemProperties->BindInt64Parameter(0, aItemId);

    PRBool results;
    rv = mDBGetItemProperties->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!results)
      return NS_ERROR_INVALID_ARG; // invalid bookmark id

    childIndex = mDBGetItemProperties->AsInt32(kGetItemPropertiesIndex_Position);
    placeId = mDBGetItemProperties->AsInt64(kGetItemPropertiesIndex_PlaceID);
    folderId = mDBGetItemProperties->AsInt64(kGetItemPropertiesIndex_Parent);
  }

  buffer.AssignLiteral("DELETE FROM moz_bookmarks WHERE id = ");
  buffer.AppendInt(aItemId);

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  if (childIndex != -1) {
    rv = AdjustIndices(folderId, childIndex + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = SetItemLastModified(folderId, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = UpdateBookmarkHashOnRemove(placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemRemoved(aItemId, folderId, childIndex))

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::CreateFolder(PRInt64 aParent, const nsAString &aName,
                             PRInt32 aIndex, PRInt64 *aNewFolder)
{
  // CreateContainerWithID returns the index of the new folder, but that's not
  // used here.  To avoid any risk of corrupting data should this function
  // be changed, we'll use a local variable to hold it.  The PR_TRUE argument
  // will cause notifications to be sent to bookmark observers.
  PRInt32 localIndex = aIndex;
  return CreateContainerWithID(-1, aParent, aName, EmptyString(), PR_TRUE,
                               &localIndex, aNewFolder);
}

NS_IMETHODIMP
nsNavBookmarks::CreateDynamicContainer(PRInt64 aParent, const nsAString &aName,
                                       const nsAString &aContractId,
                                       PRInt32 aIndex,
                                       PRInt64 *aNewFolder)
{
  if (aContractId.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  return CreateContainerWithID(-1, aParent, aName, aContractId, PR_FALSE,
                               &aIndex, aNewFolder);
}

NS_IMETHODIMP
nsNavBookmarks::GetFolderReadonly(PRInt64 aFolder, PRBool *aResult)
{
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  return annosvc->ItemHasAnnotation(aFolder, READ_ONLY_ANNO, aResult);
}

NS_IMETHODIMP
nsNavBookmarks::SetFolderReadonly(PRInt64 aFolder, PRBool aReadOnly)
{
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  if (aReadOnly) {
    return annosvc->SetItemAnnotationInt32(aFolder, READ_ONLY_ANNO, 1,
                                           0,
                                           nsAnnotationService::EXPIRE_NEVER);
  }
  else {
    PRBool hasAnno;
    nsresult rv = annosvc->ItemHasAnnotation(aFolder, READ_ONLY_ANNO, &hasAnno);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasAnno)
      return annosvc->RemoveItemAnnotation(aFolder, READ_ONLY_ANNO);
  }
  return NS_OK;
}

nsresult
nsNavBookmarks::CreateContainerWithID(PRInt64 aItemId, PRInt64 aParent,
                                      const nsAString& aName,
                                      const nsAString& aContractId,
                                      PRBool aIsBookmarkFolder,
                                      PRInt32* aIndex, PRInt64* aNewFolder)
{
  // You can pass -1 to indicate append, but no other negative number is allowed
  if (*aIndex < -1)
    return NS_ERROR_INVALID_ARG;

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt32 index = (*aIndex == -1) ? FolderCount(aParent) : *aIndex;

  nsresult rv = AdjustIndices(aParent, index, PR_INT32_MAX, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> statement;
  if (aItemId == -1) {
    rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks (title, type, parent, position, folder_type, dateAdded) VALUES (?1, ?2, ?3, ?4, ?5, ?6)"),
                                 getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks (id, title, type, parent, position, folder_type, dateAdded) VALUES (?7, ?1, ?2, ?3, ?4, ?5, ?6)"),
                                 getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindInt64Parameter(6, aItemId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = statement->BindStringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 containerType =
    aIsBookmarkFolder ? TYPE_FOLDER : TYPE_DYNAMIC_CONTAINER;

  rv = statement->BindInt32Parameter(1, containerType);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(2, aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(3, index);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(4, aContractId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(5, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 id;
  rv = dbConn->GetLastInsertRowID(&id);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetItemLastModified(aParent, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemAdded(id, aParent, index))

  *aIndex = index;
  *aNewFolder = id;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::InsertSeparator(PRInt64 aParent, PRInt32 aIndex,
                                PRInt64* aNewItemId)
{
  // You can pass -1 to indicate append, but no other negative number is
  // allowed
  if (aIndex < -1)
    return NS_ERROR_INVALID_ARG;

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt32 index = (aIndex == -1) ? FolderCount(aParent) : aIndex;

  nsresult rv = AdjustIndices(aParent, index, PR_INT32_MAX, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> statement;
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks "
                                          "(type, parent, position, dateAdded) VALUES (?1, ?2, ?3, ?4)"),
                               getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, TYPE_SEPARATOR);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(1, aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(2, index);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(3, PR_Now()); 
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 rowId;
  rv = dbConn->GetLastInsertRowID(&rowId);
  NS_ENSURE_SUCCESS(rv, rv);
  *aNewItemId = rowId;

  rv = SetItemLastModified(aParent, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemAdded(rowId, aParent, index))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveChildAt(PRInt64 aParent, PRInt32 aIndex)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);
  nsresult rv;
  PRInt64 id;
  PRInt32 type;

  {
    mozStorageStatementScoper scope(mDBGetChildAt);
    rv = mDBGetChildAt->BindInt64Parameter(0, aParent);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetChildAt->BindInt32Parameter(1, aIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    rv = mDBGetChildAt->ExecuteStep(&hasMore);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasMore) {
      // Child doesn't exist
      return NS_ERROR_INVALID_ARG;
    }

    id = mDBGetChildAt->AsInt64(0);
    type = mDBGetChildAt->AsInt32(2);
  }

  if (type == TYPE_BOOKMARK || type == TYPE_SEPARATOR) {
    // Commit this transaction so that we don't notify observers mid-tranaction
    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    return RemoveItem(id);
  }
  else if (type == TYPE_FOLDER) {
    // Commit this transaction so that we don't notify observers mid-tranaction
    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    return RemoveFolder(id);
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult 
nsNavBookmarks::GetParentAndIndexOfFolder(PRInt64 aFolder, PRInt64* aParent, 
                                          PRInt32* aIndex)
{
  nsCAutoString buffer;
  buffer.AssignLiteral("SELECT parent, position FROM moz_bookmarks WHERE id = ");
  buffer.AppendInt(aFolder);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = DBConn()->CreateStatement(buffer, getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = statement->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!results) {
    return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
  }

  *aParent = statement->AsInt64(0);
  *aIndex = statement->AsInt32(1);
  
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveFolder(PRInt64 aFolder)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  // First, remove item annotations
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = annosvc->RemoveItemAnnotations(aFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 parent;
  PRInt32 index;
  nsCAutoString folderType;
  {
    mozStorageStatementScoper scope(mDBGetItemProperties);
    rv = mDBGetItemProperties->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = mDBGetItemProperties->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!results) {
      return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
    }

    parent = mDBGetItemProperties->AsInt64(kGetItemPropertiesIndex_Parent);
    index = mDBGetItemProperties->AsInt32(kGetItemPropertiesIndex_Position);
    rv = mDBGetItemProperties->GetUTF8String(kGetItemPropertiesIndex_ServiceContractId, folderType);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If this is a container bookmark, try to notify its service.
  if (folderType.Length() > 0) {
    // There is a type associated with this folder; it's a livemark.
    nsCOMPtr<nsIDynamicContainer> bmcServ = do_GetService(folderType.get());
    if (bmcServ) {
      rv = bmcServ->OnContainerRemoving(aFolder);
      if (NS_FAILED(rv))
        NS_WARNING("Remove folder container notification failed.");
    }
  }

  // Remove all of the folder's children
  RemoveFolderChildren(aFolder);

  // Remove the folder from its parent
  nsCAutoString buffer;
  buffer.AssignLiteral("DELETE FROM moz_bookmarks WHERE id = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AdjustIndices(parent, index + 1, PR_INT32_MAX, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetItemLastModified(parent, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  if (aFolder == mToolbarFolder) {
    mToolbarFolder = 0;
  }

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemRemoved(aFolder, parent, index))

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsNavBookmarks::RemoveFolderTransaction, nsITransaction)

NS_IMETHODIMP
nsNavBookmarks::GetRemoveFolderTransaction(PRInt64 aFolder, nsITransaction** aResult)
{
  // Create and initialize a RemoveFolderTransaction object that can be used to
  // recreate the folder safely later. 

  nsAutoString title;
  nsresult rv = GetItemTitle(aFolder, title);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 parent;
  PRInt32 index;
  rv = GetParentAndIndexOfFolder(aFolder, &parent, &index);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString type;
  rv = GetFolderType(aFolder, type);
  NS_ENSURE_SUCCESS(rv, rv);

  RemoveFolderTransaction* rft = 
    new RemoveFolderTransaction(aFolder, parent, title, index, NS_ConvertUTF8toUTF16(type));
  if (!rft)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult = rft);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveFolderChildren(PRInt64 aFolder)
{
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  nsTArray<PRInt64> folderChildren;
  nsTArray<PRInt64> itemChildren; // bookmarks / separators
  nsresult rv;
  {
    mozStorageStatementScoper scope(mDBGetChildren);
    rv = mDBGetChildren->BindInt64Parameter(0, aFolder);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetChildren->BindInt32Parameter(1, 0);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetChildren->BindInt32Parameter(2, PR_INT32_MAX);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    while (NS_SUCCEEDED(mDBGetChildren->ExecuteStep(&hasMore)) && hasMore) {
      PRInt32 type = mDBGetChildren->AsInt32(kGetChildrenIndex_Type);
      if (type == TYPE_FOLDER) {
        // folder
        folderChildren.AppendElement(
            mDBGetChildren->AsInt64(nsNavHistory::kGetInfoIndex_ItemId));
      } else {
        // bookmarks / separators
        itemChildren.AppendElement(mDBGetChildren->AsInt64(nsNavHistory::kGetInfoIndex_ItemId));
      }
    }
  }

  PRUint32 i;

  // remove folders
  for (i = 0; i < folderChildren.Length(); ++i) {
    rv = RemoveFolder(folderChildren[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // remove items
  for (i = 0; i < itemChildren.Length(); ++i) {
    rv = RemoveItem(itemChildren[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  transaction.Commit();
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::MoveItem(PRInt64 aItemId, PRInt64 aNewParent, PRInt32 aIndex)
{
  // You can pass -1 to indicate append, but no other negative number is allowed
  if (aIndex < -1)
    return NS_ERROR_INVALID_ARG;

  // Disallow making a folder it's own parent.
  if (aItemId == aNewParent)
    return NS_ERROR_INVALID_ARG;

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  // get item properties
  nsresult rv;
  PRInt64 oldParent;
  PRInt32 oldIndex;
  PRInt32 itemType;
  nsCAutoString folderType;
  {
    mozStorageStatementScoper scope(mDBGetItemProperties);
    rv = mDBGetItemProperties->BindInt64Parameter(0, aItemId);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = mDBGetItemProperties->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!results) {
      return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
    }

    oldParent = mDBGetItemProperties->AsInt64(kGetItemPropertiesIndex_Parent);
    oldIndex = mDBGetItemProperties->AsInt32(kGetItemPropertiesIndex_Position);
    itemType = mDBGetItemProperties->AsInt32(kGetItemPropertiesIndex_Type);
    if (itemType == TYPE_FOLDER) {
      rv = mDBGetItemProperties->GetUTF8String(kGetItemPropertiesIndex_ServiceContractId,
                                               folderType);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // if parent and index are the same, nothing to do
  if (oldParent == aNewParent && oldIndex == aIndex)
    return NS_OK;

  // Make sure aNewParent is not aFolder or a subfolder of aFolder
  if (itemType == TYPE_FOLDER) {
    PRInt64 p = aNewParent;

    while (p) {
      mozStorageStatementScoper scope(mDBGetItemProperties);
      if (p == aItemId) {
        return NS_ERROR_INVALID_ARG;
      }

      rv = mDBGetItemProperties->BindInt64Parameter(0, p);
      NS_ENSURE_SUCCESS(rv, rv);

      PRBool results;
      rv = mDBGetItemProperties->ExecuteStep(&results);
      NS_ENSURE_SUCCESS(rv, rv);
      p = results ? mDBGetItemProperties->AsInt64(kGetItemPropertiesIndex_Parent) : 0;
    }
  }

  // calculate new index
  PRInt32 newIndex;
  if (aIndex == -1) {
    newIndex = FolderCount(aNewParent);
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
    } else {
      rv = AdjustIndices(oldParent, oldIndex + 1, newIndex, -1);
    }
  } else {
    // We're moving between containers, so this happens in two steps.
    // First, fill the hole from the removal from the old parent.
    rv = AdjustIndices(oldParent, oldIndex + 1, PR_INT32_MAX, -1);
    NS_ENSURE_SUCCESS(rv, rv);
    // Now, make room in the new parent for the insertion.
    rv = AdjustIndices(aNewParent, newIndex + 1, PR_INT32_MAX, 1);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // update parent/index fields
  nsCAutoString buffer;
  buffer.AssignLiteral("UPDATE moz_bookmarks SET ");
  if (aNewParent != oldParent) {
    buffer.AppendLiteral(" parent = ");
    buffer.AppendInt(aNewParent);
  }
  if (newIndex != oldIndex) {
    if (aNewParent != oldParent)
      buffer.AppendLiteral(", ");
    buffer.AppendLiteral(" position = ");
    buffer.AppendInt(newIndex);
  }
  buffer.AppendLiteral(" WHERE id = ");
  buffer.AppendInt(aItemId);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  PRTime now = PR_Now();
  rv = SetItemLastModified(oldParent, now);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetItemLastModified(aNewParent, now);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // notify bookmark observers
  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemMoved(aItemId, oldParent, oldIndex, aNewParent,
                                  newIndex))

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

NS_IMETHODIMP
nsNavBookmarks::GetChildFolder(PRInt64 aFolder, const nsAString& aSubFolder,
                               PRInt64* _result)
{
  // note: we allow empty folder names
  nsresult rv;
  if (aFolder == 0)
    return NS_ERROR_INVALID_ARG;

  // If this gets used a lot, we'll want a precompiled statement
  nsCAutoString getChildFolderQuery =
    NS_LITERAL_CSTRING("SELECT id "
                       "FROM moz_bookmarks "
                       "WHERE parent = ?1 AND type = ") +
    nsPrintfCString("%d", TYPE_FOLDER) +
    NS_LITERAL_CSTRING(" AND title = ?2");
  nsCOMPtr<mozIStorageStatement> statement;
  rv = DBConn()->CreateStatement(getChildFolderQuery, getter_AddRefs(statement));
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
nsNavBookmarks::SetItemDateAdded(PRInt64 aItemId, PRTime aDateAdded)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_bookmarks SET dateAdded = ?1 WHERE id = ?2"),
                               getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(0, aDateAdded);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(1, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // note, we are not notifying the observers
  // that the item has changed.

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetItemDateAdded(PRInt64 aItemId, PRTime *aDateAdded)
{
  NS_ENSURE_ARG_POINTER(aDateAdded);

  mozStorageStatementScoper scope(mDBGetItemProperties);
  nsresult rv = mDBGetItemProperties->BindInt64Parameter(0, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetItemProperties->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results)
    return NS_ERROR_INVALID_ARG; // invalid item id

  return mDBGetItemProperties->GetInt64(kGetItemPropertiesIndex_DateAdded, aDateAdded);
}

NS_IMETHODIMP
nsNavBookmarks::SetItemLastModified(PRInt64 aItemId, PRTime aLastModified)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_bookmarks SET lastModified = ?1 WHERE id = ?2"),
                               getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(0, aLastModified);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(1, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // note, we are not notifying the observers
  // that the item has changed.

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetItemLastModified(PRInt64 aItemId, PRTime *aLastModified)
{
  NS_ENSURE_ARG_POINTER(aLastModified);

  mozStorageStatementScoper scope(mDBGetItemProperties);
  nsresult rv = mDBGetItemProperties->BindInt64Parameter(0, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetItemProperties->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results)
    return NS_ERROR_INVALID_ARG; // invalid item id

  return mDBGetItemProperties->GetInt64(kGetItemPropertiesIndex_LastModified, aLastModified);
}

NS_IMETHODIMP
nsNavBookmarks::GetItemGUID(PRInt64 aItemId, nsAString &aGUID)
{
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = annosvc->GetItemAnnotationString(aItemId, GUID_ANNO, aGUID);

  if (NS_SUCCEEDED(rv) || rv != NS_ERROR_NOT_AVAILABLE)
    return rv;

  nsAutoString tmp;
  tmp.Assign(mGUIDBase);
  tmp.AppendInt(mItemCount++);
  aGUID.Assign(tmp);

  return SetItemGUID(aItemId, aGUID);
}

NS_IMETHODIMP
nsNavBookmarks::SetItemGUID(PRInt64 aItemId, const nsAString &aGUID)
{
  PRInt64 checkId;
  GetItemIdForGUID(aGUID, &checkId);
  if (checkId != -1)
    return NS_ERROR_INVALID_ARG; // invalid GUID, already exists

  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  return annosvc->SetItemAnnotationString(aItemId, GUID_ANNO, aGUID, 0,
                                          nsIAnnotationService::EXPIRE_NEVER);
}

NS_IMETHODIMP
nsNavBookmarks::GetItemIdForGUID(const nsAString &aGUID, PRInt64 *aItemId)
{
  mozStorageStatementScoper scoper(mDBGetItemIdForGUID);
  nsresult rv = mDBGetItemIdForGUID->BindStringParameter(0, aGUID);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = mDBGetItemIdForGUID->ExecuteStep(&hasMore);
  if (NS_FAILED(rv) || ! hasMore) {
    *aItemId = -1;
    return NS_OK; // not found: return -1
  }

  // found, get the itemId
  return mDBGetItemIdForGUID->GetInt64(0, aItemId);
}

NS_IMETHODIMP
nsNavBookmarks::SetItemTitle(PRInt64 aItemId, const nsAString &aTitle)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_bookmarks SET title = ?1 WHERE id = ?2"),
                               getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(0, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(1, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetItemLastModified(aItemId, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemChanged(aItemId, NS_LITERAL_CSTRING("title"),
                                    PR_FALSE, NS_ConvertUTF16toUTF8(aTitle)));
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetItemTitle(PRInt64 aItemId, nsAString &aTitle)
{
  mozStorageStatementScoper scope(mDBGetItemProperties);

  nsresult rv = mDBGetItemProperties->BindInt64Parameter(0, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetItemProperties->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results)
    return NS_ERROR_INVALID_ARG; // invalid bookmark id

  return mDBGetItemProperties->GetString(kGetItemPropertiesIndex_Title, aTitle);
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkURI(PRInt64 aItemId, nsIURI **aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  mozStorageStatementScoper scope(mDBGetItemProperties);
  nsresult rv = mDBGetItemProperties->BindInt64Parameter(0, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetItemProperties->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results)
    return NS_ERROR_INVALID_ARG; // invalid bookmark id

  PRInt32 type = mDBGetItemProperties->AsInt32(kGetItemPropertiesIndex_Type);
  if (type != TYPE_BOOKMARK)
    return NS_ERROR_INVALID_ARG; // invalid type (only for bookmarks)

  nsCAutoString spec;
  rv = mDBGetItemProperties->GetUTF8String(kGetItemPropertiesIndex_URI, spec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewURI(aURI, spec);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetItemType(PRInt64 aItemId, PRUint16 *aType)
{
  mozStorageStatementScoper scope(mDBGetItemProperties);

  nsresult rv = mDBGetItemProperties->BindInt64Parameter(0, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetItemProperties->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    return NS_ERROR_INVALID_ARG; // invalid bookmark id
  }

  *aType = (PRUint16)mDBGetItemProperties->AsInt32(kGetItemPropertiesIndex_Type);
  return NS_OK;
}

nsresult
nsNavBookmarks::GetFolderType(PRInt64 aFolder, nsACString &aType)
{
  mozStorageStatementScoper scope(mDBGetItemProperties);
  nsresult rv = mDBGetItemProperties->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetItemProperties->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    return NS_ERROR_INVALID_ARG;
  }

  return mDBGetItemProperties->GetUTF8String(kGetItemPropertiesIndex_ServiceContractId, aType);
}

nsresult
nsNavBookmarks::ResultNodeForContainer(PRInt64 aID,
                                       nsNavHistoryQueryOptions *aOptions,
                                       nsNavHistoryResultNode **aNode)
{
  mozStorageStatementScoper scope(mDBGetItemProperties);
  mDBGetItemProperties->BindInt64Parameter(0, aID);

  PRBool results;
  nsresult rv = mDBGetItemProperties->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(results, "ResultNodeForContainer expects a valid item id");

  // contract id
  nsCAutoString contractId;
  rv = mDBGetItemProperties->GetUTF8String(kGetItemPropertiesIndex_ServiceContractId,
                                           contractId);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  nsCAutoString title;
  rv = mDBGetItemProperties->GetUTF8String(kGetItemPropertiesIndex_Title, title);

  PRInt32 itemType = mDBGetItemProperties->AsInt32(kGetItemPropertiesIndex_Type);
  if (itemType == TYPE_DYNAMIC_CONTAINER) {
    *aNode = new nsNavHistoryContainerResultNode(EmptyCString(), title, EmptyCString(),
                                                 nsINavHistoryResultNode::RESULT_TYPE_DYNAMIC_CONTAINER,
                                                 PR_TRUE,
                                                 contractId,
                                                 aOptions);
    (*aNode)->mItemId = aID;
  } else { // TYPE_FOLDER
    *aNode = new nsNavHistoryFolderResultNode(title, aOptions, aID, contractId);
  }
  if (!*aNode)
    return NS_ERROR_OUT_OF_MEMORY;

  (*aNode)->mDateAdded =
    mDBGetItemProperties->AsInt64(kGetItemPropertiesIndex_DateAdded);
  (*aNode)->mLastModified =
    mDBGetItemProperties->AsInt64(kGetItemPropertiesIndex_LastModified);

  NS_ADDREF(*aNode);
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
  nsCAutoString spec("place:folder=");
  spec.AppendInt(aFolder);
  spec.AppendLiteral("&group=3"); // GROUP_BY_FOLDER
  return NS_NewURI(aURI, spec);
}

nsresult
nsNavBookmarks::QueryFolderChildren(PRInt64 aFolderId,
                                    nsNavHistoryQueryOptions *aOptions,
                                    nsCOMArray<nsNavHistoryResultNode> *aChildren)
{
  mozStorageStatementScoper scope(mDBGetChildren);

  nsresult rv = mDBGetChildren->BindInt64Parameter(0, aFolderId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBGetChildren->BindInt32Parameter(1, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBGetChildren->BindInt32Parameter(2, PR_INT32_MAX);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;

  nsCOMPtr<nsNavHistoryQueryOptions> options = do_QueryInterface(aOptions, &rv);
  PRInt32 index = -1;
  while (NS_SUCCEEDED(mDBGetChildren->ExecuteStep(&results)) && results) {

    // The results will be in order of index. Even if we don't add a node
    // because it was excluded, we need to count it's index, so do that
    // before doing anything else. Index was initialized to -1 above, so
    // it will start counting at 0 the first time through the loop.
    index ++;

    PRInt32 itemType = mDBGetChildren->AsInt32(kGetChildrenIndex_Type);
    PRInt64 id = mDBGetChildren->AsInt64(nsNavHistory::kGetInfoIndex_ItemId);
    nsCOMPtr<nsNavHistoryResultNode> node;
    if (itemType == TYPE_FOLDER || itemType == TYPE_DYNAMIC_CONTAINER) {
      if (itemType == TYPE_DYNAMIC_CONTAINER ||
          (itemType == TYPE_FOLDER && options->ExcludeReadOnlyFolders())) {
        // see if it's read only and skip it
        PRBool readOnly = PR_FALSE;
        GetFolderReadonly(id, &readOnly);
        if (readOnly)
          continue; // skip
      }

      rv = ResultNodeForContainer(id, aOptions, getter_AddRefs(node));
      if (NS_FAILED(rv))
        continue;
    } else if (mDBGetChildren->AsInt32(kGetChildrenIndex_Type) == TYPE_SEPARATOR) {
      // separator
      if (aOptions->ExcludeItems()) {
        continue;
      }
      node = new nsNavHistorySeparatorResultNode();
      NS_ENSURE_TRUE(node, NS_ERROR_OUT_OF_MEMORY);

      // add the item identifier (RowToResult does so for bookmark items in
      // the next else block);
      node->mItemId =
        mDBGetChildren->AsInt64(nsNavHistory::kGetInfoIndex_ItemId);

      // date-added and last-modified
      node->mDateAdded =
        mDBGetChildren->AsInt64(nsNavHistory::kGetInfoIndex_ItemDateAdded);
      node->mLastModified =
        mDBGetChildren->AsInt64(nsNavHistory::kGetInfoIndex_ItemLastModified);
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

    // this method fills all bookmark queries, so we store the index of the
    // item in its parent
    node->mBookmarkIndex = index;

    NS_ENSURE_TRUE(aChildren->AppendObject(node), NS_ERROR_OUT_OF_MEMORY);
  }
  return NS_OK;
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
  nsNavHistory* history = History();
  NS_ENSURE_TRUE(history, NS_ERROR_UNEXPECTED);

  // convert the URL to an ID
  PRInt64 urlID;
  nsresult rv = history->GetUrlIdFor(aURI, &urlID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! urlID) {
    // never seen this before, not even in history
    *aBookmarked = PR_FALSE;
    return NS_OK;
  }

  PRInt64 bookmarkedID;
  PRBool foundOne = mBookmarksHash.Get(urlID, &bookmarkedID);

  // IsBookmarked only tests if this exact URI is bookmarked, so we need to
  // check that the destination matches
  if (foundOne)
    *aBookmarked = (urlID == bookmarkedID);
  else
    *aBookmarked = PR_FALSE;

#ifdef DEBUG
  // sanity check for the bookmark hashtable
  PRBool realBookmarked;
  rv = IsBookmarkedInDatabase(urlID, &realBookmarked);
  NS_ASSERTION(realBookmarked == *aBookmarked,
               "Bookmark hash table out-of-sync with the database");
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkedURIFor(nsIURI* aURI, nsIURI** _retval)
{
  *_retval = nsnull;

  nsNavHistory* history = History();
  NS_ENSURE_TRUE(history, NS_ERROR_UNEXPECTED);

  // convert the URL to an ID
  PRInt64 urlID;
  nsresult rv = history->GetUrlIdFor(aURI, &urlID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! urlID) {
    // never seen this before, not even in history, leave result NULL
    return NS_OK;
  }

  PRInt64 bookmarkID;
  if (mBookmarksHash.Get(urlID, &bookmarkID)) {
    // found one, convert ID back to URL. This statement is NOT refcounted
    mozIStorageStatement* statement = history->DBGetIdPageInfo();
    NS_ENSURE_TRUE(statement, NS_ERROR_UNEXPECTED);
    mozStorageStatementScoper scoper(statement);

    rv = statement->BindInt64Parameter(0, bookmarkID);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    if (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
      nsCAutoString spec;
      statement->GetUTF8String(nsNavHistory::kGetInfoIndex_URL, spec);
      return NS_NewURI(_retval, spec);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::ChangeBookmarkURI(PRInt64 aBookmarkId, nsIURI *aNewURI)
{
  NS_ENSURE_ARG(aNewURI);

  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 placeId;
  nsresult rv = History()->GetUrlIdFor(aNewURI, &placeId, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!placeId)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<mozIStorageStatement> statement;
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_bookmarks SET fk = ?1 WHERE id = ?2"),
                               getter_AddRefs(statement));
  statement->BindInt64Parameter(0, placeId);
  statement->BindInt64Parameter(1, aBookmarkId);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetItemLastModified(aBookmarkId, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  rv = aNewURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // Pass the new URI to OnItemChanged.
  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
    OnItemChanged(aBookmarkId, NS_LITERAL_CSTRING("uri"), PR_FALSE, spec))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetFolderIdForItem(PRInt64 aItemId, PRInt64 *aFolderId)
{
  NS_ENSURE_ARG_POINTER(aFolderId);

  mozStorageStatementScoper scope(mDBGetItemProperties);
  nsresult rv = mDBGetItemProperties->BindInt64Parameter(0, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetItemProperties->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results)
    return NS_ERROR_INVALID_ARG; // invalid item id

  return mDBGetItemProperties->GetInt64(kGetItemPropertiesIndex_Parent, aFolderId);
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkIdsForURITArray(nsIURI *aURI,
                                         nsTArray<PRInt64> *aResult) 
{
  mozStorageStatementScoper scope(mDBFindURIBookmarks);
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  nsresult rv = BindStatementURI(mDBFindURIBookmarks, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  mDBFindURIBookmarks->BindInt32Parameter(1, TYPE_BOOKMARK);

  PRBool more;
  while (NS_SUCCEEDED((rv = mDBFindURIBookmarks->ExecuteStep(&more))) && more) {
    if (! aResult->AppendElement(
        mDBFindURIBookmarks->AsInt64(kFindBookmarksIndex_ID)))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  return transaction.Commit();
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkIdsForURI(nsIURI *aURI, PRUint32 *aCount,
                                   PRInt64 **aBookmarks)
{
  *aCount = 0;
  *aBookmarks = nsnull;
  nsTArray<PRInt64> bookmarks;

  // Get the information from the DB as a TArray
  nsresult rv = GetBookmarkIdsForURITArray(aURI, &bookmarks);
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the results into a new array for output
  if (bookmarks.Length()) {
    *aBookmarks = static_cast<PRInt64*>
                             (nsMemory::Alloc(sizeof(PRInt64) * bookmarks.Length()));
    if (! *aBookmarks)
      return NS_ERROR_OUT_OF_MEMORY;
    for (PRUint32 i = 0; i < bookmarks.Length(); i ++)
      (*aBookmarks)[i] = bookmarks[i];
  }
  *aCount = bookmarks.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetItemIndex(PRInt64 aItemId, PRInt32 *aIndex)
{
  mozStorageStatementScoper scope(mDBGetItemIndex);
  mDBGetItemIndex->BindInt64Parameter(0, aItemId);
  PRBool results;
  nsresult rv = mDBGetItemIndex->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!results) {
    *aIndex = -1;
    return NS_OK;
  }

  *aIndex = mDBGetItemIndex->AsInt32(0);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::SetItemIndex(PRInt64 aItemId, PRInt32 aNewIndex)
{
  nsresult rv;
  PRInt32 oldIndex = 0;
  PRInt64 parent = 0;

  {
    mozStorageStatementScoper scopeGet(mDBGetItemProperties);
    rv = mDBGetItemProperties->BindInt64Parameter(0, aItemId);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = mDBGetItemProperties->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!results)
      return NS_OK;

    oldIndex = mDBGetItemProperties->AsInt32(kGetItemPropertiesIndex_Position);
    parent = mDBGetItemProperties->AsInt64(kGetItemPropertiesIndex_Parent);
  }

  nsCOMPtr<mozIStorageStatement> statement;
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING("UPDATE moz_bookmarks SET position = ?2 WHERE id = ?1"),
                               getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt64Parameter(0, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt32Parameter(1, aNewIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemRemoved(aItemId, parent, oldIndex))
  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemAdded(aItemId, parent, aNewIndex))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::SetKeywordForBookmark(PRInt64 aBookmarkId, const nsAString& aKeyword)
{
  if (aBookmarkId < 1)
    return NS_ERROR_INVALID_ARG; // invalid bookmark id

  // Shortcuts are always lowercased internally.
  nsAutoString kwd(aKeyword);
  ToLowerCase(kwd);

  mozStorageTransaction transaction(DBConn(), PR_FALSE);
  nsresult rv;
  PRBool results;
  PRInt64 keywordId = 0;

  if (!kwd.IsEmpty()) {
    //  Attempt to find pre-existing keyword record
    nsCOMPtr<mozIStorageStatement> getKeywordStmnt;
    rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
         "SELECT id from moz_keywords WHERE keyword = ?1"),
      getter_AddRefs(getKeywordStmnt));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = getKeywordStmnt->BindStringParameter(0, kwd);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = getKeywordStmnt->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);

    if (results) {
      rv = getKeywordStmnt->GetInt64(0, &keywordId);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      // If not already in the db, create new keyword record
      nsCOMPtr<mozIStorageStatement> addKeywordStmnt;
      rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
          "INSERT INTO moz_keywords (keyword) VALUES (?1)"),
        getter_AddRefs(addKeywordStmnt));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = addKeywordStmnt->BindStringParameter(0, kwd);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = addKeywordStmnt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
      rv = DBConn()->GetLastInsertRowID(&keywordId);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Update bookmark record w/ the keyword's id, or null
  nsCOMPtr<mozIStorageStatement> updateKeywordStmnt;
  rv = DBConn()->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_bookmarks SET keyword_id = ?1 WHERE id = ?2"),
    getter_AddRefs(updateKeywordStmnt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = updateKeywordStmnt->BindInt64Parameter(0, keywordId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = updateKeywordStmnt->BindInt64Parameter(1, aBookmarkId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = updateKeywordStmnt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetItemLastModified(aBookmarkId, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  transaction.Commit();

  // Pass the new keyword to OnItemChanged.
  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemChanged(aBookmarkId, NS_LITERAL_CSTRING("keyword"),
                                    PR_FALSE, NS_ConvertUTF16toUTF8(aKeyword)))

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetKeywordForURI(nsIURI* aURI, nsAString& aKeyword)
{
  aKeyword.Truncate(0);

  mozStorageStatementScoper scoper(mDBGetKeywordForURI);
  nsresult rv = BindStatementURI(mDBGetKeywordForURI, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = mDBGetKeywordForURI->ExecuteStep(&hasMore);
  if (NS_FAILED(rv) || ! hasMore) {
    aKeyword.SetIsVoid(PR_TRUE);
    return NS_OK; // not found: return void keyword string
  }

  // found, get the keyword
  return mDBGetKeywordForURI->GetString(0, aKeyword);
}

NS_IMETHODIMP
nsNavBookmarks::GetKeywordForBookmark(PRInt64 aBookmarkId, nsAString& aKeyword)
{
  aKeyword.Truncate(0);

  mozStorageStatementScoper scoper(mDBGetKeywordForBookmark);
  nsresult rv = mDBGetKeywordForBookmark->BindInt64Parameter(0, aBookmarkId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = mDBGetKeywordForBookmark->ExecuteStep(&hasMore);
  if (NS_FAILED(rv) || ! hasMore) {
    aKeyword.SetIsVoid(PR_TRUE);
    return NS_OK; // not found: return void keyword string
  }

  // found, get the keyword
  return mDBGetKeywordForBookmark->GetString(0, aKeyword);
}

NS_IMETHODIMP
nsNavBookmarks::GetURIForKeyword(const nsAString& aKeyword, nsIURI** aURI)
{
  *aURI = nsnull;
  if (aKeyword.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  // Shortcuts are always lowercased internally.
  nsAutoString kwd(aKeyword);
  ToLowerCase(kwd);

  mozStorageStatementScoper scoper(mDBGetURIForKeyword);
  nsresult rv = mDBGetURIForKeyword->BindStringParameter(0, kwd);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = mDBGetURIForKeyword->ExecuteStep(&hasMore);
  if (NS_FAILED(rv) || ! hasMore)
    return NS_OK; // not found: leave URI null

  // found, get the URI
  nsCAutoString spec;
  rv = mDBGetURIForKeyword->GetUTF8String(0, spec);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_NewURI(aURI, spec);
}

// See RunInBatchMode, mLock _must_ be set when batching
nsresult
nsNavBookmarks::BeginUpdateBatch()
{
  if (mBatchLevel++ == 0) {
    mozIStorageConnection* conn = DBConn();
    PRBool transactionInProgress = PR_TRUE; // default to no transaction on err
    conn->GetTransactionInProgress(&transactionInProgress);
    mBatchHasTransaction = ! transactionInProgress;
    if (mBatchHasTransaction)
      conn->BeginTransaction();

    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnBeginUpdateBatch())
  }
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);
  return NS_OK;
}

nsresult
nsNavBookmarks::EndUpdateBatch()
{
  if (--mBatchLevel == 0) {
    if (mBatchHasTransaction)
      DBConn()->CommitTransaction();
    mBatchHasTransaction = PR_FALSE;
    ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                        OnEndUpdateBatch())
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RunInBatchMode(nsINavHistoryBatchCallback* aCallback,
                               nsISupports* aUserData) {
  NS_ENSURE_STATE(mLock);
  NS_ENSURE_ARG_POINTER(aCallback);

  nsAutoLock lock(mLock);
  BeginUpdateBatch();
  nsresult rv = aCallback->RunBatched(aUserData);
  EndUpdateBatch();
  NS_ENSURE_SUCCESS(rv, rv);

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

/**
 * Called by the History service when shutting down
 */
nsresult
nsNavBookmarks::OnQuit()
{
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
nsNavBookmarks::OnVisit(nsIURI *aURI, PRInt64 aVisitID, PRTime aTime,
                        PRInt64 aSessionID, PRInt64 aReferringID,
                        PRUint32 aTransitionType)
{
  // If the page is bookmarked, we need to notify observers
  PRBool bookmarked = PR_FALSE;
  IsBookmarked(aURI, &bookmarked);
  if (bookmarked) {
    // query for all bookmarks for that URI, notify for each
    nsTArray<PRInt64> bookmarks;

    nsresult rv = GetBookmarkIdsForURITArray(aURI, &bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    if (bookmarks.Length()) {
      for (PRUint32 i = 0; i < bookmarks.Length(); i++)
        ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                            OnItemVisited(bookmarks[i], aVisitID, aTime))
    }
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
    // query for all bookmarks for that URI, notify for each 
    nsTArray<PRInt64> bookmarks;

    nsresult rv = GetBookmarkIdsForURITArray(aURI, &bookmarks);
    NS_ENSURE_SUCCESS(rv, rv);

    if (bookmarks.Length()) {
      for (PRUint32 i = 0; i < bookmarks.Length(); i ++)
        ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                            OnItemChanged(bookmarks[i], NS_LITERAL_CSTRING("cleartime"),
                                          PR_FALSE, EmptyCString()))
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
nsNavBookmarks::OnPageChanged(nsIURI *aURI, PRUint32 aWhat,
                              const nsAString &aValue)
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

      nsNavHistory* history = History();
      NS_ENSURE_TRUE(history, NS_ERROR_UNEXPECTED);
  
      nsCOMArray<nsNavHistoryQuery> queries;
      nsCOMPtr<nsNavHistoryQueryOptions> options;
      rv = history->QueryStringToQueryArray(spec, &queries, getter_AddRefs(options));
      NS_ENSURE_SUCCESS(rv, rv);

      NS_ENSURE_STATE(queries.Count() == 1);
      NS_ENSURE_STATE(queries[0]->Folders().Length() == 1);

      ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                          OnItemChanged(queries[0]->Folders()[0], NS_LITERAL_CSTRING("favicon"),
                                        PR_FALSE, NS_ConvertUTF16toUTF8(aValue)));
    }
    else {
      // query for all bookmarks for that URI, notify for each 
      nsTArray<PRInt64> bookmarks;
      rv = GetBookmarkIdsForURITArray(aURI, &bookmarks);
      NS_ENSURE_SUCCESS(rv, rv);

      if (bookmarks.Length()) {
        for (PRUint32 i = 0; i < bookmarks.Length(); i ++)
          ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                              OnItemChanged(bookmarks[i], NS_LITERAL_CSTRING("favicon"),
                                            PR_FALSE, NS_ConvertUTF16toUTF8(aValue)));
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnPageExpired(nsIURI* aURI, PRTime aVisitTime,
                              PRBool aWholeEntry)
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
  nsresult rv = SetItemLastModified(aItemId, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemChanged(aItemId, aName, PR_TRUE, EmptyCString()));

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
  nsresult rv = SetItemLastModified(aItemId, PR_Now());
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_WEAKARRAY(mObservers, nsINavBookmarkObserver,
                      OnItemChanged(aItemId, aName, PR_TRUE, EmptyCString()));

  return NS_OK;
}

PRBool
nsNavBookmarks::ItemExists(PRInt64 aItemId) {
  mozStorageStatementScoper scope(mDBGetItemProperties);
  nsresult rv = mDBGetItemProperties->BindInt64Parameter(0, aItemId);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  PRBool results;
  rv = mDBGetItemProperties->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  if (!results)
    return PR_FALSE;

  return PR_TRUE;
}
