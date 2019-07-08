/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

// Tests for `History.removeVisitsByFilter`, as implemented in History.jsm

"use strict";

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);

add_task(async function test_removeVisitsByFilter() {
  let referenceDate = new Date(1999, 9, 9, 9, 9);

  // Populate a database with 20 entries, remove a subset of entries,
  // ensure consistency.
  let remover = async function(options) {
    info("Remover with options " + JSON.stringify(options));
    let SAMPLE_SIZE = options.sampleSize;

    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();

    // Populate the database.
    // Create `SAMPLE_SIZE` visits, from the oldest to the newest.

    let bookmarkIndices = new Set(options.bookmarks);
    let visits = [];
    let frecencyChangePromises = new Map();
    let uriDeletePromises = new Map();
    let getURL = options.url
      ? i =>
          "http://mozilla.com/test_browserhistory/test_removeVisitsByFilter/removeme/byurl/" +
          Math.floor(i / (SAMPLE_SIZE / 5)) +
          "/"
      : i =>
          "http://mozilla.com/test_browserhistory/test_removeVisitsByFilter/removeme/" +
          i +
          "/" +
          Math.random();
    for (let i = 0; i < SAMPLE_SIZE; ++i) {
      let spec = getURL(i);
      let uri = NetUtil.newURI(spec);
      let jsDate = new Date(Number(referenceDate) + 3600 * 1000 * i);
      let dbDate = jsDate * 1000;
      let hasBookmark = bookmarkIndices.has(i);
      let hasOwnBookmark = hasBookmark;
      if (!hasOwnBookmark && options.url) {
        // Also mark as bookmarked if one of the earlier bookmarked items has the same URL.
        hasBookmark = options.bookmarks
          .filter(n => n < i)
          .some(n => visits[n].uri.spec == spec && visits[n].test.hasBookmark);
      }
      info("Generating " + uri.spec + ", " + dbDate);
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
        info("Adding a bookmark to visit " + i);
        await PlacesUtils.bookmarks.insert({
          url: uri,
          parentGuid: PlacesUtils.bookmarks.unfiledGuid,
          title: "test bookmark",
        });
        info("Bookmark added");
      }
    }

    info("Adding visits");
    await PlacesTestUtils.addVisits(visits);

    info("Preparing filters");
    let filter = {};
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
      endIndex = Math.min(
        endIndex,
        removedItems.findIndex((v, index) => v.uri.spec != rawURL) - 1
      );
    }
    removedItems.splice(endIndex + 1);
    let remainingItems = visits.filter(v => !removedItems.includes(v));
    for (let i = 0; i < removedItems.length; i++) {
      let test = removedItems[i].test;
      info("Marking visit " + (beginIndex + i) + " as expecting removal");
      test.toRemove = true;
      if (
        test.hasBookmark ||
        (options.url &&
          remainingItems.some(v => v.uri.spec == removedItems[i].uri.spec))
      ) {
        frecencyChangePromises.set(
          removedItems[i].uri.spec,
          PromiseUtils.defer()
        );
      } else if (!options.url || i == 0) {
        uriDeletePromises.set(removedItems[i].uri.spec, PromiseUtils.defer());
      }
    }

    let observer = {
      deferred: PromiseUtils.defer(),
      onBeginUpdateBatch() {},
      onEndUpdateBatch() {},
      onTitleChanged(uri) {
        this.deferred.reject(
          new Error("Unexpected call to onTitleChanged " + uri.spec)
        );
      },
      onClearHistory() {
        this.deferred.reject("Unexpected call to onClearHistory");
      },
      onPageChanged(uri) {
        this.deferred.reject(
          new Error("Unexpected call to onPageChanged " + uri.spec)
        );
      },
      onFrecencyChanged(aURI) {
        info("onFrecencyChanged " + aURI.spec);
        let deferred = frecencyChangePromises.get(aURI.spec);
        Assert.ok(!!deferred, "Observing onFrecencyChanged");
        deferred.resolve();
      },
      onManyFrecenciesChanged() {
        info("Many frecencies changed");
        for (let [, deferred] of frecencyChangePromises) {
          deferred.resolve();
        }
      },
      onDeleteURI(aURI) {
        info("onDeleteURI " + aURI.spec);
        let deferred = uriDeletePromises.get(aURI.spec);
        Assert.ok(!!deferred, "Observing onDeleteURI");
        deferred.resolve();
      },
      onDeleteVisits(aURI) {
        // Not sure we can test anything.
      },
    };
    PlacesUtils.history.addObserver(observer);

    let cbarg;
    if (options.useCallback) {
      info("Setting up callback");
      cbarg = [
        info => {
          for (let visit of visits) {
            info("Comparing " + info.date + " and " + visit.test.jsDate);
            if (Math.abs(visit.test.jsDate - info.date) < 100) {
              // Assume rounding errors
              Assert.ok(
                !visit.test.announcedByOnRow,
                "This is the first time we announce the removal of this visit"
              );
              Assert.ok(
                visit.test.toRemove,
                "This is a visit we intended to remove"
              );
              visit.test.announcedByOnRow = true;
              return;
            }
          }
          Assert.ok(false, "Could not find the visit we attempt to remove");
        },
      ];
    } else {
      info("No callback");
      cbarg = [];
    }
    let result = await PlacesUtils.history.removeVisitsByFilter(
      filter,
      ...cbarg
    );

    Assert.ok(result, "Removal succeeded");

    // Make sure that we have eliminated exactly the entries we expected
    // to eliminate.
    for (let i = 0; i < visits.length; ++i) {
      let visit = visits[i];
      info("Controlling the results on visit " + i);
      let remainingVisitsForURI = remainingItems.filter(
        v => visit.uri.spec == v.uri.spec
      ).length;
      Assert.equal(
        visits_in_database(visit.uri),
        remainingVisitsForURI,
        "Visit is still present iff expected"
      );
      if (options.useCallback) {
        Assert.equal(
          visit.test.toRemove,
          visit.test.announcedByOnRow,
          "Visit removal has been announced by onResult iff expected"
        );
      }
      if (visit.test.hasBookmark || remainingVisitsForURI) {
        Assert.notEqual(
          page_in_database(visit.uri),
          0,
          "The page should still appear in the db"
        );
      } else {
        Assert.equal(
          page_in_database(visit.uri),
          0,
          "The page should have been removed from the db"
        );
      }
    }

    // Make sure that the observer has been called wherever applicable.
    info("Checking URI delete promises.");
    await Promise.all(Array.from(uriDeletePromises.values()));
    info("Checking frecency change promises.");
    await Promise.all(Array.from(frecencyChangePromises.values()));
    PlacesUtils.history.removeObserver(observer);
  };

  let size = 20;
  for (let range of [
    { begin: 0 },
    { end: 19 },
    { begin: 0, end: 10 },
    { begin: 3, end: 4 },
    { begin: 5, end: 8, limit: 2 },
    { begin: 10, end: 18, limit: 5 },
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
      await remover(options);
      options.url = 1;
      await remover(options);
      options.url = 2;
      await remover(options);
      options.url = 3;
      await remover(options);
    }
  }
  await PlacesUtils.history.clear();
});

