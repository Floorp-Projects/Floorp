/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that all the operations affecting frecency are triggering a recalc.
 * This currently includes:
 *  - adding visits (will recalc immediately)
 *  - removing visits
 *  - adding a bookmark
 *  - removing a bookmark
 *  - changing url of a bookmark+
 *
 * Also check setting a frecency resets recalc_frecency to 0.
 **/

const TEST_URL = "https://example.com/";
const TEST_URL_2 = "https://example2.com/";

// NOTE: Until we fix Bug 1806666 this test has to run queries manually because
// the official APIs recalculate frecency immediately. After the fix, these
// helpers can be removed and the test can be much simpler.
function insertVisit(url) {
  return PlacesUtils.withConnectionWrapper("insertVisit", async db => {
    await db.execute(
      `INSERT INTO moz_historyvisits(place_id, visit_date)
       VALUES ((SELECT id FROM moz_places WHERE url = :url), 1648226608386000)`,
      { url }
    );
  });
}
function removeVisit(url) {
  return PlacesUtils.withConnectionWrapper("insertVisit", async db => {
    await db.execute(
      `DELETE FROM moz_historyvisits WHERE place_id
        = (SELECT id FROM moz_places WHERE url = :url)`,
      { url }
    );
  });
}
function resetFrecency(url) {
  return PlacesUtils.withConnectionWrapper("insertVisit", async db => {
    await db.execute(`UPDATE moz_places SET frecency = -1 WHERE url = :url`, {
      url,
    });
  });
}
function changeBookmarkToUrl(guid, url) {
  return PlacesUtils.withConnectionWrapper("insertVisit", async db => {
    await db.execute(
      `UPDATE moz_bookmarks SET fk = (SELECT id FROM moz_places WHERE url = :url )
       WHERE guid = :guid`,
      {
        url,
        guid,
      }
    );
  });
}
function removeBookmark(guid) {
  return PlacesUtils.withConnectionWrapper("insertVisit", async db => {
    await db.execute(`DELETE FROM moz_bookmarks WHERE guid = :guid`, {
      guid,
    });
  });
}
add_task(async function test_visit() {
  // First add a bookmark so the page is not orphaned.
  let bm = await PlacesUtils.bookmarks.insert({
    url: TEST_URL,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });

  info("Add a visit check frecency is calculated immediately");
  await PlacesTestUtils.addVisits(TEST_URL);
  let originalFrecency = await PlacesTestUtils.fieldInDB(TEST_URL, "frecency");
  Assert.ok(originalFrecency > 0, "frecency was recalculated immediately");
  let recalc = await PlacesTestUtils.fieldInDB(TEST_URL, "recalc_frecency");
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");

  info("Add a visit (raw query) check frecency is not calculated immadiately");
  await insertVisit(TEST_URL);
  let frecency = await PlacesTestUtils.fieldInDB(TEST_URL, "frecency");
  Assert.equal(frecency, originalFrecency, "frecency is unchanged");
  recalc = await PlacesTestUtils.fieldInDB(TEST_URL, "recalc_frecency");
  Assert.equal(recalc, 1, "frecency needs a recalc");

  info("Check setting frecency resets recalc_frecency");
  await resetFrecency(TEST_URL);
  recalc = await PlacesTestUtils.fieldInDB(TEST_URL, "recalc_frecency");
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");

  info("Removing a visit sets recalc_frecency");
  await removeVisit(TEST_URL);
  frecency = await PlacesTestUtils.fieldInDB(TEST_URL, "frecency");
  Assert.equal(frecency, -1, "frecency is unchanged");
  recalc = await PlacesTestUtils.fieldInDB(TEST_URL, "recalc_frecency");
  Assert.equal(recalc, 1, "frecency needs a recalc");

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.remove(bm);
});

add_task(async function test_bookmark() {
  // First add a visit so the page is not orphaned.
  await PlacesTestUtils.addVisits(TEST_URL);
  await PlacesTestUtils.addVisits(TEST_URL_2);

  info("Check adding a bookmark sets frecency immediately");
  let bm = await PlacesUtils.bookmarks.insert({
    url: TEST_URL,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  let originalFrecency = await PlacesTestUtils.fieldInDB(TEST_URL, "frecency");
  Assert.ok(originalFrecency > 0, "frecency was recalculated immediately");
  let recalc = await PlacesTestUtils.fieldInDB(TEST_URL, "recalc_frecency");
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");

  info("Check changing a bookmark url sets recalc_frecency on both urls");
  await changeBookmarkToUrl(bm.guid, TEST_URL_2);
  let frecency = await PlacesTestUtils.fieldInDB(TEST_URL, "frecency");
  Assert.equal(frecency, originalFrecency, "frecency is unchanged");
  recalc = await PlacesTestUtils.fieldInDB(TEST_URL, "recalc_frecency");
  Assert.equal(recalc, 1, "frecency needs a recalc");
  frecency = await PlacesTestUtils.fieldInDB(TEST_URL_2, "frecency");
  Assert.ok(frecency > 0, "frecency is valid");
  recalc = await PlacesTestUtils.fieldInDB(TEST_URL_2, "recalc_frecency");
  Assert.equal(recalc, 1, "frecency needs a recalc");

  info("Check setting frecency resets recalc_frecency");
  await resetFrecency(TEST_URL);
  recalc = await PlacesTestUtils.fieldInDB(TEST_URL, "recalc_frecency");
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");
  await resetFrecency(TEST_URL_2);
  recalc = await PlacesTestUtils.fieldInDB(TEST_URL_2, "recalc_frecency");
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");

  info("Removing a bookmark sets recalc_frecency");
  await removeBookmark(bm.guid);
  frecency = await PlacesTestUtils.fieldInDB(TEST_URL, "frecency");
  Assert.equal(frecency, -1, "frecency is unchanged");
  recalc = await PlacesTestUtils.fieldInDB(TEST_URL, "recalc_frecency");
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");
  frecency = await PlacesTestUtils.fieldInDB(TEST_URL_2, "frecency");
  Assert.equal(frecency, -1, "frecency is unchanged");
  recalc = await PlacesTestUtils.fieldInDB(TEST_URL_2, "recalc_frecency");
  Assert.equal(recalc, 1, "frecency needs a recalc");

  await PlacesUtils.history.clear();
});
