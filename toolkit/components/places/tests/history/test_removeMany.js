/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

// Tests for `History.remove` with removing many urls, as implemented in
// History.jsm.

"use strict";

// Test removing a list of pages
add_task(async function test_remove_many() {
  // This is set so that we are guaranteed to trigger REMOVE_PAGES_CHUNKLEN.
  const SIZE = 310;

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  info("Adding a witness page");
  let WITNESS_URI = NetUtil.newURI(
    "http://mozilla.com/test_browserhistory/test_remove/" + Math.random()
  );
  await PlacesTestUtils.addVisits(WITNESS_URI);
  Assert.ok(page_in_database(WITNESS_URI), "Witness page added");

  info("Generating samples");
  let pages = [];
  for (let i = 0; i < SIZE; ++i) {
    let uri = NetUtil.newURI(
      "http://mozilla.com/test_browserhistory/test_remove?sample=" +
        i +
        "&salt=" +
        Math.random()
    );
    let title = "Visit " + i + ", " + Math.random();
    let hasBookmark = i % 3 == 0;
    let page = {
      uri,
      title,
      hasBookmark,
      // `true` once `onResult` has been called for this page
      onResultCalled: false,
      // `true` once page-removed for store has been fired for this page
      pageRemovedFromStore: false,
      // `true` once page-removed for all visits has been fired for this page
      pageRemovedAllVisits: false,
    };
    info("Pushing: " + uri.spec);
    pages.push(page);

    await PlacesTestUtils.addVisits(page);
    page.guid = await PlacesTestUtils.getDatabaseValue("moz_places", "guid", {
      url: uri,
    });
    if (hasBookmark) {
      await PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: uri,
        title: "test bookmark " + i,
      });
    }
    Assert.ok(page_in_database(uri), "Page added");
  }

  info("Mixing key types and introducing dangling keys");
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

  let onPageRankingChanged = false;
  const placesEventListener = events => {
    for (const event of events) {
      switch (event.type) {
        case "page-title-changed": {
          Assert.ok(
            false,
            "Unexpected page-title-changed event happens on " + event.url
          );
          break;
        }
        case "history-cleared": {
          Assert.ok(false, "Unexpected history-cleared event happens");
          break;
        }
        case "pages-rank-changed": {
          onPageRankingChanged = true;
          break;
        }
        case "page-removed": {
          const origin = pages.find(x => x.uri.spec === event.url);
          Assert.ok(origin);

          if (event.isRemovedFromStore) {
            Assert.ok(
              !origin.hasBookmark,
              "Observing page-removed event on a page without a bookmark"
            );
            Assert.ok(
              !origin.pageRemovedFromStore,
              "Observing page-removed for store for the first time"
            );
            origin.pageRemovedFromStore = true;
          } else {
            Assert.ok(
              !origin.pageRemovedAllVisits,
              "Observing page-removed for all visits for the first time"
            );
            origin.pageRemovedAllVisits = true;
          }
          break;
        }
      }
    }
  };

  PlacesObservers.addListener(
    [
      "page-title-changed",
      "history-cleared",
      "pages-rank-changed",
      "page-removed",
    ],
    placesEventListener
  );

  info("Removing the pages and checking the callbacks");

  let removed = await PlacesUtils.history.remove(keys, page => {
    let origin = pages.find(candidate => candidate.uri.spec == page.url.href);

    Assert.ok(origin, "onResult has a valid page");
    Assert.ok(!origin.onResultCalled, "onResult has not seen this page yet");
    origin.onResultCalled = true;
    Assert.equal(page.guid, origin.guid, "onResult has the right guid");
    Assert.equal(page.title, origin.title, "onResult has the right title");
  });
  Assert.ok(removed, "Something was removed");
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  PlacesObservers.removeListener(
    [
      "page-title-changed",
      "history-cleared",
      "pages-rank-changed",
      "page-removed",
    ],
    placesEventListener
  );

  info("Checking out results");
  // By now the observers should have been called.
  for (let i = 0; i < pages.length; ++i) {
    let page = pages[i];
    Assert.ok(
      page.onResultCalled,
      `We have reached the page #${i} from the callback`
    );
    Assert.ok(
      visits_in_database(page.uri) == 0,
      "History entry has disappeared"
    );
    Assert.equal(
      page_in_database(page.uri) != 0,
      page.hasBookmark,
      "Page is present only if it also has bookmarks"
    );
    Assert.notEqual(
      page.pageRemovedFromStore,
      page.pageRemovedAllVisits,
      "Either only page-removed event for store or all visits should be called"
    );
  }

  Assert.equal(
    onPageRankingChanged,
    pages.some(p => p.pageRemovedFromStore || p.pageRemovedAllVisits),
    "page-rank-changed was fired"
  );

  Assert.notEqual(
    visits_in_database(WITNESS_URI),
    0,
    "Witness URI still has visits"
  );
  Assert.notEqual(
    page_in_database(WITNESS_URI),
    0,
    "Witness URI is still here"
  );
});

add_task(async function cleanup() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
});
