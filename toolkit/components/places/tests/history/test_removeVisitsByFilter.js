/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

// Tests for `History.removeVisitsByFilter`, as implemented in History.jsm

"use strict";

Cu.importGlobalProperties(["URL"]);

Cu.import("resource://gre/modules/PromiseUtils.jsm", this);

add_task(function* test_removeVisitsByFilter() {
  let referenceDate = new Date(1999, 9, 9, 9, 9);

  /**
   * Populate a database with 20 entries, remove a subset of entries,
   * ensure consistency.
   */
  let remover = Task.async(function*(options) {
    do_print("Remover with options " + JSON.stringify(options));
    let SAMPLE_SIZE = options.sampleSize;

    yield PlacesTestUtils.clearHistory();
    yield PlacesUtils.bookmarks.eraseEverything();

    // Populate the database.
    // Create `SAMPLE_SIZE` visits, from the oldest to the newest.

    let bookmarks = new Set(options.bookmarks);
    let visits = [];
    let visitByURI = new Map();
    for (let i = 0; i < SAMPLE_SIZE; ++i) {
      let spec = "http://mozilla.com/test_browserhistory/test_removeVisitsByFilter/removeme/" + i + "/" + Math.random();
      let uri = NetUtil.newURI(spec);
      let jsDate = new Date(Number(referenceDate) + 3600 * 1000 * i);
      let dbDate = jsDate * 1000;
      let hasBookmark = bookmarks.has(i);
      do_print("Generating " + uri.spec + ", " + dbDate);
      let visit = {
        uri,
        title: "visit " + i,
        visitDate: dbDate,
        test: {
          // `visitDate`, as a Date
          jsDate: jsDate,
          // `true` if we expect that the visit will be removed
          toRemove: false,
          // `true` if `onRow` informed of the removal of this visit
          announcedByOnRow: false,
          // `true` if there is a bookmark for this URI, i.e. of the page
          // should not be entirely removed.
          hasBookmark: hasBookmark,
          onFrecencyChanged: null,
          onDeleteURI: null,
        },
      };
      visits.push(visit);
      visitByURI.set(spec, visit);
      if (hasBookmark) {
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
    for (let i = beginIndex; i <= endIndex; ++i) {
      let test = visits[i].test;
      do_print("Marking visit " + i + " as expecting removal");
      test.toRemove = true;
      if (test.hasBookmark) {
        test.onFrecencyChanged = PromiseUtils.defer();
      } else {
        test.onDeleteURI = PromiseUtils.defer();
      }
    }

    let observer = {
      deferred: PromiseUtils.defer(),
      onBeginUpdateBatch: function() {},
      onEndUpdateBatch: function() {},
      onVisit: function(uri) {
        this.deferred.reject(new Error("Unexpected call to onVisit " + uri.spec));
      },
      onTitleChanged: function(uri) {
        this.deferred.reject(new Error("Unexpected call to onTitleChanged " + uri.spec));
      },
      onClearHistory: function() {
        this.deferred.reject("Unexpected call to onClearHistory");
      },
      onPageChanged: function(uri) {
        this.deferred.reject(new Error("Unexpected call to onPageChanged " + uri.spec));
      },
      onFrecencyChanged: function(aURI) {
        do_print("onFrecencyChanged " + aURI.spec);
        let visit = visitByURI.get(aURI.spec);
        Assert.ok(!!visit.test.onFrecencyChanged, "Observing onFrecencyChanged");
        visit.test.onFrecencyChanged.resolve();
      },
      onManyFrecenciesChanged: function() {
        do_print("Many frecencies changed");
        for (let visit of visits) {
          if (visit.onFrecencyChanged) {
            visit.onFrecencyChanged.resolve();
          }
        }
      },
      onDeleteURI: function(aURI) {
        do_print("onDeleteURI " + aURI.spec);
        let visit = visitByURI.get(aURI.spec);
        Assert.ok(!!visit.test.onDeleteURI, "Observing onDeleteURI");
        visit.test.onDeleteURI.resolve();
      },
      onDeleteVisits: function(aURI) {
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
      Assert.equal(
        visits_in_database(visit.uri) == 0,
        visit.test.toRemove,
        "Visit is still present iff expected");
      if (options.useCallback) {
        Assert.equal(
          visit.test.toRemove,
          visit.test.announcedByOnRow,
          "Visit removal has been announced by onResult iff expected");
      }
      if (visit.test.hasBookmark || !visit.test.toRemove) {
        Assert.notEqual(page_in_database(visit.uri), 0, "The page is should still appear in the db");
      } else {
        Assert.equal(page_in_database(visit.uri), 0, "The page should have been removed from the db");
      }
    }

    // Make sure that the observer has been called wherever applicable.
    for (let visit of visits) {
      do_print("Making sure that the observer has been called for " + visit.uri.spec);
      let test = visit.test;
      do_print("Checking onFrecencyChanged");
      if (test.onFrecencyChanged) {
        yield test.onFrecencyChanged.promise;
      }
      do_print("Checking onDeleteURI");
      if (test.onDeleteURI) {
        yield test.onDeleteURI.promise;
      }
    }
    PlacesUtils.history.removeObserver(observer);
  });

  let size = 20;
  for (let range of [
    {begin: 0},
    {end: 19},
    {begin: 0, end: 10},
    {begin: 3, end: 4},
  ]) {
    for (let bookmarks of [[], [5, 6]]) {
      let options = {
        sampleSize: size,
        bookmarks: bookmarks,
      };
      if ("begin" in range) {
        options.begin = range.begin;
      }
      if ("end" in range) {
        options.end = range.end;
      }
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
});


function run_test() {
  run_next_test();
}
