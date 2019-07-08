/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const JS_NOW = Date.now();
const DB_NOW = JS_NOW * 1000;
const TEST_URI = uri("http://example.com/");

async function cleanup() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  // This is needed to remove place: entries.
  DBConn().executeSimpleSQL("DELETE FROM moz_places");
}

add_task(async function remove_visits_outside_unbookmarked_uri() {
  info(
    "*** TEST: Remove some visits outside valid timeframe from an unbookmarked URI"
  );

  info("Add 10 visits for the URI from way in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - 100000 - i * 1000 });
  }
  await PlacesTestUtils.addVisits(visits);

  info("Remove visits using timerange outside the URI's visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 10),
    endDate: new Date(JS_NOW),
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

  info("URI should still exist in moz_places.");
  Assert.ok(page_in_database(TEST_URI.spec));

  info("Run a history query and check that all visits still exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 10);
  for (let i = 0; i < root.childCount; i++) {
    let visitTime = root.getChild(i).time;
    Assert.equal(visitTime, DB_NOW - 100000 - i * 1000);
  }
  root.containerOpen = false;

  Assert.ok(
    await PlacesUtils.history.hasVisits(TEST_URI),
    "visit should exist"
  );

  await PlacesTestUtils.promiseAsyncUpdates();
  info("Frecency should be positive.");
  Assert.ok(frecencyForUrl(TEST_URI) > 0);

  await cleanup();
});

add_task(async function remove_visits_outside_bookmarked_uri() {
  info(
    "*** TEST: Remove some visits outside valid timeframe from a bookmarked URI"
  );

  info("Add 10 visits for the URI from way in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - 100000 - i * 1000 });
  }
  await PlacesTestUtils.addVisits(visits);
  info("Bookmark the URI.");
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URI,
    title: "bookmark title",
  });

  info("Remove visits using timerange outside the URI's visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 10),
    endDate: new Date(JS_NOW),
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

  info("URI should still exist in moz_places.");
  Assert.ok(page_in_database(TEST_URI.spec));

  info("Run a history query and check that all visits still exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 10);
  for (let i = 0; i < root.childCount; i++) {
    let visitTime = root.getChild(i).time;
    Assert.equal(visitTime, DB_NOW - 100000 - i * 1000);
  }
  root.containerOpen = false;

  Assert.ok(
    await PlacesUtils.history.hasVisits(TEST_URI),
    "visit should exist"
  );
  await PlacesTestUtils.promiseAsyncUpdates();

  info("Frecency should be positive.");
  Assert.ok(frecencyForUrl(TEST_URI) > 0);

  await cleanup();
});

add_task(async function remove_visits_unbookmarked_uri() {
  info("*** TEST: Remove some visits from an unbookmarked URI");

  info("Add 10 visits for the URI from now to 9 usecs in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - i * 1000 });
  }
  await PlacesTestUtils.addVisits(visits);

  info("Remove the 5 most recent visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 4),
    endDate: new Date(JS_NOW),
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

  info("URI should still exist in moz_places.");
  Assert.ok(page_in_database(TEST_URI.spec));

  info(
    "Run a history query and check that only the older 5 visits still exist."
  );
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 5);
  for (let i = 0; i < root.childCount; i++) {
    let visitTime = root.getChild(i).time;
    Assert.equal(visitTime, DB_NOW - i * 1000 - 5000);
  }
  root.containerOpen = false;

  Assert.ok(
    await PlacesUtils.history.hasVisits(TEST_URI),
    "visit should exist"
  );
  await PlacesTestUtils.promiseAsyncUpdates();

  info("Frecency should be positive.");
  Assert.ok(frecencyForUrl(TEST_URI) > 0);

  await cleanup();
});

add_task(async function remove_visits_bookmarked_uri() {
  info("*** TEST: Remove some visits from a bookmarked URI");

  info("Add 10 visits for the URI from now to 9 usecs in the past.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - i * 1000 });
  }
  await PlacesTestUtils.addVisits(visits);
  info("Bookmark the URI.");
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URI,
    title: "bookmark title",
  });

  info("Remove the 5 most recent visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 4),
    endDate: new Date(JS_NOW),
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

  info("URI should still exist in moz_places.");
  Assert.ok(page_in_database(TEST_URI.spec));

  info(
    "Run a history query and check that only the older 5 visits still exist."
  );
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 5);
  for (let i = 0; i < root.childCount; i++) {
    let visitTime = root.getChild(i).time;
    Assert.equal(visitTime, DB_NOW - i * 1000 - 5000);
  }
  root.containerOpen = false;

  Assert.ok(
    await PlacesUtils.history.hasVisits(TEST_URI),
    "visit should exist"
  );
  await PlacesTestUtils.promiseAsyncUpdates();

  info("Frecency should be positive.");
  Assert.ok(frecencyForUrl(TEST_URI) > 0);

  await cleanup();
});

