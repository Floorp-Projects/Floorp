/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test ensures that download history (filtered by transition) queries
// don't invalidate (and requery) too often.

function accumulateNotifications(result) {
  let notifications = [];
  let resultObserver = new Proxy(NavHistoryResultObserver, {
    get(target, name) {
      if (name == "check") {
        result.removeObserver(resultObserver, false);
        return expectedNotifications =>
          Assert.deepEqual(notifications, expectedNotifications);
      }
      // ignore a few uninteresting notifications.
      if (["QueryInterface", "containerStateChanged"].includes(name))
        return () => {};
      return () => {
        notifications.push(name);
      };
    }
  });
  result.addObserver(resultObserver, false);
  return resultObserver;
}

add_task(async function test_downloadhistory_query_notifications() {
  const MAX_RESULTS = 5;
  let query = PlacesUtils.history.getNewQuery();
  query.setTransitions([PlacesUtils.history.TRANSITIONS.DOWNLOAD], 1);
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;
  options.maxResults = MAX_RESULTS;
  let result = PlacesUtils.history.executeQuery(query, options);
  let notifications = accumulateNotifications(result);
  let root = PlacesUtils.asContainer(result.root);
  root.containerOpen = true;
  // Add more maxResults downloads in order.
  let transitions = Object.values(PlacesUtils.history.TRANSITIONS);
  for (let transition of transitions) {
    let uri = Services.io.newURI("http://fx-search.com/" + transition);
    await PlacesTestUtils.addVisits({ uri, transition, title: "test " + transition });
    // For each visit also set apart:
    //  - a bookmark
    //  - an annotation
    //  - an icon
    await PlacesUtils.bookmarks.insert({
      url: uri,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    });
    PlacesUtils.annotations.setPageAnnotation(uri, "test/anno", "testValue", 0,
                                              PlacesUtils.annotations.EXPIRE_WITH_HISTORY);
    await PlacesTestUtils.addFavicons(new Map([[uri.spec, SMALLPNG_DATA_URI.spec]]));
  }
  // Remove all the visits one by one.
  for (let transition of transitions) {
    let uri = Services.io.newURI("http://fx-search.com/" + transition);
    await PlacesUtils.history.remove(uri);
  }
  root.containerOpen = false;
  // We pretty much don't want to see invalidateContainer here, because that
  // means we requeried.
  // We also don't want to see changes caused by filtered-out transition types.
  notifications.check(["nodeHistoryDetailsChanged",
                       "nodeInserted",
                       "nodeTitleChanged",
                       "nodeIconChanged",
                       "nodeRemoved"]);
});

add_task(async function test_downloadhistory_query_filtering() {
  const MAX_RESULTS = 3;
  let query = PlacesUtils.history.getNewQuery();
  query.setTransitions([PlacesUtils.history.TRANSITIONS.DOWNLOAD], 1);
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;
  options.maxResults = MAX_RESULTS;
  let result = PlacesUtils.history.executeQuery(query, options);
  let root = PlacesUtils.asContainer(result.root);
  root.containerOpen = true;
  Assert.equal(root.childCount, 0, "No visits found");
  // Add more than maxResults downloads.
  let uris = [];
  // Define a monotonic visit date to ensure results order stability.
  let visitDate = Date.now() * 1000;
  for (let i = 0; i < MAX_RESULTS + 1; ++i, visitDate += 1000) {
    let uri = `http://fx-search.com/download/${i}`;
    await PlacesTestUtils.addVisits({
      uri,
      transition: PlacesUtils.history.TRANSITIONS.DOWNLOAD,
      visitDate
    });
    uris.push(uri);
  }
  // Add an older download visit out of the maxResults timeframe.
  await PlacesTestUtils.addVisits({
    uri: `http://fx-search.com/download/unordered`,
    transition: PlacesUtils.history.TRANSITIONS.DOWNLOAD,
    visitDate: new Date(Date.now() - 7200000)
  });

  Assert.equal(root.childCount, MAX_RESULTS, "Result should be limited");
  // Invert the uris array because we are sorted by date descending.
  uris.reverse();
  for (let i = 0; i < root.childCount; ++i) {
    let node = root.getChild(i);
    Assert.equal(node.uri, uris[i], "Found the expected uri");
  }

  root.containerOpen = false;
});
