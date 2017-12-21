/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var bs = PlacesUtils.bookmarks;
var hs = PlacesUtils.history;
var anno = PlacesUtils.annotations;


var bookmarksObserver = {
  onBeginUpdateBatch() {
    this._beginUpdateBatch = true;
  },
  onEndUpdateBatch() {
    this._endUpdateBatch = true;
  },
  onItemAdded(id, folder, index, itemType, uri, title, dateAdded,
                        guid) {
    this._itemAddedId = id;
    this._itemAddedParent = folder;
    this._itemAddedIndex = index;
    this._itemAddedURI = uri;
    this._itemAddedTitle = title;

    // Ensure that we've created a guid for this item.
    let stmt = DBConn().createStatement(
      `SELECT guid
       FROM moz_bookmarks
       WHERE id = :item_id`
    );
    stmt.params.item_id = id;
    Assert.ok(stmt.executeStep());
    Assert.ok(!stmt.getIsNull(0));
    do_check_valid_places_guid(stmt.row.guid);
    Assert.equal(stmt.row.guid, guid);
    stmt.finalize();
  },
  onItemRemoved(id, folder, index, itemType) {
    this._itemRemovedId = id;
    this._itemRemovedFolder = folder;
    this._itemRemovedIndex = index;
  },
  onItemChanged(id, property, isAnnotationProperty, value,
                          lastModified, itemType, parentId, guid, parentGuid,
                          oldValue) {
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChanged_isAnnotationProperty = isAnnotationProperty;
    this._itemChangedValue = value;
    this._itemChangedOldValue = oldValue;
  },
  onItemVisited(id, visitID, time) {
    this._itemVisitedId = id;
    this._itemVisitedVistId = visitID;
    this._itemVisitedTime = time;
  },
  onItemMoved(id, oldParent, oldIndex, newParent, newIndex,
                        itemType) {
    this._itemMovedId = id;
    this._itemMovedOldParent = oldParent;
    this._itemMovedOldIndex = oldIndex;
    this._itemMovedNewParent = newParent;
    this._itemMovedNewIndex = newIndex;
  },
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
  ])
};


// Get bookmarks menu folder id.
var root = bs.bookmarksMenuFolder;
// Index at which items should begin.
var bmStartIndex = 0;

