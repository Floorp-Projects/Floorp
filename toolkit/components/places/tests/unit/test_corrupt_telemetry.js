/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that history initialization correctly handles a request to forcibly
// replace the current database.

add_task(async function() {
  let profileDBPath = await OS.Path.join(OS.Constants.Path.profileDir, "places.sqlite");
  await OS.File.remove(profileDBPath, {ignoreAbsent: true});
  // Ensure that our database doesn't already exist.
  Assert.ok(!(await OS.File.exists(profileDBPath)), "places.sqlite shouldn't exist");
  let dir = await OS.File.getCurrentDirectory();
  let src = OS.Path.join(dir, "corruptDB.sqlite");
  await OS.File.copy(src, profileDBPath);
  Assert.ok(await OS.File.exists(profileDBPath), "places.sqlite should exist");

  let count = Services.telemetry
                      .getHistogramById("PLACES_DATABASE_CORRUPTION_HANDLING_STAGE")
                      .snapshot()
                      .counts[3];
  Assert.equal(count, 0, "There should be no telemetry");

  do_check_eq(PlacesUtils.history.databaseStatus,
              PlacesUtils.history.DATABASE_STATUS_CORRUPT);

  count = Services.telemetry
                  .getHistogramById("PLACES_DATABASE_CORRUPTION_HANDLING_STAGE")
                  .snapshot()
                  .counts[3];
  Assert.equal(count, 1, "Telemetry should have been added");
});
