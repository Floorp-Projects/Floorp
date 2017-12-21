/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var resultObserver = {
  insertedNode: null,
  nodeInserted(parent, node, newIndex) {
    this.insertedNode = node;
  },
  removedNode: null,
  nodeRemoved(parent, node, oldIndex) {
    this.removedNode = node;
  },

  nodeAnnotationChanged() {},

  newTitle: "",
  nodeChangedByTitle: null,
  nodeTitleChanged(node, newTitle) {
    this.nodeChangedByTitle = node;
    this.newTitle = newTitle;
  },

  newAccessCount: 0,
  newTime: 0,
  nodeChangedByHistoryDetails: null,
  nodeHistoryDetailsChanged(node,
                            oldVisitDate,
                            oldVisitCount) {
    this.nodeChangedByHistoryDetails = node;
    this.newTime = node.time;
    this.newAccessCount = node.accessCount;
  },

  movedNode: null,
  nodeMoved(node, oldParent, oldIndex, newParent, newIndex) {
    this.movedNode = node;
  },
  openedContainer: null,
  closedContainer: null,
  containerStateChanged(aNode, aOldState, aNewState) {
    if (aNewState == Ci.nsINavHistoryContainerResultNode.STATE_OPENED) {
      this.openedContainer = aNode;
    } else if (aNewState == Ci.nsINavHistoryContainerResultNode.STATE_CLOSED) {
      this.closedContainer = aNode;
    }
  },
  invalidatedContainer: null,
  invalidateContainer(node) {
    this.invalidatedContainer = node;
  },
  sortingMode: null,
  sortingChanged(sortingMode) {
    this.sortingMode = sortingMode;
  },
  inBatchMode: false,
  batching(aToggleMode) {
    Assert.notEqual(this.inBatchMode, aToggleMode);
    this.inBatchMode = aToggleMode;
  },
  result: null,
  reset() {
    this.insertedNode = null;
    this.removedNode = null;
    this.nodeChangedByTitle = null;
    this.nodeChangedByHistoryDetails = null;
    this.replacedNode = null;
    this.movedNode = null;
    this.openedContainer = null;
    this.closedContainer = null;
    this.invalidatedContainer = null;
    this.sortingMode = null;
  }
};

var testURI = uri("http://mozilla.com");

add_test(function check_history_query() {
  var options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  var query = PlacesUtils.history.getNewQuery();
  var result = PlacesUtils.history.executeQuery(query, options);
  result.addObserver(resultObserver);
  var root = result.root;
  root.containerOpen = true;

  Assert.notEqual(resultObserver.openedContainer, null);

  // nsINavHistoryResultObserver.nodeInserted
  // add a visit
  PlacesTestUtils.addVisits(testURI).then(function() {
    Assert.equal(testURI.spec, resultObserver.insertedNode.uri);

    // nsINavHistoryResultObserver.nodeHistoryDetailsChanged
    // adding a visit causes nodeHistoryDetailsChanged for the folder
    Assert.equal(root.uri, resultObserver.nodeChangedByHistoryDetails.uri);

    // nsINavHistoryResultObserver.itemTitleChanged for a leaf node
    PlacesTestUtils.addVisits({ uri: testURI, title: "baz" }).then(function() {
      Assert.equal(resultObserver.nodeChangedByTitle.title, "baz");

      // nsINavHistoryResultObserver.nodeRemoved
      var removedURI = uri("http://google.com");
      PlacesTestUtils.addVisits(removedURI).then(function() {
        return PlacesUtils.history.remove(removedURI);
      }).then(function() {
        Assert.equal(removedURI.spec, resultObserver.removedNode.uri);

        // nsINavHistoryResultObserver.invalidateContainer
        PlacesUtils.history.removePagesFromHost("mozilla.com", false);
        Assert.equal(root.uri, resultObserver.invalidatedContainer.uri);

        // nsINavHistoryResultObserver.sortingChanged
        resultObserver.invalidatedContainer = null;
        result.sortingMode = options.SORT_BY_TITLE_ASCENDING;
        Assert.equal(resultObserver.sortingMode, options.SORT_BY_TITLE_ASCENDING);
        Assert.equal(resultObserver.invalidatedContainer, result.root);

        // nsINavHistoryResultObserver.invalidateContainer
        PlacesTestUtils.clearHistory().then(() => {
          Assert.equal(root.uri, resultObserver.invalidatedContainer.uri);

          // nsINavHistoryResultObserver.batching
          Assert.ok(!resultObserver.inBatchMode);
          PlacesUtils.history.runInBatchMode({
            runBatched(aUserData) {
              Assert.ok(resultObserver.inBatchMode);
            }
          }, null);
          Assert.ok(!resultObserver.inBatchMode);
          PlacesUtils.bookmarks.runInBatchMode({
            runBatched(aUserData) {
              Assert.ok(resultObserver.inBatchMode);
            }
          }, null);
          Assert.ok(!resultObserver.inBatchMode);

          root.containerOpen = false;
          Assert.equal(resultObserver.closedContainer, resultObserver.openedContainer);
          result.removeObserver(resultObserver);
          resultObserver.reset();
          PlacesTestUtils.promiseAsyncUpdates().then(run_next_test);
        });
      });
    });
  });
});

