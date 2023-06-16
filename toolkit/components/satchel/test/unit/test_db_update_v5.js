/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  let destPath = await copyToProfile(
    "formhistory_v3.sqlite",
    "formhistory.sqlite"
  );
  Assert.equal(3, await getDBSchemaVersion(destPath));

  // Do something that will cause FormHistory to access and upgrade the
  // database
  await FormHistory.count({});

  // check for upgraded schema.
  Assert.equal(CURRENT_SCHEMA, await getDBSchemaVersion(destPath));

  // Check that the source tables were added.
  let db = await Sqlite.openConnection({ path: destPath });
  try {
    Assert.ok(db.tableExists("moz_sources"));
    Assert.ok(db.tableExists("moz_sources_to_history"));
  } finally {
    await db.close();
  }
  // check that an entry still exists
  let num = await promiseCountEntries("name-A", "value-A");
  Assert.ok(num > 0);
});
