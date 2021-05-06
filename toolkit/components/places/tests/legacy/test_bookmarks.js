/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var bs = PlacesUtils.bookmarks;
var hs = PlacesUtils.history;
var os = PlacesUtils.observers;

var bookmarksObserver = {
  handlePlacesEvents(events) {
    Assert.equal(events.length, 1);
    let event = events[0];
    switch (event.type) {
      case "bookmark-added":
        bookmarksObserver._itemAddedId = event.id;
        bookmarksObserver._itemAddedParent = event.parentId;
        bookmarksObserver._itemAddedIndex = event.index;
        bookmarksObserver._itemAddedURI = event.url
          ? Services.io.newURI(event.url)
          : null;
        bookmarksObserver._itemAddedTitle = event.title;

        // Ensure that we've created a guid for this item.
        let stmt = DBConn().createStatement(
          `SELECT guid
           FROM moz_bookmarks
           WHERE id = :item_id`
        );
        stmt.params.item_id = event.id;
        Assert.ok(stmt.executeStep());
        Assert.ok(!stmt.getIsNull(0));
        do_check_valid_places_guid(stmt.row.guid);
        Assert.equal(stmt.row.guid, event.guid);
        stmt.finalize();
        break;
      case "bookmark-removed":
        bookmarksObserver._itemRemovedId = event.id;
        bookmarksObserver._itemRemovedFolder = event.parentId;
        bookmarksObserver._itemRemovedIndex = event.index;
    }
  },

  onItemChanged(
    id,
    property,
    isAnnotationProperty,
    value,
    lastModified,
    itemType,
    parentId,
    guid,
    parentGuid,
    oldValue
  ) {
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChangedValue = value;
    this._itemChangedOldValue = oldValue;
  },
  QueryInterface: ChromeUtils.generateQI(["nsINavBookmarkObserver"]),
};

// Get bookmarks menu folder id.
var root = bs.bookmarksMenuFolder;
// Index at which items should begin.
var bmStartIndex = 0;

