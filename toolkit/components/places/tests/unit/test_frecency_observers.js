/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Each of these tests a path that triggers a frecency update.  Together they
// hit all sites that update a frecency.

// InsertVisitedURIs::UpdateFrecency and History::InsertPlace
add_task(
  async function test_InsertVisitedURIs_UpdateFrecency_and_History_InsertPlace() {
    // InsertPlace is at the end of a path that UpdateFrecency is also on, so kill
    // two birds with one stone and expect two notifications.  Trigger the path by
    // adding a download.
    let url = Services.io.newURI("http://example.com/a");
    let promise = onRankingChanged();
    await PlacesUtils.history.insert({
      url,
      visits: [
        {
          transition: PlacesUtils.history.TRANSITIONS.DOWNLOAD,
        },
      ],
    });
    await promise;
  }
);

// nsNavHistory::UpdateFrecency
add_task(async function test_nsNavHistory_UpdateFrecency() {
  let url = Services.io.newURI("http://example.com/b");
  let promise = onRankingChanged();
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url,
    title: "test",
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  await promise;
});

// History.sys.mjs invalidateFrecencies()
add_task(async function test_invalidateFrecencies() {
  let url = Services.io.newURI("http://test-invalidateFrecencies.com/");
  // Bookmarking the URI is enough to add it to moz_places, and importantly, it
  // means that removeByFilter doesn't remove it from moz_places, so its
  // frecency is able to be changed.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url,
    title: "test",
  });
  let promise = onRankingChanged();
  await PlacesUtils.history.removeByFilter({ host: url.host });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  await promise;
});

// History.sys.mjs clear() should not cause a frecency recalculation since pages
// are removed.
add_task(async function test_clear() {
  let received = [];
  let listener = events =>
    (received = received.concat(events.map(e => e.type)));
  PlacesObservers.addListener(
    ["history-cleared", "pages-rank-changed"],
    listener
  );
  await PlacesUtils.history.clear();
  PlacesObservers.removeListener(
    ["history-cleared", "pages-rank-changed"],
    listener
  );
  Assert.deepEqual(received, ["history-cleared"]);
});

add_task(async function test_nsNavHistory_idleDaily() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "https://test-site1.org",
    title: "test",
  });
  PlacesFrecencyRecalculator.observe(null, "idle-daily", "");
  await Promise.all([onRankingChanged()]);
});

add_task(async function test_nsNavHistory_recalculate() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "https://test-site1.org",
    title: "test",
  });
  await Promise.all([
    onRankingChanged(),
    PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies(),
  ]);
});

function onRankingChanged() {
  return PlacesTestUtils.waitForNotification("pages-rank-changed");
}
