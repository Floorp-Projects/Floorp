/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

// Tests for `History.remove`, as implemented in History.jsm

"use strict";

Cu.importGlobalProperties(["URL"]);


// Test removing a single page
add_task(function* test_remove_single() {
  yield PlacesTestUtils.clearHistory();
  yield PlacesUtils.bookmarks.eraseEverything();


  let WITNESS_URI = NetUtil.newURI("http://mozilla.com/test_browserhistory/test_remove/" + Math.random());
  yield PlacesTestUtils.addVisits(WITNESS_URI);
  Assert.ok(page_in_database(WITNESS_URI));

  let remover = Task.async(function*(name, filter, options) {
    do_print(name);
    do_print(JSON.stringify(options));
    do_print("Setting up visit");

    let uri = NetUtil.newURI("http://mozilla.com/test_browserhistory/test_remove/" + Math.random());
    let title = "Visit " + Math.random();
    yield PlacesTestUtils.addVisits({uri: uri, title: title});
    Assert.ok(visits_in_database(uri), "History entry created");

    let removeArg = yield filter(uri);

    if (options.addBookmark) {
      PlacesUtils.bookmarks.insertBookmark(
        PlacesUtils.unfiledBookmarksFolderId,
        uri,
        PlacesUtils.bookmarks.DEFAULT_INDEX,
        "test bookmark");
    }

    let shouldRemove = !options.addBookmark;
    let observer;
    let promiseObserved = new Promise((resolve, reject) => {
      observer = {
        onBeginUpdateBatch: function() {},
        onEndUpdateBatch: function() {},
        onVisit: function(uri) {
          reject(new Error("Unexpected call to onVisit " + uri.spec));
        },
        onTitleChanged: function(uri) {
          reject(new Error("Unexpected call to onTitleChanged " + uri.spec));
        },
        onClearHistory: function() {
          reject("Unexpected call to onClearHistory");
        },
        onPageChanged: function(uri) {
          reject(new Error("Unexpected call to onPageChanged " + uri.spec));
        },
        onFrecencyChanged: function(aURI) {
          try {
            Assert.ok(!shouldRemove, "Observing onFrecencyChanged");
            Assert.equal(aURI.spec, uri.spec, "Observing effect on the right uri");
          } finally {
            resolve();
          }
        },
        onManyFrecenciesChanged: function() {
          try {
            Assert.ok(!shouldRemove, "Observing onManyFrecenciesChanged");
          } finally {
            resolve();
          }
        },
        onDeleteURI: function(aURI) {
          try {
            Assert.ok(shouldRemove, "Observing onDeleteURI");
            Assert.equal(aURI.spec, uri.spec, "Observing effect on the right uri");
          } finally {
            resolve();
          }
        },
        onDeleteVisits: function(aURI) {
          Assert.equal(aURI.spec, uri.spec, "Observing onDeleteVisits on the right uri");
        }
      };
    });
    PlacesUtils.history.addObserver(observer, false);

    do_print("Performing removal");
    let removed = false;
    if (options.useCallback) {
      let onRowCalled = false;
      let guid = do_get_guid_for_uri(uri);
      removed = yield PlacesUtils.history.remove(removeArg, page => {
        Assert.equal(onRowCalled, false, "Callback has not been called yet");
        onRowCalled = true;
        Assert.equal(page.url.href, uri.spec, "Callback provides the correct url");
        Assert.equal(page.guid, guid, "Callback provides the correct guid");
        Assert.equal(page.title, title, "Callback provides the correct title");
      });
      Assert.ok(onRowCalled, "Callback has been called");
    } else {
      removed = yield PlacesUtils.history.remove(removeArg);
    }

    yield promiseObserved;
    PlacesUtils.history.removeObserver(observer);

    Assert.equal(visits_in_database(uri), 0, "History entry has disappeared");
    Assert.notEqual(visits_in_database(WITNESS_URI), 0, "Witness URI still has visits");
    Assert.notEqual(page_in_database(WITNESS_URI), 0, "Witness URI is still here");
    if (shouldRemove) {
      Assert.ok(removed, "Something was removed");
      Assert.equal(page_in_database(uri), 0, "Page has disappeared");
    } else {
      Assert.ok(!removed, "The page was not removed, as there was a bookmark");
      Assert.notEqual(page_in_database(uri), 0, "The page is still present");
    }
  });

  try {
    for (let useCallback of [false, true]) {
      for (let addBookmark of [false, true]) {
        let options = { useCallback: useCallback, addBookmark: addBookmark };
        yield remover("Testing History.remove() with a single URI", x => x, options);
        yield remover("Testing History.remove() with a single string url", x => x.spec, options);
        yield remover("Testing History.remove() with a single string guid", x => do_get_guid_for_uri(x), options);
        yield remover("Testing History.remove() with a single URI in an array", x => [x], options);
        yield remover("Testing History.remove() with a single string url in an array", x => [x.spec], options);
        yield remover("Testing History.remove() with a single string guid in an array", x => [do_get_guid_for_uri(x)], options);
      }
    }
  } finally {
    yield PlacesTestUtils.clearHistory();
  }
  return;
});