add_task(async function test_bookmarks() {
  bs.addObserver(bookmarksObserver);

  // test special folders
  Assert.ok(bs.placesRoot > 0);
  Assert.ok(bs.bookmarksMenuFolder > 0);
  Assert.ok(bs.tagsFolder > 0);
  Assert.ok(bs.toolbarFolder > 0);
  Assert.ok(bs.unfiledBookmarksFolder > 0);

  // test getFolderIdForItem() with bogus item id will throw
  try {
    bs.getFolderIdForItem(0);
    do_throw("getFolderIdForItem accepted bad input");
  } catch (ex) {}

  // test getFolderIdForItem() with bogus item id will throw
  try {
    bs.getFolderIdForItem(-1);
    do_throw("getFolderIdForItem accepted bad input");
  } catch (ex) {}

  // test root parentage
  Assert.equal(bs.getFolderIdForItem(bs.bookmarksMenuFolder), bs.placesRoot);
  Assert.equal(bs.getFolderIdForItem(bs.tagsFolder), bs.placesRoot);
  Assert.equal(bs.getFolderIdForItem(bs.toolbarFolder), bs.placesRoot);
  Assert.equal(bs.getFolderIdForItem(bs.unfiledBookmarksFolder), bs.placesRoot);

  // create a folder to hold all the tests
  // this makes the tests more tolerant of changes to default_places.html
  let testRoot = bs.createFolder(root, "places bookmarks xpcshell tests",
                                 bs.DEFAULT_INDEX);
  Assert.equal(bookmarksObserver._itemAddedId, testRoot);
  Assert.equal(bookmarksObserver._itemAddedParent, root);
  Assert.equal(bookmarksObserver._itemAddedIndex, bmStartIndex);
  Assert.equal(bookmarksObserver._itemAddedURI, null);
  let testStartIndex = 0;

  // test getItemIndex for folders
  Assert.equal(bs.getItemIndex(testRoot), bmStartIndex);

  // test getItemType for folders
  Assert.equal(bs.getItemType(testRoot), bs.TYPE_FOLDER);

  // insert a bookmark.
  // the time before we insert, in microseconds
  let beforeInsert = Date.now() * 1000;
  Assert.ok(beforeInsert > 0);

  let newId = bs.insertBookmark(testRoot, uri("http://google.com/"),
                                bs.DEFAULT_INDEX, "");
  Assert.equal(bookmarksObserver._itemAddedId, newId);
  Assert.equal(bookmarksObserver._itemAddedParent, testRoot);
  Assert.equal(bookmarksObserver._itemAddedIndex, testStartIndex);
  Assert.ok(bookmarksObserver._itemAddedURI.equals(uri("http://google.com/")));
  Assert.equal(bs.getBookmarkURI(newId).spec, "http://google.com/");

  let dateAdded = bs.getItemDateAdded(newId);
  // dateAdded can equal beforeInsert
  Assert.ok(is_time_ordered(beforeInsert, dateAdded));

  // after just inserting, modified should not be set
  let lastModified = bs.getItemLastModified(newId);
  Assert.equal(lastModified, dateAdded);

  // The time before we set the title, in microseconds.
  let beforeSetTitle = Date.now() * 1000;
  Assert.ok(beforeSetTitle >= beforeInsert);

  // Workaround possible VM timers issues moving lastModified and dateAdded
  // to the past.
  lastModified -= 1000;
  bs.setItemLastModified(newId, lastModified);
  dateAdded -= 1000;
  bs.setItemDateAdded(newId, dateAdded);

  // set bookmark title
  bs.setItemTitle(newId, "Google");
  Assert.equal(bookmarksObserver._itemChangedId, newId);
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");
  Assert.equal(bookmarksObserver._itemChangedValue, "Google");

  // check that dateAdded hasn't changed
  let dateAdded2 = bs.getItemDateAdded(newId);
  Assert.equal(dateAdded2, dateAdded);

  // check lastModified after we set the title
  let lastModified2 = bs.getItemLastModified(newId);
  do_print("test setItemTitle");
  do_print("dateAdded = " + dateAdded);
  do_print("beforeSetTitle = " + beforeSetTitle);
  do_print("lastModified = " + lastModified);
  do_print("lastModified2 = " + lastModified2);
  Assert.ok(is_time_ordered(lastModified, lastModified2));
  Assert.ok(is_time_ordered(dateAdded, lastModified2));

  // get item title
  let title = bs.getItemTitle(newId);
  Assert.equal(title, "Google");

  // test getItemType for bookmarks
  Assert.equal(bs.getItemType(newId), bs.TYPE_BOOKMARK);

  // get item title bad input
  try {
    bs.getItemTitle(-3);
    do_throw("getItemTitle accepted bad input");
  } catch (ex) {}

  // get the folder that the bookmark is in
  let folderId = bs.getFolderIdForItem(newId);
  Assert.equal(folderId, testRoot);

  // test getItemIndex for bookmarks
  Assert.equal(bs.getItemIndex(newId), testStartIndex);

  // create a folder at a specific index
  let workFolder = bs.createFolder(testRoot, "Work", 0);
  Assert.equal(bookmarksObserver._itemAddedId, workFolder);
  Assert.equal(bookmarksObserver._itemAddedParent, testRoot);
  Assert.equal(bookmarksObserver._itemAddedIndex, 0);
  Assert.equal(bookmarksObserver._itemAddedURI, null);

  Assert.equal(bs.getItemTitle(workFolder), "Work");
  bs.setItemTitle(workFolder, "Work #");
  Assert.equal(bs.getItemTitle(workFolder), "Work #");

  // add item into subfolder, specifying index
  let newId2 = bs.insertBookmark(workFolder,
                                 uri("http://developer.mozilla.org/"),
                                 0, "");
  Assert.equal(bookmarksObserver._itemAddedId, newId2);
  Assert.equal(bookmarksObserver._itemAddedParent, workFolder);
  Assert.equal(bookmarksObserver._itemAddedIndex, 0);

  // change item
  bs.setItemTitle(newId2, "DevMo");
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");

  // insert item into subfolder
  let newId3 = bs.insertBookmark(workFolder,
                                 uri("http://msdn.microsoft.com/"),
                                 bs.DEFAULT_INDEX, "");
  Assert.equal(bookmarksObserver._itemAddedId, newId3);
  Assert.equal(bookmarksObserver._itemAddedParent, workFolder);
  Assert.equal(bookmarksObserver._itemAddedIndex, 1);

  // change item
  bs.setItemTitle(newId3, "MSDN");
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");

  // remove item
  bs.removeItem(newId2);
  Assert.equal(bookmarksObserver._itemRemovedId, newId2);
  Assert.equal(bookmarksObserver._itemRemovedFolder, workFolder);
  Assert.equal(bookmarksObserver._itemRemovedIndex, 0);

  // insert item into subfolder
  let newId4 = bs.insertBookmark(workFolder,
                                 uri("http://developer.mozilla.org/"),
                                 bs.DEFAULT_INDEX, "");
  Assert.equal(bookmarksObserver._itemAddedId, newId4);
  Assert.equal(bookmarksObserver._itemAddedParent, workFolder);
  Assert.equal(bookmarksObserver._itemAddedIndex, 1);

  // create folder
  let homeFolder = bs.createFolder(testRoot, "Home", bs.DEFAULT_INDEX);
  Assert.equal(bookmarksObserver._itemAddedId, homeFolder);
  Assert.equal(bookmarksObserver._itemAddedParent, testRoot);
  Assert.equal(bookmarksObserver._itemAddedIndex, 2);

  // insert item
  let newId5 = bs.insertBookmark(homeFolder, uri("http://espn.com/"),
                                 bs.DEFAULT_INDEX, "");
  Assert.equal(bookmarksObserver._itemAddedId, newId5);
  Assert.equal(bookmarksObserver._itemAddedParent, homeFolder);
  Assert.equal(bookmarksObserver._itemAddedIndex, 0);

  // change item
  bs.setItemTitle(newId5, "ESPN");
  Assert.equal(bookmarksObserver._itemChangedId, newId5);
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");

  // insert query item
  let uri6 = uri("place:domain=google.com&type=" +
                 Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY);
  let newId6 = bs.insertBookmark(testRoot, uri6, bs.DEFAULT_INDEX, "");
  Assert.equal(bookmarksObserver._itemAddedParent, testRoot);
  Assert.equal(bookmarksObserver._itemAddedIndex, 3);

  // change item
  bs.setItemTitle(newId6, "Google Sites");
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");

  // test getIdForItemAt
  Assert.equal(bs.getIdForItemAt(testRoot, 0), workFolder);
  // wrong parent, should return -1
  Assert.equal(bs.getIdForItemAt(1337, 0), -1);
  // wrong index, should return -1
  Assert.equal(bs.getIdForItemAt(testRoot, 1337), -1);
  // wrong parent and index, should return -1
  Assert.equal(bs.getIdForItemAt(1337, 1337), -1);

  // move folder, appending, to different folder
  let oldParentCC = getChildCount(testRoot);
  bs.moveItem(workFolder, homeFolder, bs.DEFAULT_INDEX);
  Assert.equal(bookmarksObserver._itemMovedId, workFolder);
  Assert.equal(bookmarksObserver._itemMovedOldParent, testRoot);
  Assert.equal(bookmarksObserver._itemMovedOldIndex, 0);
  Assert.equal(bookmarksObserver._itemMovedNewParent, homeFolder);
  Assert.equal(bookmarksObserver._itemMovedNewIndex, 1);

  // test that the new index is properly stored
  Assert.equal(bs.getItemIndex(workFolder), 1);
  Assert.equal(bs.getFolderIdForItem(workFolder), homeFolder);

  // try to get index of the item from within the old parent folder
  // check that it has been really removed from there
  Assert.notEqual(bs.getIdForItemAt(testRoot, 0), workFolder);
  // check the last item from within the old parent folder
  Assert.notEqual(bs.getIdForItemAt(testRoot, -1), workFolder);
  // check the index of the item within the new parent folder
  Assert.equal(bs.getIdForItemAt(homeFolder, 1), workFolder);
  // try to get index of the last item within the new parent folder
  Assert.equal(bs.getIdForItemAt(homeFolder, -1), workFolder);
  // XXX expose FolderCount, and check that the old parent has one less child?
  Assert.equal(getChildCount(testRoot), oldParentCC - 1);

  // move item, appending, to different folder
  bs.moveItem(newId5, testRoot, bs.DEFAULT_INDEX);
  Assert.equal(bookmarksObserver._itemMovedId, newId5);
  Assert.equal(bookmarksObserver._itemMovedOldParent, homeFolder);
  Assert.equal(bookmarksObserver._itemMovedOldIndex, 0);
  Assert.equal(bookmarksObserver._itemMovedNewParent, testRoot);
  Assert.equal(bookmarksObserver._itemMovedNewIndex, 3);

  // test get folder's index
  let tmpFolder = bs.createFolder(testRoot, "tmp", 2);
  Assert.equal(bs.getItemIndex(tmpFolder), 2);

  // test setKeywordForBookmark
  let kwTestItemId = bs.insertBookmark(testRoot, uri("http://keywordtest.com"),
                                       bs.DEFAULT_INDEX, "");
  bs.setKeywordForBookmark(kwTestItemId, "bar");

  // test getKeywordForBookmark
  let k = bs.getKeywordForBookmark(kwTestItemId);
  Assert.equal("bar", k);

  // test PlacesUtils.keywords.fetch()
  let u = await PlacesUtils.keywords.fetch("bar");
  Assert.equal("http://keywordtest.com/", u.url);
  // test removeFolderChildren
  // 1) add/remove each child type (bookmark, separator, folder)
  tmpFolder = bs.createFolder(testRoot, "removeFolderChildren",
                              bs.DEFAULT_INDEX);
  bs.insertBookmark(tmpFolder, uri("http://foo9.com/"), bs.DEFAULT_INDEX, "");
  bs.createFolder(tmpFolder, "subfolder", bs.DEFAULT_INDEX);
  bs.insertSeparator(tmpFolder, bs.DEFAULT_INDEX);
  // 2) confirm that folder has 3 children
  let options = hs.getNewQueryOptions();
  let query = hs.getNewQuery();
  query.setFolders([tmpFolder], 1);
  try {
    let result = hs.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    Assert.equal(rootNode.childCount, 3);
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("test removeFolderChildren() - querying for children failed: " + ex);
  }
  // 3) remove all children
  bs.removeFolderChildren(tmpFolder);
  // 4) confirm that folder has 0 children
  try {
    let result = hs.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    Assert.equal(rootNode.childCount, 0);
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("removeFolderChildren(): " + ex);
  }

  // XXX - test folderReadOnly

  // test bookmark id in query output
  try {
    options = hs.getNewQueryOptions();
    query = hs.getNewQuery();
    query.setFolders([testRoot], 1);
    let result = hs.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let cc = rootNode.childCount;
    do_print("bookmark itemId test: CC = " + cc);
    Assert.ok(cc > 0);
    for (let i = 0; i < cc; ++i) {
      let node = rootNode.getChild(i);
      if (node.type == node.RESULT_TYPE_FOLDER ||
          node.type == node.RESULT_TYPE_URI ||
          node.type == node.RESULT_TYPE_SEPARATOR ||
          node.type == node.RESULT_TYPE_QUERY) {
        Assert.ok(node.itemId > 0);
      } else {
        Assert.equal(node.itemId, -1);
      }
    }
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("bookmarks query: " + ex);
  }

  // test that multiple bookmarks with same URI show up right in bookmark
  // folder queries, todo: also to do for complex folder queries
  try {
    // test uri
    let mURI = uri("http://multiple.uris.in.query");

    let testFolder = bs.createFolder(testRoot, "test Folder", bs.DEFAULT_INDEX);
    // add 2 bookmarks
    bs.insertBookmark(testFolder, mURI, bs.DEFAULT_INDEX, "title 1");
    bs.insertBookmark(testFolder, mURI, bs.DEFAULT_INDEX, "title 2");

    // query
    options = hs.getNewQueryOptions();
    query = hs.getNewQuery();
    query.setFolders([testFolder], 1);
    let result = hs.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let cc = rootNode.childCount;
    Assert.equal(cc, 2);
    Assert.equal(rootNode.getChild(0).title, "title 1");
    Assert.equal(rootNode.getChild(1).title, "title 2");
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("bookmarks query: " + ex);
  }

  // test change bookmark uri
  let newId10 = bs.insertBookmark(testRoot, uri("http://foo10.com/"),
                                  bs.DEFAULT_INDEX, "");
  dateAdded = bs.getItemDateAdded(newId10);
  // after just inserting, modified should not be set
  lastModified = bs.getItemLastModified(newId10);
  Assert.equal(lastModified, dateAdded);

  // Workaround possible VM timers issues moving lastModified and dateAdded
  // to the past.
  lastModified -= 1000;
  bs.setItemLastModified(newId10, lastModified);
  dateAdded -= 1000;
  bs.setItemDateAdded(newId10, dateAdded);

  bs.changeBookmarkURI(newId10, uri("http://foo11.com/"));

  // check that lastModified is set after we change the bookmark uri
  lastModified2 = bs.getItemLastModified(newId10);
  do_print("test changeBookmarkURI");
  do_print("dateAdded = " + dateAdded);
  do_print("lastModified = " + lastModified);
  do_print("lastModified2 = " + lastModified2);
  Assert.ok(is_time_ordered(lastModified, lastModified2));
  Assert.ok(is_time_ordered(dateAdded, lastModified2));

  Assert.equal(bookmarksObserver._itemChangedId, newId10);
  Assert.equal(bookmarksObserver._itemChangedProperty, "uri");
  Assert.equal(bookmarksObserver._itemChangedValue, "http://foo11.com/");
  Assert.equal(bookmarksObserver._itemChangedOldValue, "http://foo10.com/");

  // test getBookmarkURI
  let newId11 = bs.insertBookmark(testRoot, uri("http://foo11.com/"),
                                  bs.DEFAULT_INDEX, "");
  let bmURI = bs.getBookmarkURI(newId11);
  Assert.equal("http://foo11.com/", bmURI.spec);

  // test getBookmarkURI with non-bookmark items
  try {
    bs.getBookmarkURI(testRoot);
    do_throw("getBookmarkURI() should throw for non-bookmark items!");
  } catch (ex) {}

  // test getItemIndex
  let newId12 = bs.insertBookmark(testRoot, uri("http://foo11.com/"), 1, "");
  let bmIndex = bs.getItemIndex(newId12);
  Assert.equal(1, bmIndex);

  // insert a bookmark with title ZZZXXXYYY and then search for it.
  // this test confirms that we can find bookmarks that we haven't visited
  // (which are "hidden") and that we can find by title.
  // see bug #369887 for more details
  let newId13 = bs.insertBookmark(testRoot, uri("http://foobarcheese.com/"),
                                  bs.DEFAULT_INDEX, "");
  Assert.equal(bookmarksObserver._itemAddedId, newId13);
  Assert.equal(bookmarksObserver._itemAddedParent, testRoot);
  Assert.equal(bookmarksObserver._itemAddedIndex, 11);

  // set bookmark title
  bs.setItemTitle(newId13, "ZZZXXXYYY");
  Assert.equal(bookmarksObserver._itemChangedId, newId13);
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");
  Assert.equal(bookmarksObserver._itemChangedValue, "ZZZXXXYYY");

  // check if setting an item annotation triggers onItemChanged
  bookmarksObserver._itemChangedId = -1;
  anno.setItemAnnotation(newId3, "test-annotation", "foo", 0, 0);
  Assert.equal(bookmarksObserver._itemChangedId, newId3);
  Assert.equal(bookmarksObserver._itemChangedProperty, "test-annotation");
  Assert.ok(bookmarksObserver._itemChanged_isAnnotationProperty);
  Assert.equal(bookmarksObserver._itemChangedValue, "");

  // test search on bookmark title ZZZXXXYYY
  try {
    options = hs.getNewQueryOptions();
    options.excludeQueries = 1;
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    query = hs.getNewQuery();
    query.searchTerms = "ZZZXXXYYY";
    let result = hs.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let cc = rootNode.childCount;
    Assert.equal(cc, 1);
    let node = rootNode.getChild(0);
    Assert.equal(node.title, "ZZZXXXYYY");
    Assert.ok(node.itemId > 0);
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("bookmarks query: " + ex);
  }

  // test dateAdded and lastModified properties
  // for a search query
  try {
    options = hs.getNewQueryOptions();
    options.excludeQueries = 1;
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    query = hs.getNewQuery();
    query.searchTerms = "ZZZXXXYYY";
    let result = hs.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let cc = rootNode.childCount;
    Assert.equal(cc, 1);
    let node = rootNode.getChild(0);

    Assert.equal(typeof node.dateAdded, "number");
    Assert.ok(node.dateAdded > 0);

    Assert.equal(typeof node.lastModified, "number");
    Assert.ok(node.lastModified > 0);

    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("bookmarks query: " + ex);
  }

  // test dateAdded and lastModified properties
  // for a folder query
  try {
    options = hs.getNewQueryOptions();
    query = hs.getNewQuery();
    query.setFolders([testRoot], 1);
    let result = hs.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let cc = rootNode.childCount;
    Assert.ok(cc > 0);
    for (let i = 0; i < cc; i++) {
      let node = rootNode.getChild(i);

      if (node.type == node.RESULT_TYPE_URI) {
        Assert.equal(typeof node.dateAdded, "number");
        Assert.ok(node.dateAdded > 0);

        Assert.equal(typeof node.lastModified, "number");
        Assert.ok(node.lastModified > 0);
        break;
      }
    }
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("bookmarks query: " + ex);
  }

  // check setItemLastModified() and setItemDateAdded()
  let newId14 = bs.insertBookmark(testRoot, uri("http://bar.tld/"),
                                  bs.DEFAULT_INDEX, "");
  dateAdded = bs.getItemDateAdded(newId14);
  lastModified = bs.getItemLastModified(newId14);
  Assert.equal(lastModified, dateAdded);
  bs.setItemLastModified(newId14, 1234000000000000);
  let fakeLastModified = bs.getItemLastModified(newId14);
  Assert.equal(fakeLastModified, 1234000000000000);
  bs.setItemDateAdded(newId14, 4321000000000000);
  let fakeDateAdded = bs.getItemDateAdded(newId14);
  Assert.equal(fakeDateAdded, 4321000000000000);

  // ensure that removing an item removes its annotations
  Assert.ok(anno.itemHasAnnotation(newId3, "test-annotation"));
  bs.removeItem(newId3);
  Assert.ok(!anno.itemHasAnnotation(newId3, "test-annotation"));

  // bug 378820
  let uri1 = uri("http://foo.tld/a");
  bs.insertBookmark(testRoot, uri1, bs.DEFAULT_INDEX, "");
  await PlacesTestUtils.addVisits(uri1);

  // bug 646993 - test bookmark titles longer than the maximum allowed length
  let title15 = Array(TITLE_LENGTH_MAX + 5).join("X");
  let title15expected = title15.substring(0, TITLE_LENGTH_MAX);
  let newId15 = bs.insertBookmark(testRoot, uri("http://evil.com/"),
                                  bs.DEFAULT_INDEX, title15);

  Assert.equal(bs.getItemTitle(newId15).length,
               title15expected.length);
  Assert.equal(bookmarksObserver._itemAddedTitle, title15expected);
  // test title length after updates
  bs.setItemTitle(newId15, title15 + " updated");
  Assert.equal(bs.getItemTitle(newId15).length,
               title15expected.length);
  Assert.equal(bookmarksObserver._itemChangedId, newId15);
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");
  Assert.equal(bookmarksObserver._itemChangedValue, title15expected);

  testSimpleFolderResult();
});

