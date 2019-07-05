/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that history initialization correctly handles a request to forcibly
// replace the current database.

add_task(async function() {
  // In reality, this won't try to clone the database, because attached
  // databases cannot be supported when cloning. This test also verifies that.
  await test_database_replacement(
    "../migration/favicons_v41.sqlite",
    "favicons.sqlite",
    true,
    PlacesUtils.history.DATABASE_STATUS_CREATE
  );
});
