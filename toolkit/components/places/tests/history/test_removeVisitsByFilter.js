/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

// Tests for `History.removeVisitsByFilter`, as implemented in History.jsm

"use strict";

Cu.importGlobalProperties(["URL"]);

Cu.import("resource://gre/modules/PromiseUtils.jsm", this);

add_task(function* test_removeVisitsByFilter() {
  let referenceDate = new Date(1999, 9, 9, 9, 9);

  // Populate a database with 20 entries, remove a subset of entries,
  // ensure consistency.
  let remover = Task.async(function*(options) {
    do_print("Remover with options " + JSON.stringify(options));
    let SAMPLE_SIZE = options.sampleSize;

    yield PlacesTestUtils.clearHistory();
    yield PlacesUtils.bookmarks.eraseEverything();

    // Populate the database.
    // Create `SAMPLE_SIZE` visits, from the oldest to the newest.

    let bookmarkIndices = new Set(options.bookmarks);
    let visits = [];
    let frecencyChangePromises = new Map();
    let uriDeletePromises = new Map();
    let getURL = options.url ?
      i => "http://mozilla.com/test_browserhistory/test_removeVisitsByFilter/removeme/byurl/" + Math.floor(i / (SAMPLE_SIZE / 5)) + "/" :
      i => "http://mozilla.com/test_browserhistory/test_removeVisitsByFilter/removeme/" + i + "/" + Math.random();
    for (let i = 0; i < SAMPLE_SIZE; ++i) {
      let spec = getURL(i);
      let uri = NetUtil.newURI(spec);
      let jsDate = new Date(Number(referenceDate) + 3600 * 1000 * i);
      let dbDate = jsDate * 1000;
      let hasBookmark = bookmarkIndices.has(i);
      let hasOwnBookmark = hasBookmark;
      if (!hasOwnBookmark && options.url) {
        // Also mark as bookmarked if one of the earlier bookmarked items has the same URL.
        hasBookmark =
          options.bookmarks.filter(n => n < i).some(n => visits[n].uri.spec == spec && visits[n].test.hasBookmark);
      }
      do_print("Generating " + uri.spec + ", " + dbDate);
      let visit = {
        uri,
        title: "visit " + i,
        visitDate: dbDate,
        test: {
          // `visitDate`, as a Date
          jsDate,
          // `true` if we expect that the visit will be removed
          toRemove: false,
          // `true` if `onRow` informed of the removal of this visit
          announcedByOnRow: false,
          // `true` if there is a bookmark for this URI, i.e. of the page
          // should not be entirely removed.
          hasBookmark,
          onFrecencyChanged: null,
          onDeleteURI: null,
        },
      };
      visits.push(visit);
      if (hasOwnBookmark) {
        do_print("Adding a bookmark to visit " + i);
        yield PlacesUtils.bookmarks.insert({
          url: uri,
          parentGuid: PlacesUtils.bookmarks.unfiledGuid,
          title: "test bookmark"
        });
        do_print("Bookmark added");
      }
    }

    do_print("Adding visits");
    yield PlacesTestUtils.addVisits(visits);

    do_print("Preparing filters");
    let filter = {
    };
    let beginIndex = 0;
    let endIndex = visits.length - 1;
    if ("begin" in options) {
      let ms = Number(visits[options.begin].test.jsDate) - 1000;
      filter.beginDate = new Date(ms);
      beginIndex = options.begin;
    }
    if ("end" in options) {
      let ms = Number(visits[options.end].test.jsDate) + 1000;
      filter.endDate = new Date(ms);
      endIndex = options.end;
    }
    if ("limit" in options) {
      endIndex = beginIndex + options.limit - 1; // -1 because the start index is inclusive.
      filter.limit = options.limit;
    }
    let removedItems = visits.slice(beginIndex);
    endIndex -= beginIndex;
    if (options.url) {
      let rawURL = "";
      switch (options.url) {
        case 1:
          filter.url = new URL(removedItems[0].uri.spec);
          rawURL = filter.url.href;
          break;
        case 2:
          filter.url = removedItems[0].uri;
          rawURL = filter.url.spec;
          break;
        case 3:
          filter.url = removedItems[0].uri.spec;
          rawURL = filter.url;
          break;
      }
      endIndex = Math.min(endIndex, removedItems.findIndex((v, index) => v.uri.spec != rawURL) - 1);
    }
    removedItems.splice(endIndex + 1);
    let remainingItems = visits.filter(v => !removedItems.includes(v));
    for (let i = 0; i < removedItems.length; i++) {
      let test = removedItems[i].test;
      do_print("Marking visit " + (beginIndex + i) + " as expecting removal");
      test.toRemove = true;
      if (test.hasBookmark ||
          (options.url && remainingItems.some(v => v.uri.spec == removedItems[i].uri.spec))) {
        frecencyChangePromises.set(removedItems[i].uri.spec, PromiseUtils.defer());
      } else if (!options.url || i == 0) {
        uriDeletePromises.set(removedItems[i].uri.spec, PromiseUtils.defer());
      }
    }

    let observer = {
      deferred: PromiseUtils.defer(),
      onBeginUpdateBatch() {},
      onEndUpdateBatch() {},
      onVisit(uri) {
        this.deferred.reject(new Error("Unexpected call to onVisit " + uri.spec));
      },
      onTitleChanged(uri) {
        this.deferred.reject(new Error("Unexpected call to onTitleChanged " + uri.spec));
      },
      onClearHistory() {
        this.deferred.reject("Unexpected call to onClearHistory");
      },
      onPageChanged(uri) {
        this.deferred.reject(new Error("Unexpected call to onPageChanged " + uri.spec));
      },
      onFrecencyChanged(aURI) {
        do_print("onFrecencyChanged " + aURI.spec);
        let deferred = frecencyChangePromises.get(aURI.spec);
        Assert.ok(!!deferred, "Observing onFrecencyChanged");
        deferred.resolve();
      },
      onManyFrecenciesChanged() {
        do_print("Many frecencies changed");
        for (let [, deferred] of frecencyChangePromises) {
          deferred.resolve();
        }
      },
      onDeleteURI(aURI) {
        do_print("onDeleteURI " + aURI.spec);
        let deferred = uriDeletePromises.get(aURI.spec);
        Assert.ok(!!deferred, "Observing onDeleteURI");
        deferred.resolve();
      },
      onDeleteVisits(aURI) {
        // Not sure we can test anything.
      }
    };
    PlacesUtils.history.addObserver(observer, false);

    let cbarg;
    if (options.useCallback) {
      do_print("Setting up callback");
      cbarg = [info => {
        for (let visit of visits) {
          do_print("Comparing " + info.date + " and " + visit.test.jsDate);
          if (Math.abs(visit.test.jsDate - info.date) < 100) { // Assume rounding errors
            Assert.ok(!visit.test.announcedByOnRow,
              "This is the first time we announce the removal of this visit");
            Assert.ok(visit.test.toRemove,
              "This is a visit we intended to remove");
            visit.test.announcedByOnRow = true;
            return;
          }
        }
        Assert.ok(false, "Could not find the visit we attempt to remove");
      }];
    } else {
      do_print("No callback");
      cbarg = [];
    }
    let result = yield PlacesUtils.history.removeVisitsByFilter(filter, ...cbarg);

    Assert.ok(result, "Removal succeeded");

    // Make sure that we have eliminated exactly the entries we expected
    // to eliminate.
    for (let i = 0; i < visits.length; ++i) {
      let visit = visits[i];
      do_print("Controlling the results on visit " + i);
      let remainingVisitsForURI = remainingItems.filter(v => visit.uri.spec == v.uri.spec).length;
      Assert.equal(
        visits_in_database(visit.uri),
        remainingVisitsForURI,
        "Visit is still present iff expected");
      if (options.useCallback) {
        Assert.equal(
          visit.test.toRemove,
          visit.test.announcedByOnRow,
          "Visit removal has been announced by onResult iff expected");
      }
      if (visit.test.hasBookmark || remainingVisitsForURI) {
        Assert.notEqual(page_in_database(visit.uri), 0, "The page should still appear in the db");
      } else {
        Assert.equal(page_in_database(visit.uri), 0, "The page should have been removed from the db");
      }
    }

    // Make sure that the observer has been called wherever applicable.
    do_print("Checking URI delete promises.");
    yield Promise.all(Array.from(uriDeletePromises.values()));
    do_print("Checking frecency change promises.");
    yield Promise.all(Array.from(frecencyChangePromises.values()));
    PlacesUtils.history.removeObserver(observer);
  });

  let size = 20;
  for (let range of [
    {begin: 0},
    {end: 19},
    {begin: 0, end: 10},
    {begin: 3, end: 4},
    {begin: 5, end: 8, limit: 2},
    {begin: 10, end: 18, limit: 5},
  ]) {
    for (let bookmarks of [[], [5, 6]]) {
      let options = {
        sampleSize: size,
        bookmarks,
      };
      if ("begin" in range) {
        options.begin = range.begin;
      }
      if ("end" in range) {
        options.end = range.end;
      }
      if ("limit" in range) {
        options.limit = range.limit;
      }
      yield remover(options);
      options.url = 1;
      yield remover(options);
      options.url = 2;
      yield remover(options);
      options.url = 3;
      yield remover(options);
    }
  }
  yield PlacesTestUtils.clearHistory();
});

