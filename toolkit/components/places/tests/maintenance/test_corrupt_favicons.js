/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that history initialization correctly handles a corrupt favicons file
// that can't be opened.

add_task(async function() {
  await createCorruptDb("favicons.sqlite");

  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_CREATE);
  let db = await PlacesUtils.promiseDBConnection();
  await db.execute("SELECT * FROM moz_icons"); // Should not fail.
});
