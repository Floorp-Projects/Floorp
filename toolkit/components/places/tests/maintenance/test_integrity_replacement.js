/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that integrity check will replace a corrupt database.

add_task(async function() {
  await setupPlacesDatabase("corruptPayload.sqlite");
  await Assert.rejects(
    PlacesDBUtils.checkIntegrity(),
    /will be replaced on next startup/,
    "Should reject on corruption"
  );
  Assert.equal(
    Services.prefs.getCharPref("places.database.replaceDatabaseOnStartup"),
    DB_FILENAME
  );
});
