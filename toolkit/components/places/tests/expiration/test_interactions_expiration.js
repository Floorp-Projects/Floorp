/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests expiration of Places interactions data.
 */
// Number of days in the past where interactions will be expired.
const EXPIRE_DAYS = 60;
// Should be more recent than EXPIRED_DAYS.
const RECENT_DATE = new Date() - (EXPIRE_DAYS - 1) * 86400000;

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.places.interactions.enabled", true);
  Services.prefs.setIntPref(
    "browser.places.interactions.expireDays",
    EXPIRE_DAYS
  );
});

add_task(async function test_expire_interactions() {
  // Add visits and metadata to 2 pages and force expiration.
  await PlacesTestUtils.addVisits([
    "https://expired.mozilla.org/",
    "https://interactions-expired.mozilla.org/",
    "https://some-interaction-expired.mozilla.org/",
    "https://not-expired.mozilla.org/",
  ]);
  // Insert dummy interactions for all the pages.
  await addDummyInteractions("https://removed.mozilla.org/", [0]);
  await addDummyInteractions("https://interactions-expired.mozilla.org/", [
    EXPIRE_DAYS + 10,
  ]);
  await addDummyInteractions("https://some-interactions-expired.mozilla.org/", [
    0,
    EXPIRE_DAYS + 10,
  ]);
  await addDummyInteractions("https://not-expired.mozilla.org/", [
    0,
    EXPIRE_DAYS / 2,
  ]);

  info("Remove a page from history and check interactions are removed");
  await PlacesUtils.history.remove("https://removed.mozilla.org/");
  await checkDummyInteractions("https://removed.mozilla.org/", 0);

  // Expire now.
  await promiseForceExpirationStep(-1);

  info("Test interactions expiration result");
  await checkDummyInteractions("https://interactions-expired.mozilla.org/", 0);
  await checkDummyInteractions(
    "https://some-interactions-expired.mozilla.org/",
    1
  );
  await checkDummyInteractions("https://not-expired.mozilla.org/", 2);

  // Clean up.
  await PlacesUtils.history.clear();
});

async function addDummyInteractions(url, interactionDaysAgo) {
  await PlacesTestUtils.addVisits(url);
  await PlacesUtils.withConnectionWrapper(
    "test_interactions_expiration.js: addDummyInteraction",
    async db => {
      await db.execute(
        `INSERT INTO moz_places_metadata (place_id, created_at, updated_at) VALUES (
        (SELECT id FROM moz_places WHERE url_hash = hash(:url)),
        strftime('%s','now','localtime','-' || :days || ' day','start of day','utc') * 1000,
        strftime('%s','now','localtime','-' || :days || ' day','start of day','utc') * 1000
      )`,
        interactionDaysAgo.map(days => ({ url, days }))
      );
    }
  );
}

async function checkDummyInteractions(url, interactionsLen) {
  info("Check interactions for " + url);
  await PlacesUtils.withConnectionWrapper(
    "test_interactions_expiration.js: addDummyInteraction",
    async db => {
      let rows = await db.execute(
        `SELECT updated_at
         FROM moz_places_metadata
         WHERE place_id = (SELECT id FROM moz_places WHERE url_hash = hash(:url))
         ORDER BY updated_at DESC`,
        { url }
      );
      let dates = rows.map(r => new Date(r.getResultByName("updated_at")));
      Assert.equal(
        rows.length,
        interactionsLen,
        "Found expected number of interactions"
      );
      Assert.ok(
        dates.every(d => d > RECENT_DATE),
        "All interactions are recent"
      );
    }
  );
}
