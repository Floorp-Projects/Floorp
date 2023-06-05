/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that recalc_alt_frecency in the moz_places table is updated correctly.

add_task(async function test() {
  // Start from a stable situation, otherwise the recalculator may set recalc
  // field at any time due to the initialization.
  let subject = {};
  PlacesFrecencyRecalculator.observe(
    subject,
    "test-alternative-frecency-init",
    ""
  );
  await subject.promise;

  info("test recalc_alt_frecency is set to 1 when a visit is added");
  const now = new Date();
  const URL = "https://mozilla.org/test/";
  let getRecalc = url =>
    PlacesTestUtils.getDatabaseValue("moz_places", "recalc_alt_frecency", {
      url,
    });
  let setRecalc = (url, val) =>
    PlacesTestUtils.updateDatabaseValues(
      "moz_places",
      { recalc_alt_frecency: val },
      { url }
    );
  let getFrecency = url =>
    PlacesTestUtils.getDatabaseValue("moz_places", "alt_frecency", {
      url,
    });
  await PlacesTestUtils.addVisits([
    {
      url: URL,
      visitDate: now,
    },
    {
      url: URL,
      visitDate: new Date(new Date().setDate(now.getDate() - 30)),
    },
  ]);
  // The frecency has been recalculated by addVisits already, checking that
  // alt frecency has a positive value should be sufficient anyway.
  Assert.equal(await getRecalc(URL), 0);
  Assert.greater(await getFrecency(URL), 0);

  info("Remove just one visit (otherwise the page would be orphaned).");
  await PlacesUtils.history.removeVisitsByFilter({
    beginDate: new Date(now.valueOf() - 10000),
    endDate: new Date(now.valueOf() + 10000),
  });
  Assert.equal(await getRecalc(URL), 1);
  await setRecalc(URL, 0);

  info("Add a bookmark to the page");
  let bm = await PlacesUtils.bookmarks.insert({
    url: URL,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  Assert.equal(await getRecalc(URL), 1);
  await setRecalc(URL, 0);

  info("Clear history");
  await PlacesUtils.history.clear();
  Assert.equal(await getRecalc(URL), 1);
  await setRecalc(URL, 0);

  // Add back a visit so the page is not an orphan once we remove the bookmark.
  await PlacesTestUtils.addVisits(URL);
  Assert.equal(await getRecalc(URL), 0);
  Assert.greater(await getFrecency(URL), 0);

  info("change the bookmark URL");
  const URL2 = "https://editedbookmark.org/";
  bm.url = URL2;
  await PlacesUtils.bookmarks.update(bm);
  Assert.equal(await getRecalc(URL), 1);
  Assert.equal(await getRecalc(URL2), 1);
  await setRecalc(URL, 0);
  await setRecalc(URL2, 0);

  info("Remove the bookmark from the page");
  await PlacesUtils.bookmarks.remove(bm);
  Assert.equal(await getRecalc(URL2), 1);
  await setRecalc(URL2, 0);

  const URL3 = "https://test3.moz.org/";
  const URL4 = "https://test4.moz.org/";
  info("Insert multiple pages now");
  await PlacesTestUtils.addVisits([URL3, URL4]);
  Assert.equal(await getRecalc(URL3), 0);
  Assert.greater(await getFrecency(URL3), 0);
  Assert.equal(await getRecalc(URL4), 0);
  Assert.greater(await getFrecency(URL4), 0);
});
