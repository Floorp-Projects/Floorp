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
    title: "title1",
  },
  {
    isVisit: true,
    uri: "http://example.com/2/",
    lastVisit: newTimeInMicroseconds(),
    isInQuery: true,
    title: "title2",
  },
  {
    isVisit: true,
    uri: "http://example.com/3/",
    lastVisit: newTimeInMicroseconds(),
    isInQuery: true,
    title: "title3",
  },
];

function searchNodeHavingUrl(aRoot, aUrl) {
  for (let i = 0; i < aRoot.childCount; i++) {
    if (aRoot.getChild(i).uri == aUrl) {
      return aRoot.getChild(i);
    }
  }
  return undefined;
}

function newQueryWithOptions()
{
  return [ PlacesUtils.history.getNewQuery(),
           PlacesUtils.history.getNewQueryOptions() ];
}

function run_test()
{
  run_next_test();
}

add_task(function* pages_query()
{
  yield task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
  for (let i = 0; i < root.childCount; i++) {
    let node = root.getChild(i);
    do_check_eq(node.title, gTestData[i].title);
    let uri = NetUtil.newURI(node.uri);
    yield PlacesTestUtils.addVisits({uri, title: "changedTitle"});
    do_check_eq(node.title, "changedTitle");
    yield PlacesTestUtils.addVisits({uri, title: gTestData[i].title});
    do_check_eq(node.title, gTestData[i].title);
  }

  root.containerOpen = false;
  yield PlacesTestUtils.clearHistory();
});

add_task(function* visits_query()
{
  yield task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);

  for (let testData of gTestData) {
    let uri = NetUtil.newURI(testData.uri);
    let node = searchNodeHavingUrl(root, testData.uri);
    do_check_eq(node.title, testData.title);
    yield PlacesTestUtils.addVisits({uri, title: "changedTitle"});
    node = searchNodeHavingUrl(root, testData.uri);
    do_check_eq(node.title, "changedTitle");
    yield PlacesTestUtils.addVisits({uri, title: testData.title});
    node = searchNodeHavingUrl(root, testData.uri);
    do_check_eq(node.title, testData.title);
  }

  root.containerOpen = false;
  yield PlacesTestUtils.clearHistory();
});

add_task(function* pages_searchterm_query()
{
  yield task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  query.searchTerms = "example";
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
  for (let i = 0; i < root.childCount; i++) {
    let node = root.getChild(i);
    let uri = NetUtil.newURI(node.uri);
    do_check_eq(node.title, gTestData[i].title);
    yield PlacesTestUtils.addVisits({uri, title: "changedTitle"});
    do_check_eq(node.title, "changedTitle");
    yield PlacesTestUtils.addVisits({uri, title: gTestData[i].title});
    do_check_eq(node.title, gTestData[i].title);
  }

  root.containerOpen = false;
  yield PlacesTestUtils.clearHistory();
});

add_task(function* visits_searchterm_query()
{
  yield task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  query.searchTerms = "example";
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
  for (let testData of gTestData) {
    let uri = NetUtil.newURI(testData.uri);
    let node = searchNodeHavingUrl(root, testData.uri);
    do_check_eq(node.title, testData.title);
    yield PlacesTestUtils.addVisits({uri, title: "changedTitle"});
    node = searchNodeHavingUrl(root, testData.uri);
    do_check_eq(node.title, "changedTitle");
    yield PlacesTestUtils.addVisits({uri, title: testData.title});
    node = searchNodeHavingUrl(root, testData.uri);
    do_check_eq(node.title, testData.title);
  }

  root.containerOpen = false;
  yield PlacesTestUtils.clearHistory();
});

add_task(function* pages_searchterm_is_title_query()
{
  yield task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  query.searchTerms = "match";
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  compareArrayToResult([], root);
  for (let data of gTestData) {
    let uri = NetUtil.newURI(data.uri);
    let origTitle = data.title;
    data.title = "match";
    yield PlacesTestUtils.addVisits({ uri, title: data.title,
                                      visitDate: data.lastVisit });
    compareArrayToResult([data], root);
    data.title = origTitle;
    yield PlacesTestUtils.addVisits({ uri, title: data.title,
                                      visitDate: data.lastVisit });
    compareArrayToResult([], root);
  }

  root.containerOpen = false;
  yield PlacesTestUtils.clearHistory();
});

add_task(function* visits_searchterm_is_title_query()
{
  yield task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  query.searchTerms = "match";
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  compareArrayToResult([], root);
  for (let data of gTestData) {
    let uri = NetUtil.newURI(data.uri);
    let origTitle = data.title;
    data.title = "match";
    yield PlacesTestUtils.addVisits({ uri, title: data.title,
                                      visitDate: data.lastVisit });
    compareArrayToResult([data], root);
    data.title = origTitle;
    yield PlacesTestUtils.addVisits({ uri, title: data.title,
                                      visitDate: data.lastVisit });
    compareArrayToResult([], root);
  }

  root.containerOpen = false;
  yield PlacesTestUtils.clearHistory();
});
