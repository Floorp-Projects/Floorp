"use strict";

/* This test will ideally test the following cases
   (each with and without a callback associated with it)
   * Case A: Tests which should remove pages (Positives)
   *   Case A 1: Page has multiple visits both in/out of timeframe, all get deleted
   *   Case A 2: Page has single uri, removed by host
   *   Case A 3: Page has random subhost, with same host, removed by wildcard
   *   Case A 4: Page is localhost and localhost:port, removed by host
   *   Case A 5: Page is a `file://` type address, removed by empty host
   * Cases A 1,2,3 will be tried with and without bookmarks added (which prevent page deletion)
   * Case B: Tests in which no pages are removed (Inverses)
   *   Case B 1 (inverse): Page has no visits in timeframe, and nothing is deleted
   *   Case B 2: Page has single uri, not removed since hostname is different
   *   Case B 3: Page has multiple subhosts, not removed since wildcard doesn't match
   * Case C: Combinations tests
   *   Case C 1: Single hostname, multiple visits, at least one in timeframe and hostname
   *   Case C 2: Random subhosts, multiple visits, at least one in timeframe and hostname-wildcard
   */

add_task(function* test_removeByFilter() {
  // Cleanup
  yield PlacesTestUtils.clearHistory();
  yield PlacesUtils.bookmarks.eraseEverything();

  // Adding a witness URI
  let witnessURI = NetUtil.newURI("http://witnessmozilla.org/test_browserhistory/test_removeByFilter" + Math.random());
  yield PlacesTestUtils.addVisits(witnessURI);
  Assert.ok((yield PlacesTestUtils.isPageInDB(witnessURI)), "Witness URI is in database");

  let removeByFilterTester = Task.async(function*(visits, filter, checkBeforeRemove, checkAfterRemove, useCallback, bookmarkedUri) {
    // Add visits for URIs
    yield PlacesTestUtils.addVisits(visits);
    if (bookmarkedUri !== null && visits.map(v => v.uri).includes(bookmarkedUri)) {
      yield PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: bookmarkedUri,
        title: "test bookmark"
      });
    }
    checkBeforeRemove();

    // Take care of any observers (due to bookmarks)
    let { observer, promiseObserved } = getObserverPromise(bookmarkedUri);
    if (observer) {
      PlacesUtils.history.addObserver(observer, false);
    }
    // Perfom delete operation on database
    let removed = false;
    if (useCallback) {
      // The amount of callbacks will be the unique URIs to remove from the database
      let netCallbacksRequired = (new Set(visits.map(v => v.uri))).size;
      removed = yield PlacesUtils.history.removeByFilter(filter, pageInfo => {
        Assert.ok(PlacesUtils.validatePageInfo(pageInfo, false), "pageInfo should follow a basic format");
        Assert.ok(netCallbacksRequired > 0, "Callback called as many times as required");
        netCallbacksRequired--;
      });
    } else {
      removed = yield PlacesUtils.history.removeByFilter(filter);
    }
    checkAfterRemove();
    yield promiseObserved;
    if (observer) {
      PlacesUtils.history.removeObserver(observer);
      // Remove the added bookmarks as they interfere with following tests
      PlacesUtils.bookmarks.eraseEverything();
    }
      Assert.ok((yield PlacesTestUtils.isPageInDB(witnessURI)), "Witness URI is still in database");
    return removed;
  });

  const remoteUriList = [ "http://mozilla.org/test_browserhistory/test_removeByFilter/" + Math.random(),
                          "http://subdomain1.mozilla.org/test_browserhistory/test_removeByFilter/" + Math.random(),
                          "http://subdomain2.mozilla.org/test_browserhistory/test_removeByFilter/" + Math.random()
                        ];
  const localhostUriList = [ "http://localhost:4500/" + Math.random(), "http://localhost/" + Math.random() ];
  const fileUriList = [ "file:///home/user/files" + Math.random() ];
  const title = "Title " + Math.random();
  let sameHostVisits = [
    {
      uri: remoteUriList[0],
      title,
      visitDate: new Date(2005, 1, 1) * 1000
    },
    {
      uri: remoteUriList[0],
      title,
      visitDate: new Date(2005, 3, 3) * 1000
    },
    {
      uri: remoteUriList[0],
      title,
      visitDate: new Date(2007, 1, 1) * 1000
    }
  ];
  let randomHostVisits = [
    {
      uri: remoteUriList[0],
      title,
      visitDate: new Date(2005, 1, 1) * 1000
    },
    {
      uri: remoteUriList[1],
      title,
      visitDate: new Date(2005, 3, 3) * 1000
    },
    {
      uri: remoteUriList[2],
      title,
      visitDate: new Date(2007, 1, 1) * 1000
    }
  ];
  let localhostVisits = [
    {
      uri: localhostUriList[0],
      title
    },
    {
      uri: localhostUriList[1],
      title
    }
  ];
  let fileVisits = [
    {
      uri: fileUriList[0],
      title
    }
  ];
  let assertInDB = function*(aUri) {
      Assert.ok((yield PlacesTestUtils.isPageInDB(aUri)));
  };
  let assertNotInDB = function*(aUri) {
    Assert.ok(!(yield PlacesTestUtils.isPageInDB(aUri)));
  };
  for (let callbackUse of [true, false]) {
    // Case A Positives
    for (let bookmarkUse of [true, false]) {
      let bookmarkedUri = (arr) => undefined;
      let checkableArray = (arr) => arr;
      let checkClosure = assertNotInDB;
      if (bookmarkUse) {
        bookmarkedUri = (arr) => arr[0];
        checkableArray = (arr) => arr.slice(1);
        checkClosure = function(aUri) { };
      }
      // Case A 1: Dates
      yield removeByFilterTester(sameHostVisits,
                                 { beginDate: new Date(2004, 1, 1), endDate: new Date(2006, 1, 1) },
                                 () => assertInDB(remoteUriList[0]),
                                 () => checkClosure(remoteUriList[0]),
                                 callbackUse, bookmarkedUri(remoteUriList));
      // Case A 2: Single Sub-host
      yield removeByFilterTester(sameHostVisits, { host: "mozilla.org" },
                                 () => assertInDB(remoteUriList[0]),
                                 () => checkClosure(remoteUriList[0]),
                                 callbackUse, bookmarkedUri(remoteUriList));
      // Case A 3: Multiple subhost
      yield removeByFilterTester(randomHostVisits, { host: "*.mozilla.org" },
                                 () => remoteUriList.forEach(assertInDB),
                                 () => checkableArray(remoteUriList).forEach(checkClosure),
                                 callbackUse, bookmarkedUri(remoteUriList));
    }

    // Case A 4: Localhost
    yield removeByFilterTester(localhostVisits, { host: "localhost" },
                               () => localhostUriList.forEach(assertInDB),
                               () => localhostUriList.forEach(assertNotInDB),
                               callbackUse);
    // Case A 5: Local Files
    yield removeByFilterTester(fileVisits, { host: "" },
                               () => fileUriList.forEach(assertInDB),
                               () => fileUriList.forEach(assertNotInDB),
                               callbackUse);

    // Case B: Tests which do not remove anything (inverses)
    // Case B 1: Date
    yield removeByFilterTester(sameHostVisits,
                               { beginDate: new Date(2001, 1, 1), endDate: new Date(2002, 1, 1) },
                               () => assertInDB(remoteUriList[0]),
                               () => assertInDB(remoteUriList[0]),
                               callbackUse);
    // Case B 2 : Single subhost
    yield removeByFilterTester(sameHostVisits, { host: "notthere.org" },
                               () => assertInDB(remoteUriList[0]),
                               () => assertInDB(remoteUriList[0]),
                               callbackUse);
    // Case B 3 : Multiple subhosts
    yield removeByFilterTester(randomHostVisits, { host: "*.notthere.org" },
                               () => remoteUriList.forEach(assertInDB),
                               () => remoteUriList.forEach(assertInDB),
                               callbackUse);

    // Case C: Combination Cases
    // Case C 1: single subhost
    yield removeByFilterTester(sameHostVisits,
                               { host: "mozilla.org",
                                 beginDate: new Date(2004, 1, 1),
                                 endDate: new Date(2006, 1, 1) },
                               () => assertInDB(remoteUriList[0]),
                               () => assertNotInDB(remoteUriList[0]),
                               callbackUse);
    // Case C 2: multiple subhost
    yield removeByFilterTester(randomHostVisits,
                               { host: "*.mozilla.org",
                                 beginDate: new Date(2005, 1, 1),
                                 endDate: new Date(2017, 1, 1) },
                               () => remoteUriList.forEach(assertInDB),
                               () => remoteUriList.forEach(assertNotInDB),
                               callbackUse);
  }
});


