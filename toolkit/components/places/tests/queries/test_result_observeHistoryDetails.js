/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test ensures that skipHistoryDetailsNotifications works as expected.

function accumulateNotifications(
  result,
  skipHistoryDetailsNotifications = false
) {
  let notifications = [];
  let resultObserver = new Proxy(NavHistoryResultObserver, {
    get(target, name) {
      if (name == "check") {
        result.removeObserver(resultObserver, false);
        return expectedNotifications =>
          Assert.deepEqual(notifications, expectedNotifications);
      }
      if (name == "skipHistoryDetailsNotifications") {
        return skipHistoryDetailsNotifications;
      }
      // ignore a few uninteresting notifications.
      if (["QueryInterface", "containerStateChanged"].includes(name)) {
        return () => {};
      }
      return () => {
        notifications.push(name);
      };
    },
  });
  result.addObserver(resultObserver, false);
  return resultObserver;
}

add_task(async function test_history_query_observe() {
  let query = PlacesUtils.history.getNewQuery();
  let options = PlacesUtils.history.getNewQueryOptions();
  let result = PlacesUtils.history.executeQuery(query, options);
  let notifications = accumulateNotifications(result);
  let root = PlacesUtils.asContainer(result.root);
  root.containerOpen = true;

  await PlacesTestUtils.addVisits({
    uri: "http://mozilla.org",
    title: "test",
  });
  notifications.check([
    "nodeHistoryDetailsChanged",
    "nodeInserted",
    "nodeTitleChanged",
  ]);

  root.containerOpen = false;
  await PlacesUtils.history.clear();
});

add_task(async function test_history_query_no_observe() {
  let query = PlacesUtils.history.getNewQuery();
  let options = PlacesUtils.history.getNewQueryOptions();
  let result = PlacesUtils.history.executeQuery(query, options);
  // Even if we opt-out of notifications, this is an history query, thus the
  // setting is pretty much ignored.
  let notifications = accumulateNotifications(result, true);
  let root = PlacesUtils.asContainer(result.root);
  root.containerOpen = true;

  await PlacesTestUtils.addVisits({
    uri: "http://mozilla.org",
    title: "test",
  });
  await PlacesTestUtils.addVisits({
    uri: "http://mozilla2.org",
    title: "test",
  });

  notifications.check([
    "nodeHistoryDetailsChanged",
    "nodeInserted",
    "nodeTitleChanged",
    "nodeHistoryDetailsChanged",
    "nodeInserted",
    "nodeTitleChanged",
  ]);

  root.containerOpen = false;
  await PlacesUtils.history.clear();
});

add_task(async function test_bookmarks_query_observe() {
  let query = PlacesUtils.history.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.toolbarGuid]);
  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let result = PlacesUtils.history.executeQuery(query, options);
  let notifications = accumulateNotifications(result);
  let root = PlacesUtils.asContainer(result.root);
  root.containerOpen = true;

  await PlacesUtils.bookmarks.insert({
    url: "http://mozilla.org",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "test",
  });
  await PlacesTestUtils.addVisits({
    uri: "http://mozilla.org",
    title: "title",
  });

  notifications.check([
    "nodeHistoryDetailsChanged",
    "nodeInserted",
    "nodeHistoryDetailsChanged",
  ]);

  root.containerOpen = false;
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_bookmarks_query_no_observe() {
  let query = PlacesUtils.history.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.toolbarGuid]);
  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let result = PlacesUtils.history.executeQuery(query, options);
  let notifications = accumulateNotifications(result, true);
  let root = PlacesUtils.asContainer(result.root);
  root.containerOpen = true;

  await PlacesUtils.bookmarks.insert({
    url: "http://mozilla.org",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "test",
  });
  await PlacesTestUtils.addVisits({
    uri: "http://mozilla.org",
    title: "title",
  });

  notifications.check(["nodeInserted"]);

  info("Change the sorting mode to one that is based on history");
  notifications = accumulateNotifications(result, true);
  result.sortingMode = options.SORT_BY_VISITCOUNT_DESCENDING;
  notifications.check(["invalidateContainer"]);

  notifications = accumulateNotifications(result, true);
  await PlacesTestUtils.addVisits({
    uri: "http://mozilla.org",
    title: "title",
  });
  notifications.check(["nodeHistoryDetailsChanged"]);

  root.containerOpen = false;
  await PlacesUtils.bookmarks.eraseEverything();
});