add_task(async function check_bookmarks_query() {
  var options = PlacesUtils.history.getNewQueryOptions();
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.bookmarksMenuFolder], 1);
  var result = PlacesUtils.history.executeQuery(query, options);
  result.addObserver(resultObserver);
  var root = result.root;
  root.containerOpen = true;

  Assert.notEqual(resultObserver.openedContainer, null);

  // nsINavHistoryResultObserver.nodeInserted
  // add a bookmark
  var testBookmark =
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      url: testURI,
      title: "foo"
  });
  Assert.equal("foo", resultObserver.insertedNode.title);
  Assert.equal(testURI.spec, resultObserver.insertedNode.uri);

  // nsINavHistoryResultObserver.nodeHistoryDetailsChanged
  // adding a visit causes nodeHistoryDetailsChanged for the folder
  Assert.equal(root.uri, resultObserver.nodeChangedByHistoryDetails.uri);

  // nsINavHistoryResultObserver.nodeTitleChanged for a leaf node
  await PlacesUtils.bookmarks.update({
    guid: testBookmark.guid,
    title: "baz",
  });
  Assert.equal(resultObserver.nodeChangedByTitle.title, "baz");
  Assert.equal(resultObserver.newTitle, "baz");

  var testBookmark2 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://google.com",
    title: "foo"
  });

  await PlacesUtils.bookmarks.update({
    guid: testBookmark2.guid,
    index: 0,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
  });
  Assert.equal(resultObserver.movedNode.bookmarkGuid, testBookmark2.guid);

  // nsINavHistoryResultObserver.nodeRemoved
  await PlacesUtils.bookmarks.remove(testBookmark2.guid);
  Assert.equal(testBookmark2.guid, resultObserver.removedNode.bookmarkGuid);

  // XXX nsINavHistoryResultObserver.invalidateContainer

  // nsINavHistoryResultObserver.sortingChanged
  resultObserver.invalidatedContainer = null;
  result.sortingMode = options.SORT_BY_TITLE_ASCENDING;
  Assert.equal(resultObserver.sortingMode, options.SORT_BY_TITLE_ASCENDING);
  Assert.equal(resultObserver.invalidatedContainer, result.root);

  // nsINavHistoryResultObserver.batching
  Assert.ok(!resultObserver.inBatchMode);
  PlacesUtils.history.runInBatchMode({
    runBatched(aUserData) {
      Assert.ok(resultObserver.inBatchMode);
    }
  }, null);
  Assert.ok(!resultObserver.inBatchMode);
  PlacesUtils.bookmarks.runInBatchMode({
    runBatched(aUserData) {
      Assert.ok(resultObserver.inBatchMode);
    }
  }, null);
  Assert.ok(!resultObserver.inBatchMode);

  root.containerOpen = false;
  Assert.equal(resultObserver.closedContainer, resultObserver.openedContainer);
  result.removeObserver(resultObserver);
  resultObserver.reset();
  PlacesTestUtils.promiseAsyncUpdates().then(run_next_test);
});

add_test(function check_mixed_query() {
  var options = PlacesUtils.history.getNewQueryOptions();
  var query = PlacesUtils.history.getNewQuery();
  query.onlyBookmarked = true;
  var result = PlacesUtils.history.executeQuery(query, options);
  result.addObserver(resultObserver);
  var root = result.root;
  root.containerOpen = true;

  Assert.notEqual(resultObserver.openedContainer, null);

  // nsINavHistoryResultObserver.batching
  Assert.ok(!resultObserver.inBatchMode);
  PlacesUtils.history.runInBatchMode({
    runBatched(aUserData) {
      Assert.ok(resultObserver.inBatchMode);
    }
  }, null);
  Assert.ok(!resultObserver.inBatchMode);
  PlacesUtils.bookmarks.runInBatchMode({
    runBatched(aUserData) {
      Assert.ok(resultObserver.inBatchMode);
    }
  }, null);
  Assert.ok(!resultObserver.inBatchMode);

  root.containerOpen = false;
  Assert.equal(resultObserver.closedContainer, resultObserver.openedContainer);
  result.removeObserver(resultObserver);
  resultObserver.reset();
  PlacesTestUtils.promiseAsyncUpdates().then(run_next_test);
});