add_task(async function test_bookmarks() {
  bs.addObserver(bookmarksObserver);
  os.addListener(
    ["bookmark-added", "bookmark-removed"],
    bookmarksObserver.handlePlacesEvents
  );

  // test special folders
  Assert.ok(bs.placesRoot > 0);
  Assert.ok(bs.bookmarksMenuFolder > 0);
  Assert.ok(bs.tagsFolder > 0);
  Assert.ok(bs.toolbarFolder > 0);

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

  // create a folder to hold all the tests
  // this makes the tests more tolerant of changes to default_places.html
  let testRoot = bs.createFolder(
    root,
    "places bookmarks xpcshell tests",
    bs.DEFAULT_INDEX
  );
  let testRootGuid = await PlacesUtils.promiseItemGuid(testRoot);
  Assert.equal(bookmarksObserver._itemAddedId, testRoot);
  Assert.equal(bookmarksObserver._itemAddedParent, root);
  Assert.equal(bookmarksObserver._itemAddedIndex, bmStartIndex);
  Assert.equal(bookmarksObserver._itemAddedURI, null);
  let testStartIndex = 0;

  // insert a bookmark.
  // the time before we insert, in microseconds
  let beforeInsert = Date.now() * 1000;
  Assert.ok(beforeInsert > 0);

  let newId = bs.insertBookmark(
    testRoot,
    uri("http://google.com/"),
    bs.DEFAULT_INDEX,
    ""
  );
  Assert.equal(bookmarksObserver._itemAddedId, newId);
  Assert.equal(bookmarksObserver._itemAddedParent, testRoot);
  Assert.equal(bookmarksObserver._itemAddedIndex, testStartIndex);
  Assert.ok(bookmarksObserver._itemAddedURI.equals(uri("http://google.com/")));

  // after just inserting, modified should not be set
  let lastModified = PlacesUtils.toPRTime(
    (
      await PlacesUtils.bookmarks.fetch(
        await PlacesUtils.promiseItemGuid(newId)
      )
    ).lastModified
  );

  // The time before we set the title, in microseconds.
  let beforeSetTitle = Date.now() * 1000;
  Assert.ok(beforeSetTitle >= beforeInsert);

  // Workaround possible VM timers issues moving lastModified and dateAdded
  // to the past.
  lastModified -= 1000;
  bs.setItemLastModified(newId, lastModified);

  // set bookmark title
  bs.setItemTitle(newId, "Google");
  Assert.equal(bookmarksObserver._itemChangedId, newId);
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");
  Assert.equal(bookmarksObserver._itemChangedValue, "Google");

  // check lastModified after we set the title
  let lastModified2 = PlacesUtils.toPRTime(
    (
      await PlacesUtils.bookmarks.fetch(
        await PlacesUtils.promiseItemGuid(newId)
      )
    ).lastModified
  );
  info("test setItemTitle");
  info("beforeSetTitle = " + beforeSetTitle);
  info("lastModified = " + lastModified);
  info("lastModified2 = " + lastModified2);
  Assert.ok(is_time_ordered(lastModified, lastModified2));

  // get item title
  let title = bs.getItemTitle(newId);
  Assert.equal(title, "Google");

  // get item title bad input
  try {
    bs.getItemTitle(-3);
    do_throw("getItemTitle accepted bad input");
  } catch (ex) {}

  // get the folder that the bookmark is in
  let folderId = bs.getFolderIdForItem(newId);
  Assert.equal(folderId, testRoot);

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
  let newId2 = bs.insertBookmark(
    workFolder,
    uri("http://developer.mozilla.org/"),
    0,
    ""
  );
  Assert.equal(bookmarksObserver._itemAddedId, newId2);
  Assert.equal(bookmarksObserver._itemAddedParent, workFolder);
  Assert.equal(bookmarksObserver._itemAddedIndex, 0);

  // change item
  bs.setItemTitle(newId2, "DevMo");
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");

  // insert item into subfolder
  let newId3 = bs.insertBookmark(
    workFolder,
    uri("http://msdn.microsoft.com/"),
    bs.DEFAULT_INDEX,
    ""
  );
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
  let newId4 = bs.insertBookmark(
    workFolder,
    uri("http://developer.mozilla.org/"),
    bs.DEFAULT_INDEX,
    ""
  );
  Assert.equal(bookmarksObserver._itemAddedId, newId4);
  Assert.equal(bookmarksObserver._itemAddedParent, workFolder);
  Assert.equal(bookmarksObserver._itemAddedIndex, 1);

  // create folder
  let homeFolder = bs.createFolder(testRoot, "Home", bs.DEFAULT_INDEX);
  Assert.equal(bookmarksObserver._itemAddedId, homeFolder);
  Assert.equal(bookmarksObserver._itemAddedParent, testRoot);
  Assert.equal(bookmarksObserver._itemAddedIndex, 2);

  // insert item
  let newId5 = bs.insertBookmark(
    homeFolder,
    uri("http://espn.com/"),
    bs.DEFAULT_INDEX,
    ""
  );
  Assert.equal(bookmarksObserver._itemAddedId, newId5);
  Assert.equal(bookmarksObserver._itemAddedParent, homeFolder);
  Assert.equal(bookmarksObserver._itemAddedIndex, 0);

  // change item
  bs.setItemTitle(newId5, "ESPN");
  Assert.equal(bookmarksObserver._itemChangedId, newId5);
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");

  // insert query item
  let uri6 = uri(
    "place:domain=google.com&type=" +
      Ci.nsINavHistoryQueryOptions.RESULTS_AS_SITE_QUERY
  );
  let newId6 = bs.insertBookmark(testRoot, uri6, bs.DEFAULT_INDEX, "");
  Assert.equal(bookmarksObserver._itemAddedParent, testRoot);
  Assert.equal(bookmarksObserver._itemAddedIndex, 3);

  // change item
  bs.setItemTitle(newId6, "Google Sites");
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");

  // test bookmark id in query output
  try {
    let options = hs.getNewQueryOptions();
    let query = hs.getNewQuery();
    query.setParents([testRootGuid]);
    let result = hs.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let cc = rootNode.childCount;
    info("bookmark itemId test: CC = " + cc);
    Assert.ok(cc > 0);
    for (let i = 0; i < cc; ++i) {
      let node = rootNode.getChild(i);
      if (
        node.type == node.RESULT_TYPE_FOLDER ||
        node.type == node.RESULT_TYPE_URI ||
        node.type == node.RESULT_TYPE_SEPARATOR ||
        node.type == node.RESULT_TYPE_QUERY
      ) {
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
    let testFolderGuid = await PlacesUtils.promiseItemGuid(testFolder);
    // add 2 bookmarks
    bs.insertBookmark(testFolder, mURI, bs.DEFAULT_INDEX, "title 1");
    bs.insertBookmark(testFolder, mURI, bs.DEFAULT_INDEX, "title 2");

    // query
    let options = hs.getNewQueryOptions();
    let query = hs.getNewQuery();
    query.setParents([testFolderGuid]);
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
  let newId10 = bs.insertBookmark(
    testRoot,
    uri("http://foo10.com/"),
    bs.DEFAULT_INDEX,
    ""
  );

  // Workaround possible VM timers issues moving lastModified and dateAdded
  // to the past.
  lastModified -= 1000;
  bs.setItemLastModified(newId10, lastModified);

  // insert a bookmark with title ZZZXXXYYY and then search for it.
  // this test confirms that we can find bookmarks that we haven't visited
  // (which are "hidden") and that we can find by title.
  // see bug #369887 for more details
  let newId13 = bs.insertBookmark(
    testRoot,
    uri("http://foobarcheese.com/"),
    bs.DEFAULT_INDEX,
    ""
  );
  Assert.equal(bookmarksObserver._itemAddedId, newId13);
  Assert.equal(bookmarksObserver._itemAddedParent, testRoot);
  Assert.equal(bookmarksObserver._itemAddedIndex, 6);

  // set bookmark title
  bs.setItemTitle(newId13, "ZZZXXXYYY");
  Assert.equal(bookmarksObserver._itemChangedId, newId13);
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");
  Assert.equal(bookmarksObserver._itemChangedValue, "ZZZXXXYYY");

  // test search on bookmark title ZZZXXXYYY
  try {
    let options = hs.getNewQueryOptions();
    options.excludeQueries = 1;
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    let query = hs.getNewQuery();
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
    let options = hs.getNewQueryOptions();
    options.excludeQueries = 1;
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    let query = hs.getNewQuery();
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
    let options = hs.getNewQueryOptions();
    let query = hs.getNewQuery();
    query.setParents([testRootGuid]);
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

  // check setItemLastModified()
  let newId14 = bs.insertBookmark(
    testRoot,
    uri("http://bar.tld/"),
    bs.DEFAULT_INDEX,
    ""
  );
  bs.setItemLastModified(newId14, 1234000000000000);
  let fakeLastModified = PlacesUtils.toPRTime(
    (
      await PlacesUtils.bookmarks.fetch(
        await PlacesUtils.promiseItemGuid(newId14)
      )
    ).lastModified
  );
  Assert.equal(fakeLastModified, 1234000000000000);

  // bug 378820
  let uri1 = uri("http://foo.tld/a");
  bs.insertBookmark(testRoot, uri1, bs.DEFAULT_INDEX, "");
  await PlacesTestUtils.addVisits(uri1);

  // bug 646993 - test bookmark titles longer than the maximum allowed length
  let title15 = Array(TITLE_LENGTH_MAX + 5).join("X");
  let title15expected = title15.substring(0, TITLE_LENGTH_MAX);
  let newId15 = bs.insertBookmark(
    testRoot,
    uri("http://evil.com/"),
    bs.DEFAULT_INDEX,
    title15
  );

  Assert.equal(bs.getItemTitle(newId15).length, title15expected.length);
  Assert.equal(bookmarksObserver._itemAddedTitle, title15expected);
  // test title length after updates
  bs.setItemTitle(newId15, title15 + " updated");
  Assert.equal(bs.getItemTitle(newId15).length, title15expected.length);
  Assert.equal(bookmarksObserver._itemChangedId, newId15);
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");
  Assert.equal(bookmarksObserver._itemChangedValue, title15expected);

  await testSimpleFolderResult();
});

async function testSimpleFolderResult() {
  // the time before we create a folder, in microseconds
  // Workaround possible VM timers issues subtracting 1us.
  let beforeCreate = Date.now() * 1000 - 1;
  Assert.ok(beforeCreate > 0);

  // create a folder
  let parent = bs.createFolder(root, "test", bs.DEFAULT_INDEX);
  let parentGuid = await PlacesUtils.promiseItemGuid(parent);

  // the time before we insert, in microseconds
  // Workaround possible VM timers issues subtracting 1ms.
  let beforeInsert = Date.now() * 1000 - 1;
  Assert.ok(beforeInsert > 0);

  // re-set item title separately so can test nodes' last modified
  let item = bs.insertBookmark(
    parent,
    uri("about:blank"),
    bs.DEFAULT_INDEX,
    ""
  );
  bs.setItemTitle(item, "test bookmark");

  // see above
  let folder = bs.createFolder(parent, "test folder", bs.DEFAULT_INDEX);
  bs.setItemTitle(folder, "test folder");

  let longName = Array(TITLE_LENGTH_MAX + 5).join("A");
  let folderLongName = bs.createFolder(parent, longName, bs.DEFAULT_INDEX);
  Assert.equal(
    bookmarksObserver._itemAddedTitle,
    longName.substring(0, TITLE_LENGTH_MAX)
  );

  let options = hs.getNewQueryOptions();
  let query = hs.getNewQuery();
  query.setParents([parentGuid]);
  let result = hs.executeQuery(query, options);
  let rootNode = result.root;
  rootNode.containerOpen = true;
  Assert.equal(rootNode.childCount, 3);

  let node = rootNode.getChild(0);
  Assert.equal(node.itemId, item);
  Assert.ok(node.dateAdded > 0);
  Assert.ok(node.lastModified > 0);
  Assert.equal(node.title, "test bookmark");
  node = rootNode.getChild(1);
  Assert.equal(node.itemId, folder);
  Assert.equal(node.title, "test folder");
  Assert.ok(node.dateAdded > 0);
  Assert.ok(node.lastModified > 0);
  node = rootNode.getChild(2);
  Assert.equal(node.itemId, folderLongName);
  Assert.equal(node.title, longName.substring(0, TITLE_LENGTH_MAX));
  Assert.ok(node.dateAdded > 0);
  Assert.ok(node.lastModified > 0);

  // update with another long title
  bs.setItemTitle(folderLongName, longName + " updated");
  Assert.equal(bookmarksObserver._itemChangedId, folderLongName);
  Assert.equal(bookmarksObserver._itemChangedProperty, "title");
  Assert.equal(
    bookmarksObserver._itemChangedValue,
    longName.substring(0, TITLE_LENGTH_MAX)
  );

  node = rootNode.getChild(2);
  Assert.equal(node.title, longName.substring(0, TITLE_LENGTH_MAX));

  rootNode.containerOpen = false;
}
