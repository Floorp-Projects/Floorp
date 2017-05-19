/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

// Tests for `History.remove` with removing many urls, as implemented in
// History.jsm.

"use strict";

// Test removing a list of pages
add_task(async function test_remove_many() {
  // This is set so that we are guaranteed to trigger REMOVE_PAGES_CHUNKLEN.
  const SIZE = 310;

  await PlacesTestUtils.clearHistory();
  await PlacesUtils.bookmarks.eraseEverything();

  do_print("Adding a witness page");
  let WITNESS_URI = NetUtil.newURI("http://mozilla.com/test_browserhistory/test_remove/" + Math.random());
  await PlacesTestUtils.addVisits(WITNESS_URI);
  Assert.ok(page_in_database(WITNESS_URI), "Witness page added");

  do_print("Generating samples");
  let pages = [];
  for (let i = 0; i < SIZE; ++i) {
    let uri = NetUtil.newURI("http://mozilla.com/test_browserhistory/test_remove?sample=" + i + "&salt=" + Math.random());
    let title = "Visit " + i + ", " + Math.random();
    let hasBookmark = i % 3 == 0;
    let page = {
      uri,
      title,
      hasBookmark,
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

    await PlacesTestUtils.addVisits(page);
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
    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onVisit(aURI) {
      Assert.ok(false, "Unexpected call to onVisit " + aURI.spec);
    },
    onTitleChanged(aURI) {
      Assert.ok(false, "Unexpected call to onTitleChanged " + aURI.spec);
    },
    onClearHistory() {
      Assert.ok(false, "Unexpected call to onClearHistory");
    },
    onPageChanged(aURI) {
      Assert.ok(false, "Unexpected call to onPageChanged " + aURI.spec);
    },
    onFrecencyChanged(aURI) {
      let origin = pages.find(x => x.uri.spec == aURI.spec);
      Assert.ok(origin);
      Assert.ok(origin.hasBookmark, "Observing onFrecencyChanged on a page with a bookmark");
      origin.onFrecencyChangedCalled = true;
    },
    onManyFrecenciesChanged() {
      Assert.ok(false, "Observing onManyFrecenciesChanges, this is most likely correct but not covered by this test");
    },
    onDeleteURI(aURI) {
      let origin = pages.find(x => x.uri.spec == aURI.spec);
      Assert.ok(origin);
      Assert.ok(!origin.hasBookmark, "Observing onDeleteURI on a page without a bookmark");
      Assert.ok(!origin.onDeleteURICalled, "Observing onDeleteURI for the first time");
      origin.onDeleteURICalled = true;
    },
    onDeleteVisits(aURI) {
      let origin = pages.find(x => x.uri.spec == aURI.spec);
      Assert.ok(origin);
      Assert.ok(!origin.onDeleteVisitsCalled, "Observing onDeleteVisits for the first time");
      origin.onDeleteVisitsCalled = true;
    }
  };
  PlacesUtils.history.addObserver(observer);

  do_print("Removing the pages and checking the callbacks");

  let removed = await PlacesUtils.history.remove(keys, page => {
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
    Assert.ok(page.onResultCalled, `We have reached the page #${i} from the callback`);
    Assert.ok(visits_in_database(page.uri) == 0, "History entry has disappeared");
    Assert.equal(page_in_database(page.uri) != 0, page.hasBookmark, "Page is present only if it also has bookmarks");
    Assert.equal(page.onFrecencyChangedCalled, page.onDeleteVisitsCalled, "onDeleteVisits was called iff onFrecencyChanged was called");
    Assert.ok(page.onFrecencyChangedCalled ^ page.onDeleteURICalled, "Either onFrecencyChanged or onDeleteURI was called");
  }

  Assert.notEqual(visits_in_database(WITNESS_URI), 0, "Witness URI still has visits");
  Assert.notEqual(page_in_database(WITNESS_URI), 0, "Witness URI is still here");
});

add_task(async function cleanup() {
  await PlacesTestUtils.clearHistory();
  await PlacesUtils.bookmarks.eraseEverything();
});