// Test various error cases
add_task(function* test_error_cases() {
  Assert.throws(
    () => PlacesUtils.history.removeByFilter(),
      /TypeError: Expected a filter/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter("obviously, not a filter"),
      /TypeError: Expected a filter/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({}),
      /TypeError: Expected a non-empty filter/
  );
  Assert.throws(
    () => PlacesUtils.history.removeVisitsByFilter({beginDate: "now"}),
      /TypeError: Expected a Date/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({beginDate: Date.now()}),
      /TypeError: Expected a Date/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({beginDate: new Date()}, "obviously, not a callback"),
      /TypeError: Invalid function/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({beginDate: new Date(1000), endDate: new Date(0)}),
      /TypeError: `beginDate` should be at least as old/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({host: "#"}),
      /TypeError: Expected well formed hostname string for/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({host: "*.org"}),
      /TypeError: Expected well formed hostname string for/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({host: "www.*.org"}),
      /TypeError: Expected well formed hostname string for/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({host: {}}),
      /TypeError: `host` should be a string/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({host: ".mozilla.org"}),
      /TypeError: Expected well formed hostname string for/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({host: "*"}),
      /TypeError: Expected well formed hostname string for/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({host: "local.host."}),
      /TypeError: Expected well formed hostname string for/
  );
  Assert.throws(
    () => PlacesUtils.history.removeByFilter({host: "(local files)"}),
      /TypeError: Expected well formed hostname string for/
  );
});

