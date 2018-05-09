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

add_task(async function check_history_query() {
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  let query = PlacesUtils.history.getNewQuery();
  let result = PlacesUtils.history.executeQuery(query, options);
  result.addObserver(resultObserver);
  let root = result.root;
  root.containerOpen = true;

  Assert.notEqual(resultObserver.openedContainer, null);

  // nsINavHistoryResultObserver.nodeInserted
  // add a visit
  await PlacesTestUtils.addVisits(testURI);
  Assert.equal(testURI.spec, resultObserver.insertedNode.uri);

  // nsINavHistoryResultObserver.nodeHistoryDetailsChanged
  // adding a visit causes nodeHistoryDetailsChanged for the folder
  Assert.equal(root.uri, resultObserver.nodeChangedByHistoryDetails.uri);

  // nsINavHistoryResultObserver.itemTitleChanged for a leaf node
  await PlacesTestUtils.addVisits({ uri: testURI, title: "baz" });
  Assert.equal(resultObserver.nodeChangedByTitle.title, "baz");

  // nsINavHistoryResultObserver.nodeRemoved
  let removedURI = uri("http://google.com");
  await PlacesTestUtils.addVisits(removedURI);
  await PlacesUtils.history.remove(removedURI);
  Assert.equal(removedURI.spec, resultObserver.removedNode.uri);

  // nsINavHistoryResultObserver.invalidateContainer
  await PlacesUtils.history.removeByFilter({ host: "mozilla.com" });
  // This test is disabled for bug 1089691. It is failing bcause the new API
  // doesn't send batching notifications and thus the result doesn't invalidate
  // the whole container.
  // Assert.equal(root.uri, resultObserver.invalidatedContainer.uri);

  // nsINavHistoryResultObserver.sortingChanged
  resultObserver.invalidatedContainer = null;
  result.sortingMode = options.SORT_BY_TITLE_ASCENDING;
  Assert.equal(resultObserver.sortingMode, options.SORT_BY_TITLE_ASCENDING);
  Assert.equal(resultObserver.invalidatedContainer, result.root);

  // nsINavHistoryResultObserver.invalidateContainer
  await PlacesUtils.history.clear();
  Assert.equal(root.uri, resultObserver.invalidatedContainer.uri);

  root.containerOpen = false;
  Assert.equal(resultObserver.closedContainer, resultObserver.openedContainer);
  result.removeObserver(resultObserver);
  resultObserver.reset();
  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function check_bookmarks_query() {
  let options = PlacesUtils.history.getNewQueryOptions();
  let query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarksMenuFolderId], 1);
  let result = PlacesUtils.history.executeQuery(query, options);
  result.addObserver(resultObserver);
  let root = result.root;
  root.containerOpen = true;

  Assert.notEqual(resultObserver.openedContainer, null);

  // nsINavHistoryResultObserver.nodeInserted
  // add a bookmark
  let testBookmark = await PlacesUtils.bookmarks.insert({
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

  let testBookmark2 = await PlacesUtils.bookmarks.insert({
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
  result.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING;
  Assert.equal(resultObserver.sortingMode, Ci.nsINavHistoryQueryOptions.SORT_BY_TITLE_ASCENDING);
  Assert.equal(resultObserver.invalidatedContainer, result.root);

  root.containerOpen = false;
  Assert.equal(resultObserver.closedContainer, resultObserver.openedContainer);
  result.removeObserver(resultObserver);
  resultObserver.reset();
  await PlacesTestUtils.promiseAsyncUpdates();
});

add_task(async function check_mixed_query() {
  let options = PlacesUtils.history.getNewQueryOptions();
  let query = PlacesUtils.history.getNewQuery();
  query.onlyBookmarked = true;
  let result = PlacesUtils.history.executeQuery(query, options);
  result.addObserver(resultObserver);
  let root = result.root;
  root.containerOpen = true;

  Assert.notEqual(resultObserver.openedContainer, null);

  root.containerOpen = false;
  Assert.equal(resultObserver.closedContainer, resultObserver.openedContainer);
  result.removeObserver(resultObserver);
  resultObserver.reset();
  await PlacesTestUtils.promiseAsyncUpdates();
});