function testSimpleFolderResult() {
  // the time before we create a folder, in microseconds
  // Workaround possible VM timers issues subtracting 1us.
  let beforeCreate = Date.now() * 1000 - 1;
  Assert.ok(beforeCreate > 0);

  // create a folder
  let parent = bs.createFolder(root, "test", bs.DEFAULT_INDEX);

  let dateCreated = bs.getItemDateAdded(parent);
  do_print("check that the folder was created with a valid dateAdded");
  do_print("beforeCreate = " + beforeCreate);
  do_print("dateCreated = " + dateCreated);
  Assert.ok(is_time_ordered(beforeCreate, dateCreated));

  // the time before we insert, in microseconds
  // Workaround possible VM timers issues subtracting 1ms.
  let beforeInsert = Date.now() * 1000 - 1;
  Assert.ok(beforeInsert > 0);

  // insert a separator
  let sep = bs.insertSeparator(parent, bs.DEFAULT_INDEX);

  let dateAdded = bs.getItemDateAdded(sep);
  do_print("check that the separator was created with a valid dateAdded");
  do_print("beforeInsert = " + beforeInsert);
  do_print("dateAdded = " + dateAdded);
  Assert.ok(is_time_ordered(beforeInsert, dateAdded));

  // re-set item title separately so can test nodes' last modified
  let item = bs.insertBookmark(parent, uri("about:blank"),
                               bs.DEFAULT_INDEX, "");
  bs.setItemTitle(item, "test bookmark");

  // see above
  let folder = bs.createFolder(parent, "test folder", bs.DEFAULT_INDEX);
  bs.setItemTitle(folder, "test folder");

  let longName = Array(TITLE_LENGTH_MAX + 5).join("A");
  let folderLongName = bs.createFolder(parent, longName, bs.DEFAULT_INDEX);
  Assert.equal(bookmarksObserver._itemAddedTitle, longName.substring(0, TITLE_LENGTH_MAX));

  let options = hs.getNewQueryOptions();
  let query = hs.getNewQuery();
  query.setFolders([parent], 1);
  let result = hs.executeQuery(query, options);
  let rootNode = result.root;
  rootNode.containerOpen = true;
  Assert.equal(rootNode.childCount, 4);

  let node = rootNode.getChild(0);
  Assert.ok(node.dateAdded > 0);
  Assert.equal(node.lastModified, node.dateAdded);
  Assert.equal(node.itemId, sep);
  Assert.equal(node.title, "");
  node = rootNode.getChild(1);
  Assert.equal(node.itemId, item);
  Assert.ok(node.dateAdded > 0);
  Assert.ok(node.lastModified > 0);
  Assert.equal(node.title, "test bookmark");
  node = rootNode.getChild(2);
  Assert.equal(node.itemId, folder);
  Assert.equal(node.title, "test folder");
  Assert.ok(node.dateAdded > 0);
  Assert.ok(node.lastModified > 0);
  node = rootNode.getChild(3);
  Assert.equal(node.itemId, folderLongName);
  Assert.equal(node.title, longName.substring(0, TITLE_LENGTH_MAX));
  Assert.ok(node.dateAdded > 0);
  Assert.ok(node.lastModified > 0);

  // update with another long title
  bs.setItemTitle(folderLongName, longName + " updated");
  Assert.equal(bookmarksObserver._itemChangedId, folderLongName);
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");
  Assert.equal(bookmarksObserver._itemChangedValue, longName.substring(0, TITLE_LENGTH_MAX));

  node = rootNode.getChild(3);
  Assert.equal(node.title, longName.substring(0, TITLE_LENGTH_MAX));

  rootNode.containerOpen = false;
}

function getChildCount(aFolderId) {
  let cc = -1;
  try {
    let options = hs.getNewQueryOptions();
    let query = hs.getNewQuery();
    query.setFolders([aFolderId], 1);
    let result = hs.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    cc = rootNode.childCount;
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("getChildCount failed: " + ex);
  }
  return cc;
}
