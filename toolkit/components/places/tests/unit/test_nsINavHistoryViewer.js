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
                                         updatedVisitDate,
                                         updatedVisitCount) {
    this.nodeChangedByHistoryDetails = node
    this.newTime = updatedVisitDate;
    this.newAccessCount = updatedVisitCount;
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
    do_check_neq(this.inBatchMode, aToggleMode);
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

function run_test() {
  run_next_test();
}

add_test(function check_history_query() {
  var options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  var query = PlacesUtils.history.getNewQuery();
  var result = PlacesUtils.history.executeQuery(query, options);
  result.addObserver(resultObserver);
  var root = result.root;
  root.containerOpen = true;

  do_check_neq(resultObserver.openedContainer, null);

  // nsINavHistoryResultObserver.nodeInserted
  // add a visit
  PlacesTestUtils.addVisits(testURI).then(function() {
    do_check_eq(testURI.spec, resultObserver.insertedNode.uri);

    // nsINavHistoryResultObserver.nodeHistoryDetailsChanged
    // adding a visit causes nodeHistoryDetailsChanged for the folder
    do_check_eq(root.uri, resultObserver.nodeChangedByHistoryDetails.uri);

    // nsINavHistoryResultObserver.itemTitleChanged for a leaf node
    PlacesTestUtils.addVisits({ uri: testURI, title: "baz" }).then(function() {
      do_check_eq(resultObserver.nodeChangedByTitle.title, "baz");

      // nsINavHistoryResultObserver.nodeRemoved
      var removedURI = uri("http://google.com");
      PlacesTestUtils.addVisits(removedURI).then(function() {
        return PlacesUtils.history.remove(removedURI);
      }).then(function() {
        do_check_eq(removedURI.spec, resultObserver.removedNode.uri);

        // nsINavHistoryResultObserver.invalidateContainer
        PlacesUtils.history.removePagesFromHost("mozilla.com", false);
        do_check_eq(root.uri, resultObserver.invalidatedContainer.uri);

        // nsINavHistoryResultObserver.sortingChanged
        resultObserver.invalidatedContainer = null;
        result.sortingMode = options.SORT_BY_TITLE_ASCENDING;
        do_check_eq(resultObserver.sortingMode, options.SORT_BY_TITLE_ASCENDING);
        do_check_eq(resultObserver.invalidatedContainer, result.root);

        // nsINavHistoryResultObserver.invalidateContainer
        PlacesTestUtils.clearHistoryEnabled().then(() => {
          do_check_eq(root.uri, resultObserver.invalidatedContainer.uri);

          // nsINavHistoryResultObserver.batching
          do_check_false(resultObserver.inBatchMode);
          PlacesUtils.history.runInBatchMode({
            runBatched(aUserData) {
              do_check_true(resultObserver.inBatchMode);
            }
          }, null);
          do_check_false(resultObserver.inBatchMode);
          PlacesUtils.bookmarks.runInBatchMode({
            runBatched(aUserData) {
              do_check_true(resultObserver.inBatchMode);
            }
          }, null);
          do_check_false(resultObserver.inBatchMode);

          root.containerOpen = false;
          do_check_eq(resultObserver.closedContainer, resultObserver.openedContainer);
          result.removeObserver(resultObserver);
          resultObserver.reset();
          PlacesTestUtils.promiseAsyncUpdates().then(run_next_test);
        });
      });
    });
  });
});

add_test(function check_bookmarks_query() {
  var options = PlacesUtils.history.getNewQueryOptions();
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.bookmarksMenuFolder], 1);
  var result = PlacesUtils.history.executeQuery(query, options);
  result.addObserver(resultObserver);
  var root = result.root;
  root.containerOpen = true;

  do_check_neq(resultObserver.openedContainer, null);

  // nsINavHistoryResultObserver.nodeInserted
  // add a bookmark
  var testBookmark =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.bookmarksMenuFolder,
                                         testURI,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "foo");
  do_check_eq("foo", resultObserver.insertedNode.title);
  do_check_eq(testURI.spec, resultObserver.insertedNode.uri);

  // nsINavHistoryResultObserver.nodeHistoryDetailsChanged
  // adding a visit causes nodeHistoryDetailsChanged for the folder
  do_check_eq(root.uri, resultObserver.nodeChangedByHistoryDetails.uri);

  // nsINavHistoryResultObserver.nodeTitleChanged for a leaf node
  PlacesUtils.bookmarks.setItemTitle(testBookmark, "baz");
  do_check_eq(resultObserver.nodeChangedByTitle.title, "baz");
  do_check_eq(resultObserver.newTitle, "baz");

  var testBookmark2 =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.bookmarksMenuFolder,
                                         uri("http://google.com"),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "foo");
  PlacesUtils.bookmarks.moveItem(testBookmark2, PlacesUtils.bookmarks.bookmarksMenuFolder, 0);
  do_check_eq(resultObserver.movedNode.itemId, testBookmark2);

  // nsINavHistoryResultObserver.nodeRemoved
  PlacesUtils.bookmarks.removeItem(testBookmark2);
  do_check_eq(testBookmark2, resultObserver.removedNode.itemId);

  // XXX nsINavHistoryResultObserver.invalidateContainer

  // nsINavHistoryResultObserver.sortingChanged
  resultObserver.invalidatedContainer = null;
  result.sortingMode = options.SORT_BY_TITLE_ASCENDING;
  do_check_eq(resultObserver.sortingMode, options.SORT_BY_TITLE_ASCENDING);
  do_check_eq(resultObserver.invalidatedContainer, result.root);

  // nsINavHistoryResultObserver.batching
  do_check_false(resultObserver.inBatchMode);
  PlacesUtils.history.runInBatchMode({
    runBatched(aUserData) {
      do_check_true(resultObserver.inBatchMode);
    }
  }, null);
  do_check_false(resultObserver.inBatchMode);
  PlacesUtils.bookmarks.runInBatchMode({
    runBatched(aUserData) {
      do_check_true(resultObserver.inBatchMode);
    }
  }, null);
  do_check_false(resultObserver.inBatchMode);

  root.containerOpen = false;
  do_check_eq(resultObserver.closedContainer, resultObserver.openedContainer);
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

  do_check_neq(resultObserver.openedContainer, null);

  // nsINavHistoryResultObserver.batching
  do_check_false(resultObserver.inBatchMode);
  PlacesUtils.history.runInBatchMode({
    runBatched(aUserData) {
      do_check_true(resultObserver.inBatchMode);
    }
  }, null);
  do_check_false(resultObserver.inBatchMode);
  PlacesUtils.bookmarks.runInBatchMode({
    runBatched(aUserData) {
      do_check_true(resultObserver.inBatchMode);
    }
  }, null);
  do_check_false(resultObserver.inBatchMode);

  root.containerOpen = false;
  do_check_eq(resultObserver.closedContainer, resultObserver.openedContainer);
  result.removeObserver(resultObserver);
  resultObserver.reset();
  PlacesTestUtils.promiseAsyncUpdates().then(run_next_test);
});
