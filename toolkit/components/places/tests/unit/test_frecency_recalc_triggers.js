/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that all the operations affecting frecency are either recalculating
 * immediately or triggering a recalculation.
 * Operations that should recalculate immediately:
 *  - adding visits
 * Operations that should just trigger a recalculation:
 *  - removing visits
 *  - adding a bookmark
 *  - removing a bookmark
 *  - changing url of a bookmark
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
      `INSERT INTO moz_historyvisits(place_id, visit_date, visit_type)
       VALUES ((SELECT id FROM moz_places WHERE url = :url), 1648226608386000, 1)`,
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

add_task(async function test_visit() {
  // First add a bookmark so the page is not orphaned.
  let bm = await PlacesUtils.bookmarks.insert({
    url: TEST_URL,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });

  info("Add a visit check frecency is calculated immediately");
  await PlacesTestUtils.addVisits(TEST_URL);
  let originalFrecency = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.greater(originalFrecency, 0, "frecency was recalculated immediately");
  let recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");

  info("Add a visit (raw query) check frecency is not calculated immediately");
  await insertVisit(TEST_URL);
  let frecency = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.equal(frecency, originalFrecency, "frecency is unchanged");
  recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.equal(recalc, 1, "frecency needs a recalc");

  info("Check setting frecency resets recalc_frecency");
  await resetFrecency(TEST_URL);
  recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");

  info("Removing a visit sets recalc_frecency");
  await removeVisit(TEST_URL);
  frecency = await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
    url: TEST_URL,
  });
  Assert.equal(frecency, -1, "frecency is unchanged");
  recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.equal(recalc, 1, "frecency needs a recalc");

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.remove(bm);
});

add_task(async function test_bookmark() {
  // First add a visit so the page is not orphaned.
  await PlacesTestUtils.addVisits([TEST_URL, TEST_URL_2]);

  let originalFrecency = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.greater(originalFrecency, 0);

  info("Check adding a bookmark sets recalc_frecency");
  let bm = await PlacesUtils.bookmarks.insert({
    url: TEST_URL,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });

  let frecency = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.equal(frecency, originalFrecency, "frecency is unchanged");
  let recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.equal(recalc, 1, "frecency needs a recalc");

  info("Check changing a bookmark url sets recalc_frecency on both urls");
  await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    url: TEST_URL_2,
  });
  frecency = await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
    url: TEST_URL,
  });
  Assert.equal(frecency, originalFrecency, "frecency is unchanged");
  recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.equal(recalc, 1, "frecency needs a recalc");
  frecency = await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
    url: TEST_URL_2,
  });
  Assert.ok(frecency > 0, "frecency is valid");
  recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL_2,
    }
  );
  Assert.equal(recalc, 1, "frecency needs a recalc");

  info("Check setting frecency resets recalc_frecency");
  await resetFrecency(TEST_URL);
  recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");
  await resetFrecency(TEST_URL_2);
  recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL_2,
    }
  );
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");

  info("Removing a bookmark sets recalc_frecency");
  await PlacesUtils.bookmarks.remove(bm.guid);
  frecency = await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
    url: TEST_URL,
  });
  Assert.equal(frecency, -1, "frecency is unchanged");
  recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL,
    }
  );
  Assert.equal(recalc, 0, "frecency doesn't need a recalc");
  frecency = await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
    url: TEST_URL_2,
  });
  Assert.equal(frecency, -1, "frecency is unchanged");
  recalc = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "recalc_frecency",
    {
      url: TEST_URL_2,
    }
  );
  Assert.equal(recalc, 1, "frecency needs a recalc");

  await PlacesUtils.history.clear();
});

add_task(async function test_bookmark_frecency_zero() {
  info("A url with frecency 0 should be recalculated if bookmarked");
  let url = "https://zerofrecency.org/";
  await PlacesTestUtils.addVisits({ url, transition: TRANSITION_FRAMED_LINK });
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", { url }),
    0
  );
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_places", "recalc_frecency", {
      url,
    }),
    0
  );
  await PlacesUtils.bookmarks.insert({
    url,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_places", "recalc_frecency", {
      url,
    }),
    1
  );
  info("place: uris should not be recalculated");
  url = "place:test";
  await PlacesUtils.bookmarks.insert({
    url,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", { url }),
    0
  );
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_places", "recalc_frecency", {
      url,
    }),
    0
  );
});
