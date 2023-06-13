/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that recalc_frecency in the moz_origins table works is consistent.

// This test does not completely cover origins frecency recalculation because
// the current system uses temp tables and triggers to make the recalculation,
// but it's likely that will change in the future and then we can add to this.

add_task(async function test() {
  // test recalc_frecency is set to 1 when frecency of a page changes.
  // Add a couple visits, then remove one of them.
  const now = new Date();
  const host = "mozilla.org";
  const url = `https://${host}/test/`;
  await PlacesTestUtils.addVisits([
    {
      url,
      visitDate: now,
    },
    {
      url,
      visitDate: new Date(new Date().setDate(now.getDate() - 30)),
    },
  ]);
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_origins", "recalc_frecency", {
      host,
    }),
    0,
    "Should have been calculated already"
  );
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue(
      "moz_origins",
      "recalc_alt_frecency",
      {
        host,
      }
    ),
    1,
    "Should have been calculated already"
  );
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  let alt_frecency = await PlacesTestUtils.getDatabaseValue(
    "moz_origins",
    "alt_frecency",
    { host }
  );
  let frecency = await PlacesTestUtils.getDatabaseValue(
    "moz_origins",
    "frecency",
    { host }
  );
  // Remove only one visit (otherwise the page would be orphaned).
  await PlacesUtils.history.removeVisitsByFilter({
    beginDate: new Date(now.valueOf() - 10000),
    endDate: new Date(now.valueOf() + 10000),
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_origins", "recalc_frecency", {
      host,
    }),
    0,
    "Should have been calculated"
  );
  Assert.greater(
    frecency,
    await PlacesTestUtils.getDatabaseValue("moz_origins", "frecency", {
      host,
    }),
    "frecency should have decreased"
  );
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue(
      "moz_origins",
      "recalc_alt_frecency",
      { host }
    ),
    0,
    "Should have been calculated"
  );
  Assert.greater(
    alt_frecency,
    await PlacesTestUtils.getDatabaseValue("moz_origins", "alt_frecency", {
      host,
    }),
    "alternative frecency should have decreased"
  );

  // Add another page to the same host.
  const url2 = `https://${host}/second/`;
  await PlacesTestUtils.addVisits(url2);
  // Remove the first page.
  await PlacesUtils.history.remove(url);
  // Note common frecency is currently calculated by a trigger, this will change
  // in the future to use a delayed recalc.
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_origins", "recalc_frecency", {
      host,
    }),
    0,
    "Should have been recalculated"
  );
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue(
      "moz_origins",
      "recalc_alt_frecency",
      { host }
    ),
    1,
    "Should request recalculation"
  );
});
