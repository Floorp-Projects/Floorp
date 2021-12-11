/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test ensures that tags changes are correctly live-updated in a history
// query.

let timeInMicroseconds = PlacesUtils.toPRTime(Date.now() - 10000);

function newTimeInMicroseconds() {
  timeInMicroseconds = timeInMicroseconds + 1000;
  return timeInMicroseconds;
}

var gTestData = [
  {
    isVisit: true,
    uri: "http://example.com/1/",
    lastVisit: newTimeInMicroseconds(),
    isInQuery: true,
    isBookmark: true,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    title: "example1",
  },
  {
    isVisit: true,
    uri: "http://example.com/2/",
    lastVisit: newTimeInMicroseconds(),
    isInQuery: true,
    isBookmark: true,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    title: "example2",
  },
  {
    isVisit: true,
    uri: "http://example.com/3/",
    lastVisit: newTimeInMicroseconds(),
    isInQuery: true,
    isBookmark: true,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    title: "example3",
  },
];

function newQueryWithOptions() {
  return [
    PlacesUtils.history.getNewQuery(),
    PlacesUtils.history.getNewQueryOptions(),
  ];
}

function testQueryContents(aQuery, aOptions, aCallback) {
  let root = PlacesUtils.history.executeQuery(aQuery, aOptions).root;
  root.containerOpen = true;
  aCallback(root);
  root.containerOpen = false;
}

add_task(async function test_initialize() {
  await task_populateDB(gTestData);
});

add_task(function pages_query() {
  let [query, options] = newQueryWithOptions();
  testQueryContents(query, options, function(root) {
    compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let uri = NetUtil.newURI(node.uri);
      Assert.equal(node.tags, null);
      PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
      Assert.equal(node.tags, "test-tag");
      PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
      Assert.equal(node.tags, null);
    }
  });
});

add_task(function visits_query() {
  let [query, options] = newQueryWithOptions();
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  testQueryContents(query, options, function(root) {
    compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let uri = NetUtil.newURI(node.uri);
      Assert.equal(node.tags, null);
      PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
      Assert.equal(node.tags, "test-tag");
      PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
      Assert.equal(node.tags, null);
    }
  });
});

add_task(function bookmarks_query() {
  let [query, options] = newQueryWithOptions();
  query.setParents([PlacesUtils.bookmarks.unfiledGuid]);
  testQueryContents(query, options, function(root) {
    compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let uri = NetUtil.newURI(node.uri);
      Assert.equal(node.tags, null);
      PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
      Assert.equal(node.tags, "test-tag");
      PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
      Assert.equal(node.tags, null);
    }
  });
});

add_task(function pages_searchterm_query() {
  let [query, options] = newQueryWithOptions();
  query.searchTerms = "example";
  testQueryContents(query, options, function(root) {
    compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let uri = NetUtil.newURI(node.uri);
      Assert.equal(node.tags, null);
      PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
      Assert.equal(node.tags, "test-tag");
      PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
      Assert.equal(node.tags, null);
    }
  });
});

add_task(function visits_searchterm_query() {
  let [query, options] = newQueryWithOptions();
  query.searchTerms = "example";
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  testQueryContents(query, options, function(root) {
    compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let uri = NetUtil.newURI(node.uri);
      Assert.equal(node.tags, null);
      PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
      Assert.equal(node.tags, "test-tag");
      PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
      Assert.equal(node.tags, null);
    }
  });
});

add_task(async function pages_searchterm_is_tag_query() {
  let [query, options] = newQueryWithOptions();
  query.searchTerms = "test-tag";
  let root;
  testQueryContents(query, options, rv => (root = rv));
  compareArrayToResult([], root);
  for (let data of gTestData) {
    let uri = NetUtil.newURI(data.uri);
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: uri,
      title: data.title,
    });
    PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
    compareArrayToResult([data], root);
    PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
    compareArrayToResult([], root);
  }
});

add_task(async function visits_searchterm_is_tag_query() {
  let [query, options] = newQueryWithOptions();
  query.searchTerms = "test-tag";
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  let root;
  testQueryContents(query, options, rv => (root = rv));
  compareArrayToResult([], root);
  for (let data of gTestData) {
    let uri = NetUtil.newURI(data.uri);
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: uri,
      title: data.title,
    });
    PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
    compareArrayToResult([data], root);
    PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
    compareArrayToResult([], root);
  }
});
