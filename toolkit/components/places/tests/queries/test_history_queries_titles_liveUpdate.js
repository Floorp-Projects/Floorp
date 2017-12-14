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

function newQueryWithOptions() {
  return [ PlacesUtils.history.getNewQuery(),
           PlacesUtils.history.getNewQueryOptions() ];
}

add_task(async function pages_query() {
  await task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
  for (let i = 0; i < root.childCount; i++) {
    let node = root.getChild(i);
    Assert.equal(node.title, gTestData[i].title);
    let uri = NetUtil.newURI(node.uri);
    await PlacesTestUtils.addVisits({uri, title: "changedTitle"});
    Assert.equal(node.title, "changedTitle");
    await PlacesTestUtils.addVisits({uri, title: gTestData[i].title});
    Assert.equal(node.title, gTestData[i].title);
  }

  root.containerOpen = false;
  await PlacesTestUtils.clearHistory();
});

add_task(async function visits_query() {
  await task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);

  for (let testData of gTestData) {
    let uri = NetUtil.newURI(testData.uri);
    let node = searchNodeHavingUrl(root, testData.uri);
    Assert.equal(node.title, testData.title);
    await PlacesTestUtils.addVisits({uri, title: "changedTitle"});
    node = searchNodeHavingUrl(root, testData.uri);
    Assert.equal(node.title, "changedTitle");
    await PlacesTestUtils.addVisits({uri, title: testData.title});
    node = searchNodeHavingUrl(root, testData.uri);
    Assert.equal(node.title, testData.title);
  }

  root.containerOpen = false;
  await PlacesTestUtils.clearHistory();
});

add_task(async function pages_searchterm_query() {
  await task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  query.searchTerms = "example";
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
  for (let i = 0; i < root.childCount; i++) {
    let node = root.getChild(i);
    let uri = NetUtil.newURI(node.uri);
    Assert.equal(node.title, gTestData[i].title);
    await PlacesTestUtils.addVisits({uri, title: "changedTitle"});
    Assert.equal(node.title, "changedTitle");
    await PlacesTestUtils.addVisits({uri, title: gTestData[i].title});
    Assert.equal(node.title, gTestData[i].title);
  }

  root.containerOpen = false;
  await PlacesTestUtils.clearHistory();
});

add_task(async function visits_searchterm_query() {
  await task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  query.searchTerms = "example";
  options.resultType = Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  compareArrayToResult([gTestData[0], gTestData[1], gTestData[2]], root);
  for (let testData of gTestData) {
    let uri = NetUtil.newURI(testData.uri);
    let node = searchNodeHavingUrl(root, testData.uri);
    Assert.equal(node.title, testData.title);
    await PlacesTestUtils.addVisits({uri, title: "changedTitle"});
    node = searchNodeHavingUrl(root, testData.uri);
    Assert.equal(node.title, "changedTitle");
    await PlacesTestUtils.addVisits({uri, title: testData.title});
    node = searchNodeHavingUrl(root, testData.uri);
    Assert.equal(node.title, testData.title);
  }

  root.containerOpen = false;
  await PlacesTestUtils.clearHistory();
});

add_task(async function pages_searchterm_is_title_query() {
  await task_populateDB(gTestData);

  let [query, options] = newQueryWithOptions();
  query.searchTerms = "match";
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  compareArrayToResult([], root);
  for (let data of gTestData) {
    let uri = NetUtil.newURI(data.uri);
    let origTitle = data.title;
    data.title = "match";
    await PlacesTestUtils.addVisits({ uri, title: data.title,
                                      visitDate: data.lastVisit });
    compareArrayToResult([data], root);
    data.title = origTitle;
    await PlacesTestUtils.addVisits({ uri, title: data.title,
                                      visitDate: data.lastVisit });
    compareArrayToResult([], root);
  }

  root.containerOpen = false;
  await PlacesTestUtils.clearHistory();
});

add_task(async function visits_searchterm_is_title_query() {
  await task_populateDB(gTestData);

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

    info("Adding " + uri.spec);
    await PlacesTestUtils.addVisits({ uri, title: data.title,
                                      visitDate: data.lastVisit });

    compareArrayToResult([data], root);
    data.title = origTitle;
    info("Clobbering " + uri.spec);
    await PlacesTestUtils.addVisits({ uri, title: data.title,
                                      visitDate: data.lastVisit });

    compareArrayToResult([], root);
  }

  root.containerOpen = false;
  await PlacesTestUtils.clearHistory();
});
