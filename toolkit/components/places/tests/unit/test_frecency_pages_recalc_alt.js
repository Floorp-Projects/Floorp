/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that recalc_alt_frecency in the moz_places table is updated correctly.

add_task(async function test() {
  info("test recalc_alt_frecency is set to 1 when a visit is added");
  const now = new Date();
  const URL = "https://mozilla.org/test/";
  let getValue = url =>
    PlacesTestUtils.getDatabaseValue("moz_places", "recalc_alt_frecency", {
      url,
    });
  let setValue = (url, val) =>
    PlacesTestUtils.updateDatabaseValue(
      "moz_places",
      "recalc_alt_frecency",
      val,
      { url }
    );
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
  Assert.equal(await getValue(URL), 1);
  await setValue(URL, 0);

  info("Remove just one visit (otherwise the page would be orphaned).");
  await PlacesUtils.history.removeVisitsByFilter({
    beginDate: new Date(now.valueOf() - 10000),
    endDate: new Date(now.valueOf() + 10000),
  });
  Assert.equal(await getValue(URL), 1);
  await setValue(URL, 0);

  info("Add a bookmark to the page");
  let bm = await PlacesUtils.bookmarks.insert({
    url: URL,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  Assert.equal(await getValue(URL), 1);
  await setValue(URL, 0);

  info("Clear history");
  await PlacesUtils.history.clear();
  Assert.equal(await getValue(URL), 1);
  await setValue(URL, 0);

  // Add back a visit so the page is not an orphan once we remove the bookmark.
  await PlacesTestUtils.addVisits(URL);
  Assert.equal(await getValue(URL), 1);
  await setValue(URL, 0);

  info("change the bookmark URL");
  const URL2 = "https://editedbookmark.org/";
  bm.url = URL2;
  await PlacesUtils.bookmarks.update(bm);
  Assert.equal(await getValue(URL), 1);
  Assert.equal(await getValue(URL2), 1);
  await setValue(URL, 0);
  await setValue(URL2, 0);

  info("Remove the bookmark from the page");
  await PlacesUtils.bookmarks.remove(bm);
  Assert.equal(await getValue(URL2), 1);
  await setValue(URL2, 0);
});
