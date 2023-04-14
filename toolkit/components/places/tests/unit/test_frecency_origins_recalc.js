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
  const url = "https://mozilla.org/test/";
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
  // TODO: use PlacesTestUtils.getDatabaseValue once available.
  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal(
    (
      await db.execute(`SELECT recalc_frecency FROM moz_origins`)
    )[0].getResultByIndex(0),
    0,
    "Should have been calculated already"
  );
  Assert.equal(
    (
      await db.execute(`SELECT recalc_alt_frecency FROM moz_origins`)
    )[0].getResultByIndex(0),
    1,
    "Should not have been calculated"
  );

  // Remove only one visit (otherwise the page would be orphaned).
  await PlacesUtils.history.removeByFilter({
    beginDate: new Date(now - 10000),
    endDate: new Date(now + 10000),
  });
  Assert.equal(
    (
      await db.execute(`SELECT recalc_frecency FROM moz_origins`)
    )[0].getResultByIndex(0),
    0,
    "Should have been calculated already"
  );
  Assert.equal(
    (
      await db.execute(`SELECT recalc_alt_frecency FROM moz_origins`)
    )[0].getResultByIndex(0),
    1,
    "Should not have been calculated yet"
  );
  // test recalc_frecency is set back to 0 when frecency of the origin is set
});
