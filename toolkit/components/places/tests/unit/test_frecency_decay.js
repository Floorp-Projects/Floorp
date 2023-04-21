/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_FREC_DECAY_RATE_DEF = 0.975;

/**
 * Promises that the pages-rank-changed event has been seen.
 *
 * @returns {Promise} A promise which is resolved when the notification is seen.
 */
function promiseRankingChanged() {
  return PlacesTestUtils.waitForNotification("pages-rank-changed");
}

add_task(async function setup() {
  Services.prefs.setCharPref(
    "places.frecency.decayRate",
    PREF_FREC_DECAY_RATE_DEF
  );
});

add_task(async function test_isFrecencyDecaying() {
  let db = await PlacesUtils.promiseDBConnection();
  async function queryFrecencyDecaying() {
    return (
      await db.executeCached(`SELECT is_frecency_decaying()`)
    )[0].getResultByIndex(0);
  }
  PlacesUtils.history.isFrecencyDecaying = true;
  Assert.equal(PlacesUtils.history.isFrecencyDecaying, true);
  Assert.equal(await queryFrecencyDecaying(), true);
  PlacesUtils.history.isFrecencyDecaying = false;
  Assert.equal(PlacesUtils.history.isFrecencyDecaying, false);
  Assert.equal(await queryFrecencyDecaying(), false);
});

add_task(async function test_frecency_decay() {
  let unvisitedBookmarkFrecency = Services.prefs.getIntPref(
    "places.frecency.unvisitedBookmarkBonus"
  );

  // Add a bookmark and check its frecency.
  let url = "http://example.com/b";
  let promiseOne = promiseRankingChanged();
  await PlacesUtils.bookmarks.insert({
    url,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  await promiseOne;

  let histogram = TelemetryTestUtils.getAndClearHistogram(
    "PLACES_IDLE_FRECENCY_DECAY_TIME_MS"
  );
  info("Trigger frecency decay.");
  Assert.equal(PlacesUtils.history.isFrecencyDecaying, false);
  let promiseRanking = promiseRankingChanged();

  PlacesFrecencyRecalculator.observe(null, "idle-daily", "");
  Assert.equal(PlacesUtils.history.isFrecencyDecaying, true);
  info("Wait for completion.");
  await PlacesFrecencyRecalculator.pendingFrecencyDecayPromise;

  await promiseRanking;
  Assert.equal(PlacesUtils.history.isFrecencyDecaying, false);

  // Now check the new frecency is correct.
  let newFrecency = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "frecency",
    { url }
  );

  Assert.equal(
    newFrecency,
    Math.round(unvisitedBookmarkFrecency * PREF_FREC_DECAY_RATE_DEF),
    "Frecencies should match"
  );

  let snapshot = histogram.snapshot();
  Assert.greater(snapshot.sum, 0);
});