add_task(async function remove_all_visits_unbookmarked_uri() {
  info("*** TEST: Remove all visits from an unbookmarked URI");

  info("Add some visits for the URI.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - i * 1000 });
  }
  await PlacesTestUtils.addVisits(visits);

  info("Remove all visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 10),
    endDate: new Date(JS_NOW),
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

  info("URI should no longer exist in moz_places.");
  Assert.ok(!page_in_database(TEST_URI.spec));

  info("Run a history query and check that no visits exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 0);
  root.containerOpen = false;

  Assert.equal(
    false,
    await PlacesUtils.history.hasVisits(TEST_URI),
    "visit should not exist"
  );

  await cleanup();
});

add_task(async function remove_all_visits_bookmarked_uri() {
  info("*** TEST: Remove all visits from a bookmarked URI");

  info("Add some visits for the URI.");
  let visits = [];
  for (let i = 0; i < 10; i++) {
    visits.push({ uri: TEST_URI, visitDate: DB_NOW - i * 1000 });
  }
  await PlacesTestUtils.addVisits(visits);
  info("Bookmark the URI.");
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URI,
    title: "bookmark title",
  });
  await PlacesTestUtils.promiseAsyncUpdates();
  let initialFrecency = frecencyForUrl(TEST_URI);

  info("Remove all visits.");
  let filter = {
    beginDate: new Date(JS_NOW - 10),
    endDate: new Date(JS_NOW),
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

  info("URI should still exist in moz_places.");
  Assert.ok(page_in_database(TEST_URI.spec));

  info("Run a history query and check that no visits exist.");
  let query = PlacesUtils.history.getNewQuery();
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  opts.sortingMode = opts.SORT_BY_DATE_DESCENDING;
  let root = PlacesUtils.history.executeQuery(query, opts).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 0);
  root.containerOpen = false;

  Assert.equal(
    false,
    await PlacesUtils.history.hasVisits(TEST_URI),
    "visit should not exist"
  );

  info("URI should be bookmarked");
  Assert.ok(await PlacesUtils.bookmarks.fetch({ url: TEST_URI }));
  await PlacesTestUtils.promiseAsyncUpdates();

  info("Frecency should be smaller.");
  Assert.ok(frecencyForUrl(TEST_URI) < initialFrecency);

  await cleanup();
});

add_task(async function remove_all_visits_bookmarked_uri() {
  info(
    "*** TEST: Remove some visits from a zero frecency URI retains zero frecency"
  );

  info("Add some visits for the URI.");
  await PlacesTestUtils.addVisits([
    {
      uri: TEST_URI,
      transition: TRANSITION_FRAMED_LINK,
      visitDate: DB_NOW - 86400000000000,
    },
    { uri: TEST_URI, transition: TRANSITION_FRAMED_LINK, visitDate: DB_NOW },
  ]);

  info("Remove newer visit.");
  let filter = {
    beginDate: new Date(JS_NOW - 10),
    endDate: new Date(JS_NOW),
  };
  await PlacesUtils.history.removeVisitsByFilter(filter);
  await PlacesTestUtils.promiseAsyncUpdates();

  info("URI should still exist in moz_places.");
  Assert.ok(page_in_database(TEST_URI.spec));
  info("Frecency should be zero.");
  Assert.equal(frecencyForUrl(TEST_URI), 0);

  await cleanup();
});
