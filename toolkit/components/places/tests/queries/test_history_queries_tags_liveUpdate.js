/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test ensures that tags changes are correctly live-updated in a history
// query.

let gNow = Date.now();
let gTestData = [
  {
    isVisit: true,
    uri: "http://example.com/1/",
    lastVisit: gNow,
    isInQuery: true,
    isBookmark: true,
    parentFolder: PlacesUtils.unfiledBookmarksFolderId,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    title: "example1",
  },
  {
    isVisit: true,
    uri: "http://example.com/2/",
    lastVisit: gNow++,
    isInQuery: true,
    isBookmark: true,
    parentFolder: PlacesUtils.unfiledBookmarksFolderId,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    title: "example2",
  },
  {
    isVisit: true,
    uri: "http://example.com/3/",
    lastVisit: gNow++,
    isInQuery: true,
    isBookmark: true,
    parentFolder: PlacesUtils.unfiledBookmarksFolderId,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    title: "example3",
  },
];

function newQueryWithOptions()
{
  return [ PlacesUtils.history.getNewQuery(),
           PlacesUtils.history.getNewQueryOptions() ];
}

function testQueryContents(aQuery, aOptions, aCallback)
{
  let root = PlacesUtils.history.executeQuery(aQuery, aOptions).root;
  root.containerOpen = true;
  aCallback(root);
  root.containerOpen = false;
}

let gTests = [

  function pages_query()
  {
    let [query, options] = newQueryWithOptions();
    testQueryContents(query, options, function (root) {
      compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
      for (let i = 0; i < root.childCount; i++) {
        let node = root.getChild(i);
        let uri = NetUtil.newURI(node.uri);
        do_check_eq(node.tags, null);
        PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
        do_check_eq(node.tags, "test-tag");
        PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
        do_check_eq(node.tags, null);
      }
    });
    run_next_test();
  },

  function visits_query()
  {
    let [query, options] = newQueryWithOptions();
    options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
    testQueryContents(query, options, function (root) {
      compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
      for (let i = 0; i < root.childCount; i++) {
        let node = root.getChild(i);
        let uri = NetUtil.newURI(node.uri);
        do_check_eq(node.tags, null);
        PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
        do_check_eq(node.tags, "test-tag");
        PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
        do_check_eq(node.tags, null);
      }
    });
    run_next_test();
  },

  function bookmarks_query()
  {
    let [query, options] = newQueryWithOptions();
    query.setFolders([PlacesUtils.unfiledBookmarksFolderId], 1);
    testQueryContents(query, options, function (root) {
      compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
      for (let i = 0; i < root.childCount; i++) {
        let node = root.getChild(i);
        let uri = NetUtil.newURI(node.uri);
        do_check_eq(node.tags, null);
        PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
        do_check_eq(node.tags, "test-tag");
        PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
        do_check_eq(node.tags, null);
      }
    });
    run_next_test();
  },

  function pages_searchterm_query()
  {
    let [query, options] = newQueryWithOptions();
    query.searchTerms = "example";
    testQueryContents(query, options, function (root) {
      compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
      for (let i = 0; i < root.childCount; i++) {
        let node = root.getChild(i);
        let uri = NetUtil.newURI(node.uri);
        do_check_eq(node.tags, null);
        PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
        do_check_eq(node.tags, "test-tag");
        PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
        do_check_eq(node.tags, null);
      }
    });
    run_next_test();
  },

  function visits_searchterm_query()
  {
    let [query, options] = newQueryWithOptions();
    query.searchTerms = "example";
    options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
    testQueryContents(query, options, function (root) {
      compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
      for (let i = 0; i < root.childCount; i++) {
        let node = root.getChild(i);
        let uri = NetUtil.newURI(node.uri);
        do_check_eq(node.tags, null);
        PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
        do_check_eq(node.tags, "test-tag");
        PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
        do_check_eq(node.tags, null);
      }
    });
    run_next_test();
  },

  function pages_searchterm_is_tag_query()
  {
    let [query, options] = newQueryWithOptions();
    query.searchTerms = "test-tag";
    testQueryContents(query, options, function (root) {
      compareArrayToResult([], root);
      gTestData.forEach(function (data) {
        let uri = NetUtil.newURI(data.uri);
        PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
        compareArrayToResult([data], root);
        PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
        compareArrayToResult([], root);
      });
    });
    run_next_test();
  },

  function visits_searchterm_is_tag_query()
  {
    let [query, options] = newQueryWithOptions();
    query.searchTerms = "test-tag";
    options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
    testQueryContents(query, options, function (root) {
      compareArrayToResult([], root);
      gTestData.forEach(function (data) {
        let uri = NetUtil.newURI(data.uri);
        PlacesUtils.tagging.tagURI(uri, ["test-tag"]);
        compareArrayToResult([data], root);
        PlacesUtils.tagging.untagURI(uri, ["test-tag"]);
        compareArrayToResult([], root);
      });
    });
    run_next_test();
  },

];

function run_test()
{
  populateDB(gTestData);
  run_next_test();
}