// Test removing a list of pages
add_task(function* test_remove_many() {
  const SIZE = 10;

  yield PlacesTestUtils.clearHistory();
  yield PlacesUtils.bookmarks.eraseEverything();

  do_print("Adding a witness page");
  let WITNESS_URI = NetUtil.newURI("http://mozilla.com/test_browserhistory/test_remove/" + Math.random());;
  yield PlacesTestUtils.addVisits(WITNESS_URI);
  Assert.ok(page_in_database(WITNESS_URI), "Witness page added");

  do_print("Generating samples");
  let pages = [];
  for (let i = 0; i < SIZE; ++i) {
    let uri = NetUtil.newURI("http://mozilla.com/test_browserhistory/test_remove?sample=" + i + "&salt=" + Math.random());
    let title = "Visit " + i + ", " + Math.random();
    let hasBookmark = i % 3 == 0;
    let resolve;
    let page = {
      uri: uri,
      title: title,
      hasBookmark: hasBookmark,
      // `true` once `onResult` has been called for this page
      onResultCalled: false,
      // `true` once `onDeleteVisits` has been called for this page
      onDeleteVisitsCalled: false,
      // `true` once `onFrecencyChangedCalled` has been called for this page
      onFrecencyChangedCalled: false,
      // `true` once `onDeleteURI` has been called for this page
      onDeleteURICalled: false,
    };
    do_print("Pushing: " + uri.spec);
    pages.push(page);

    yield PlacesTestUtils.addVisits(page);
    page.guid = do_get_guid_for_uri(uri);
    if (hasBookmark) {
      PlacesUtils.bookmarks.insertBookmark(
        PlacesUtils.unfiledBookmarksFolderId,
        uri,
        PlacesUtils.bookmarks.DEFAULT_INDEX,
        "test bookmark " + i);
    }
    Assert.ok(page_in_database(uri), "Page added");
  }

  do_print("Mixing key types and introducing dangling keys");
  let keys = [];
  for (let i = 0; i < SIZE; ++i) {
    if (i % 4 == 0) {
      keys.push(pages[i].uri);
      keys.push(NetUtil.newURI("http://example.org/dangling/nsIURI/" + i));
    } else if (i % 4 == 1) {
      keys.push(new URL(pages[i].uri.spec));
      keys.push(new URL("http://example.org/dangling/URL/" + i));
    } else if (i % 4 == 2) {
      keys.push(pages[i].uri.spec);
      keys.push("http://example.org/dangling/stringuri/" + i);
    } else {
      keys.push(pages[i].guid);
      keys.push(("guid_" + i + "_01234567890").substr(0, 12));
    }
  }

  let observer = {
    onBeginUpdateBatch: function() {},
    onEndUpdateBatch: function() {},
    onVisit: function(aURI) {
      Assert.ok(false, "Unexpected call to onVisit " + aURI.spec);
    },
    onTitleChanged: function(aURI) {
      Assert.ok(false, "Unexpected call to onTitleChanged " + aURI.spec);
    },
    onClearHistory: function() {
      Assert.ok(false, "Unexpected call to onClearHistory");
    },
    onPageChanged: function(aURI) {
      Assert.ok(false, "Unexpected call to onPageChanged " + aURI.spec);
    },
    onFrecencyChanged: function(aURI) {
      let origin = pages.find(x =>  x.uri.spec == aURI.spec);
      Assert.ok(origin);
      Assert.ok(origin.hasBookmark, "Observing onFrecencyChanged on a page with a bookmark");
      origin.onFrecencyChangedCalled = true;
      // We do not make sure that `origin.onFrecencyChangedCalled` is `false`, as
    },
    onManyFrecenciesChanged: function() {
      Assert.ok(false, "Observing onManyFrecenciesChanges, this is most likely correct but not covered by this test");
    },
    onDeleteURI: function(aURI) {
      let origin = pages.find(x => x.uri.spec == aURI.spec);
      Assert.ok(origin);
      Assert.ok(!origin.hasBookmark, "Observing onDeleteURI on a page without a bookmark");
      Assert.ok(!origin.onDeleteURICalled, "Observing onDeleteURI for the first time");
      origin.onDeleteURICalled = true;
    },
    onDeleteVisits: function(aURI) {
      let origin = pages.find(x => x.uri.spec == aURI.spec);
      Assert.ok(origin);
      Assert.ok(!origin.onDeleteVisitsCalled, "Observing onDeleteVisits for the first time");
      origin.onDeleteVisitsCalled = true;
    }
  };
  PlacesUtils.history.addObserver(observer, false);

  do_print("Removing the pages and checking the callbacks");
  let removed = yield PlacesUtils.history.remove(keys, page => {
    let origin = pages.find(candidate => candidate.uri.spec == page.url.href);

    Assert.ok(origin, "onResult has a valid page");
    Assert.ok(!origin.onResultCalled, "onResult has not seen this page yet");
    origin.onResultCalled = true;
    Assert.equal(page.guid, origin.guid, "onResult has the right guid");
    Assert.equal(page.title, origin.title, "onResult has the right title");
  });
  Assert.ok(removed, "Something was removed");

  PlacesUtils.history.removeObserver(observer);

  do_print("Checking out results");
  // By now the observers should have been called.
  for (let i = 0; i < pages.length; ++i) {
    let page = pages[i];
    do_print("Page: " + i);
    Assert.ok(page.onResultCalled, "We have reached the page from the callback");
    Assert.ok(visits_in_database(page.uri) == 0, "History entry has disappeared");
    Assert.equal(page_in_database(page.uri) != 0, page.hasBookmark, "Page is present only if it also has bookmarks");
    Assert.equal(page.onFrecencyChangedCalled, page.onDeleteVisitsCalled, "onDeleteVisits was called iff onFrecencyChanged was called");
    Assert.ok(page.onFrecencyChangedCalled ^ page.onDeleteURICalled, "Either onFrecencyChanged or onDeleteURI was called");
  }

  Assert.notEqual(visits_in_database(WITNESS_URI), 0, "Witness URI still has visits");
  Assert.notEqual(page_in_database(WITNESS_URI), 0, "Witness URI is still here");
});