// Helper functions

function getObserverPromise(bookmarkedUri) {
  if (!bookmarkedUri) {
    return { observer: null, promiseObserved: Promise.resolve() };
  }
  let observer;
  let promiseObserved = new Promise((resolve, reject) => {
    observer = {
      onBeginUpdateBatch() {},
      onEndUpdateBatch() {},
      onVisit(aUri) {
        reject(new Error("Unexpected call to onVisit"));
      },
      onTitleChanged(aUri) {
        reject(new Error("Unexpected call to onTitleChanged"));
      },
      onClearHistory() {
        reject(new Error("Unexpected call to onClearHistory"));
      },
      onPageChanged(aUri) {
        reject(new Error("Unexpected call to onPageChanged"));
      },
      onFrecencyChanged(aURI) {},
      onManyFrecenciesChanged() {},
      onDeleteURI(aURI) {
        try {
          Assert.notEqual(aURI.spec, bookmarkedUri, "Bookmarked URI should not be deleted");
        } finally {
          resolve();
        }
      },
      onDeleteVisits(aURI, aVisitTime) {
        try {
          Assert.equal(aVisitTime, PlacesUtils.toPRTime(new Date(0)), "Observing onDeleteVisits deletes all visits");
          Assert.equal(aURI.spec, bookmarkedUri, "Bookmarked URI should have all visits removed but not the page itself");
        } finally {
          resolve();
        }
      }
    };
  });
  return { observer, promiseObserved };
}
