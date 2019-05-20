/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// a search term that matches a default bookmark
const searchTerm = "about";

var testRoot;

add_task(async function setup() {
  // create a folder to hold all the tests
  // this makes the tests more tolerant of changes to the default bookmarks set
  // also, name it using the search term, for testing that containers that match don't show up in query results
  testRoot = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: searchTerm,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
});

add_task(async function test_savedsearches_bookmarks() {
  // add a bookmark that matches the search term
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: searchTerm,
    url: "http://foo.com",
  });

  // create a saved-search that matches a default bookmark
  let search = await PlacesUtils.bookmarks.insert({
    parentGuid: testRoot.guid,
    title: searchTerm,
    url: "place:terms=" + searchTerm + "&excludeQueries=1&expandQueries=1&queryType=1",
  });

  // query for the test root, expandQueries=0
  // the query should show up as a regular bookmark
  try {
    let options = PlacesUtils.history.getNewQueryOptions();
    options.expandQueries = 0;
    let query = PlacesUtils.history.getNewQuery();
    query.setParents([testRoot.guid]);
    let result = PlacesUtils.history.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let cc = rootNode.childCount;
    Assert.equal(cc, 1);
    for (let i = 0; i < cc; i++) {
      let node = rootNode.getChild(i);
      // test that queries have valid itemId
      Assert.ok(node.itemId > 0);
      // test that the container is closed
      node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      Assert.equal(node.containerOpen, false);
    }
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("expandQueries=0 query error: " + ex);
  }

  // bookmark saved search
  // query for the test root, expandQueries=1
  // the query should show up as a query container, with 1 child
  try {
    let options = PlacesUtils.history.getNewQueryOptions();
    options.expandQueries = 1;
    let query = PlacesUtils.history.getNewQuery();
    query.setParents([testRoot.guid]);
    let result = PlacesUtils.history.executeQuery(query, options);
    let rootNode = result.root;
    rootNode.containerOpen = true;
    let cc = rootNode.childCount;
    Assert.equal(cc, 1);
    for (let i = 0; i < cc; i++) {
      let node = rootNode.getChild(i);
      // test that query node type is container when expandQueries=1
      Assert.equal(node.type, node.RESULT_TYPE_QUERY);
      // test that queries (as containers) have valid itemId
      Assert.ok(node.itemId > 0);
      node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      node.containerOpen = true;

      // test that queries have children when excludeItems=1
      // test that query nodes don't show containers (shouldn't have our folder that matches)
      // test that queries don't show themselves in query results (shouldn't have our saved search)
      Assert.equal(node.childCount, 1);

      // test that bookmark shows in query results
      var item = node.getChild(0);
      Assert.equal(item.bookmarkGuid, bookmark.guid);

      // XXX - FAILING - test live-update of query results - add a bookmark that matches the query
      // var tmpBmId = PlacesUtils.bookmarks.insertBookmark(
      //  root, uri("http://" + searchTerm + ".com"),
      //  PlacesUtils.bookmarks.DEFAULT_INDEX, searchTerm + "blah");
      // do_check_eq(query.childCount, 2);

      // XXX - test live-update of query results - delete a bookmark that matches the query
      // PlacesUtils.bookmarks.removeItem(tmpBMId);
      // do_check_eq(query.childCount, 1);

      // test live-update of query results - add a folder that matches the query
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        title: searchTerm + "zaa",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      });
      Assert.equal(node.childCount, 1);
      // test live-update of query results - add a query that matches the query
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.menuGuid,
        title: searchTerm + "blah",
        url: "place:terms=foo&excludeQueries=1&expandQueries=1&queryType=1",
      });
      Assert.equal(node.childCount, 1);
    }
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("expandQueries=1 bookmarks query: " + ex);
  }

  // delete the bookmark search
  await PlacesUtils.bookmarks.remove(search);
});

add_task(async function test_savedsearches_history() {
  // add a visit that matches the search term
  var testURI = uri("http://" + searchTerm + ".com");
  await PlacesTestUtils.addVisits({ uri: testURI, title: searchTerm });

  // create a saved-search that matches the visit we added
  var searchItem = await PlacesUtils.bookmarks.insert({
    parentGuid: testRoot.guid,
    title: searchTerm,
    url: "place:terms=" + searchTerm + "&excludeQueries=1&expandQueries=1&queryType=0",
  });

  // query for the test root, expandQueries=1
  // the query should show up as a query container, with 1 child
  try {
    var options = PlacesUtils.history.getNewQueryOptions();
    options.expandQueries = 1;
    var query = PlacesUtils.history.getNewQuery();
    query.setParents([testRoot.guid]);
    var result = PlacesUtils.history.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    Assert.equal(cc, 1);
    for (var i = 0; i < cc; i++) {
      var node = rootNode.getChild(i);
      // test that query node type is container when expandQueries=1
      Assert.equal(node.type, node.RESULT_TYPE_QUERY);
      // test that queries (as containers) have valid itemId
      Assert.equal(node.bookmarkGuid, searchItem.guid);
      node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      node.containerOpen = true;

      // test that queries have children when excludeItems=1
      // test that query nodes don't show containers (shouldn't have our folder that matches)
      // test that queries don't show themselves in query results (shouldn't have our saved search)
      Assert.equal(node.childCount, 1);

      // test that history visit shows in query results
      var item = node.getChild(0);
      Assert.equal(item.type, item.RESULT_TYPE_URI);
      Assert.equal(item.itemId, -1); // history visit
      Assert.equal(item.uri, testURI.spec); // history visit

      // test live-update of query results - add a history visit that matches the query
      await PlacesTestUtils.addVisits({
        uri: uri("http://foo.com"),
        title: searchTerm + "blah",
      });
      Assert.equal(node.childCount, 2);

      // test live-update of query results - delete a history visit that matches the query
      await PlacesUtils.history.remove("http://foo.com");
      Assert.equal(node.childCount, 1);
      node.containerOpen = false;
    }

    // test live-update of moved queries
    let tmpFolder = await PlacesUtils.bookmarks.insert({
      parentGuid: testRoot.guid,
      title: "foo",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
    });

    searchItem.parentGuid = tmpFolder.guid;
    await PlacesUtils.bookmarks.update(searchItem);
    var tmpFolderNode = rootNode.getChild(0);
    Assert.equal(tmpFolderNode.bookmarkGuid, tmpFolder.guid);
    tmpFolderNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
    tmpFolderNode.containerOpen = true;
    Assert.equal(tmpFolderNode.childCount, 1);

    // test live-update of renamed queries
    searchItem.title = "foo";
    await PlacesUtils.bookmarks.update(searchItem);
    Assert.equal(tmpFolderNode.title, "foo");

    // test live-update of deleted queries
    await PlacesUtils.bookmarks.remove(searchItem);
    Assert.throws(() => tmpFolderNode = rootNode.getChild(1), /NS_ERROR_ILLEGAL_VALUE/,
      "getting a deleted child should throw");

    tmpFolderNode.containerOpen = false;
    rootNode.containerOpen = false;
  } catch (ex) {
    do_throw("expandQueries=1 bookmarks query: " + ex);
  }
});