add_task(function* cleanup() {
  yield PlacesTestUtils.clearHistory();
  yield PlacesUtils.bookmarks.eraseEverything();  
});

// Test the various error cases
add_task(function* test_error_cases() {
  Assert.throws(
    () =>  PlacesUtils.history.remove(),
    /TypeError: Invalid url/,
    "History.remove with no argument should throw a TypeError"
  );
  Assert.throws(
    () =>  PlacesUtils.history.remove(null),
    /TypeError: Invalid url/,
    "History.remove with `null` should throw a TypeError"
  );
  Assert.throws(
    () =>  PlacesUtils.history.remove(undefined),
    /TypeError: Invalid url/,
    "History.remove with `undefined` should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove("not a guid, obviously"),
    /TypeError: .* is not a valid URL/,
    "History.remove with an ill-formed guid/url argument should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove({"not the kind of object we know how to handle": true}),
    /TypeError: Invalid url/,
    "History.remove with an unexpected object should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove([]),
    /TypeError: Expected at least one page/,
    "History.remove with an empty array should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove([null]),
    /TypeError: Invalid url or guid/,
    "History.remove with an array containing null should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove(["http://example.org", "not a guid, obviously"]),
    /TypeError: .* is not a valid URL/,
    "History.remove with an array containing an ill-formed guid/url argument should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove(["0123456789ab"/*valid guid*/, null]),
    /TypeError: Invalid url or guid: null/,
    "History.remove with an array containing a guid and a second argument that is null should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove(["http://example.org", {"not the kind of object we know how to handle": true}]),
    /TypeError: Invalid url/,
    "History.remove with an array containing an unexpected objecgt should throw a TypeError"
  );
  Assert.throws(
    () => PlacesUtils.history.remove("http://example.org", "not a function, obviously"),
    /TypeError: Invalid function/,
    "History.remove with a second argument that is not a function argument should throw a TypeError"
  );
  try {
    PlacesUtils.history.remove("http://example.org/I/have/clearly/not/been/added", null);
    Assert.ok(true, "History.remove should ignore `null` as a second argument");
  } catch (ex) {
    Assert.ok(false, "History.remove should ignore `null` as a second argument");
  }
});

function run_test() {
  run_next_test();
}
