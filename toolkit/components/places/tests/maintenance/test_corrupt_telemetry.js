/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that history initialization correctly handles a request to forcibly
// replace the current database.

add_task(async function() {
  await createCorruptDb("places.sqlite");

  let count = Services.telemetry
                      .getHistogramById("PLACES_DATABASE_CORRUPTION_HANDLING_STAGE")
                      .snapshot()
                      .counts[3];
  Assert.equal(count, 0, "There should be no telemetry");

  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_CORRUPT);

  count = Services.telemetry
                  .getHistogramById("PLACES_DATABASE_CORRUPTION_HANDLING_STAGE")
                  .snapshot()
                  .counts[3];
  Assert.equal(count, 1, "Telemetry should have been added");
});