// Test the various error cases
add_task(async function test_error_cases() {
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
    () => PlacesUtils.history.removeVisitsByFilter({ beginDate: "now" }),
    /TypeError: Expected a Date/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({ beginDate: Date.now() }),
    /TypeError: Expected a Date/
  );
  Assert.throws(
    () =>
      PlacesUtils.history.removeVisitsByFilter(
        { beginDate: new Date() },
        "obviously, not a callback"
      ),
    /TypeError: Invalid function/
  );
  Assert.throws(
    () =>
      PlacesUtils.history.removeVisitsByFilter({
        beginDate: new Date(1000),
        endDate: new Date(0),
      }),
    /TypeError: `beginDate` should be at least as old/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({ limit: {} }),
    /Expected a non-zero positive integer as a limit/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({ limit: -1 }),
    /Expected a non-zero positive integer as a limit/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({ limit: 0.1 }),
    /Expected a non-zero positive integer as a limit/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({ limit: Infinity }),
    /Expected a non-zero positive integer as a limit/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({ url: {} }),
    /Expected a valid URL for `url`/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({ url: 0 }),
    /Expected a valid URL for `url`/
  );
  Assert.throws(
    () =>
      PlacesUtils.history.removeVisitsByFilter({
        beginDate: new Date(1000),
        endDate: new Date(0),
      }),
    /TypeError: `beginDate` should be at least as old/
  );
  Assert.throws(
    () =>
      PlacesUtils.history.removeVisitsByFilter({
        beginDate: new Date(1000),
        endDate: new Date(0),
      }),
    /TypeError: `beginDate` should be at least as old/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({ transition: -1 }),
    /TypeError: `transition` should be valid/
  );
});

add_task(async function test_orphans() {
  let uri = NetUtil.newURI("http://moz.org/");
  await PlacesTestUtils.addVisits({ uri });

  PlacesUtils.favicons.setAndFetchFaviconForPage(
    uri,
    SMALLPNG_DATA_URI,
    true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    Services.scriptSecurityManager.getSystemPrincipal()
  );
  await PlacesUtils.history.update({
    url: uri,
    annotations: new Map([["test", "restval"]]),
  });

  await PlacesUtils.history.removeVisitsByFilter({
    beginDate: new Date(1999, 9, 9, 9, 9),
    endDate: new Date(),
  });
  Assert.ok(
    !(await PlacesTestUtils.isPageInDB(uri)),
    "Page should have been removed"
  );
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`SELECT (SELECT count(*) FROM moz_annos) +
                                      (SELECT count(*) FROM moz_icons) +
                                      (SELECT count(*) FROM moz_pages_w_icons) +
                                      (SELECT count(*) FROM moz_icons_to_pages) AS count`);
  Assert.equal(rows[0].getResultByName("count"), 0, "Should not find orphans");
});
