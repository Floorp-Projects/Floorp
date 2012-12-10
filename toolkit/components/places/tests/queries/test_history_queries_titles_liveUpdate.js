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
    title: "title1",
  },
  {
    isVisit: true,
    uri: "http://example.com/2/",
    lastVisit: gNow++,
    isInQuery: true,
    title: "title2",
  },
  {
    isVisit: true,
    uri: "http://example.com/3/",
    lastVisit: gNow++,
    isInQuery: true,
    title: "title3",
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

function run_test()
{
  run_next_test();
}

add_task(function test_initialize()
{
  yield task_populateDB(gTestData);
});

add_task(function pages_query()
{
  let [query, options] = newQueryWithOptions();
  testQueryContents(query, options, function (root) {
    compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let uri = NetUtil.newURI(node.uri);
      do_check_eq(node.title, gTestData[i].title);
      PlacesUtils.history.setPageTitle(uri, "changedTitle");
      do_check_eq(node.title, "changedTitle");
      PlacesUtils.history.setPageTitle(uri, gTestData[i].title);
      do_check_eq(node.title, gTestData[i].title);
    }
  });
});

add_task(function visits_query()
{
  let [query, options] = newQueryWithOptions();
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  testQueryContents(query, options, function (root) {
    compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let uri = NetUtil.newURI(node.uri);
      do_check_eq(node.title, gTestData[i].title);
      PlacesUtils.history.setPageTitle(uri, "changedTitle");
      do_check_eq(node.title, "changedTitle");
      PlacesUtils.history.setPageTitle(uri, gTestData[i].title);
      do_check_eq(node.title, gTestData[i].title);
    }
  });
});

add_task(function pages_searchterm_query()
{
  let [query, options] = newQueryWithOptions();
  query.searchTerms = "example";
  testQueryContents(query, options, function (root) {
    compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let uri = NetUtil.newURI(node.uri);
      do_check_eq(node.title, gTestData[i].title);
      PlacesUtils.history.setPageTitle(uri, "changedTitle");
      do_check_eq(node.title, "changedTitle");
      PlacesUtils.history.setPageTitle(uri, gTestData[i].title);
      do_check_eq(node.title, gTestData[i].title);
    }
  });
});

add_task(function visits_searchterm_query()
{
  let [query, options] = newQueryWithOptions();
  query.searchTerms = "example";
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  testQueryContents(query, options, function (root) {
    compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
    for (let i = 0; i < root.childCount; i++) {
      let node = root.getChild(i);
      let uri = NetUtil.newURI(node.uri);
      do_check_eq(node.title, gTestData[i].title);
      PlacesUtils.history.setPageTitle(uri, "changedTitle");
      do_check_eq(node.title, "changedTitle");
      PlacesUtils.history.setPageTitle(uri, gTestData[i].title);
      do_check_eq(node.title, gTestData[i].title);
    }
  });
});

add_task(function pages_searchterm_is_title_query()
{
  let [query, options] = newQueryWithOptions();
  query.searchTerms = "match";
  testQueryContents(query, options, function (root) {
    compareArrayToResult([], root);
    gTestData.forEach(function (data) {
      let uri = NetUtil.newURI(data.uri);
      let origTitle = data.title;
      data.title = "match";
      PlacesUtils.history.setPageTitle(uri, data.title);
      compareArrayToResult([data], root);
      data.title = origTitle;
      PlacesUtils.history.setPageTitle(uri, data.title);
      compareArrayToResult([], root);
    });
  });
});

add_task(function visits_searchterm_is_title_query()
{
  let [query, options] = newQueryWithOptions();
  query.searchTerms = "match";
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  testQueryContents(query, options, function (root) {
    compareArrayToResult([], root);
    gTestData.forEach(function (data) {
      let uri = NetUtil.newURI(data.uri);
      let origTitle = data.title;
      data.title = "match";
      PlacesUtils.history.setPageTitle(uri, data.title);
      compareArrayToResult([data], root);
      data.title = origTitle;
      PlacesUtils.history.setPageTitle(uri, data.title);
      compareArrayToResult([], root);
    });
  });
});