// Test the various error cases
add_task(function* test_error_cases() {
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter(),
    /TypeError: Expected a filter/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter("obviously, not a filter"),
    /TypeError: Expected a filter/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({}),
    /TypeError: Expected a non-empty filter/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({beginDate: "now"}),
    /TypeError: Expected a Date/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({beginDate: Date.now()}),
    /TypeError: Expected a Date/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({beginDate: new Date()}, "obviously, not a callback"),
    /TypeError: Invalid function/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({beginDate: new Date(1000), endDate: new Date(0)}),
    /TypeError: `beginDate` should be at least as old/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({limit: {}}),
    /Expected a non-zero positive integer as a limit/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({limit: -1}),
    /Expected a non-zero positive integer as a limit/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({limit: 0.1}),
    /Expected a non-zero positive integer as a limit/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({limit: Infinity}),
    /Expected a non-zero positive integer as a limit/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({url: {}}),
    /Expected a valid URL for `url`/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({url: 0}),
    /Expected a valid URL for `url`/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({beginDate: new Date(1000), endDate: new Date(0)}),
    /TypeError: `beginDate` should be at least as old/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({beginDate: new Date(1000), endDate: new Date(0)}),
    /TypeError: `beginDate` should be at least as old/
  );
});

add_task(function* test_orphans() {
  let uri = NetUtil.newURI("http://moz.org/");
  yield PlacesTestUtils.addVisits({ uri });

  PlacesUtils.favicons.setAndFetchFaviconForPage(
    uri, SMALLPNG_DATA_URI, true,  PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null, Services.scriptSecurityManager.getSystemPrincipal());
  PlacesUtils.annotations.setPageAnnotation(uri, "test", "restval", 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);

  yield PlacesUtils.history.removeVisitsByFilter({ beginDate: new Date(1999, 9, 9, 9, 9),
                                                   endDate: new Date() });
  Assert.ok(!(yield PlacesTestUtils.isPageInDB(uri)), "Page should have been removed");
  let db = yield PlacesUtils.promiseDBConnection();
  let rows = yield db.execute(`SELECT (SELECT count(*) FROM moz_annos) +
                                      (SELECT count(*) FROM moz_favicons) AS count`);
  Assert.equal(rows[0].getResultByName("count"), 0, "Should not find orphans");
});
