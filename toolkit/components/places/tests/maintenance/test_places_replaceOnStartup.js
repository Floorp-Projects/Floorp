/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that history initialization correctly handles a request to forcibly
// replace the current database.

add_task(async function () {
  await test_database_replacement(
    ["migration", "places_v52.sqlite"],
    "places.sqlite",
    false,
    PlacesUtils.history.DATABASE_STATUS_CORRUPT
  );
});
