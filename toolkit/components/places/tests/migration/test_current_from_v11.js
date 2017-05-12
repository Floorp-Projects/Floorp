/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await setupPlacesDatabase("places_v11.sqlite");
});

add_task(async function database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal((await db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(async function test_moz_hosts() {
  let db = await PlacesUtils.promiseDBConnection();

  // This will throw if the column does not exist.
  await db.execute("SELECT host, frecency, typed, prefix FROM moz_hosts");

  // moz_hosts is populated asynchronously, so we need to wait.
  await PlacesTestUtils.promiseAsyncUpdates();

  // check the number of entries in moz_hosts equals the number of
  // unique rev_host in moz_places
  let rows = await db.execute(
    `SELECT (SELECT COUNT(host) FROM moz_hosts),
            (SELECT COUNT(DISTINCT rev_host)
             FROM moz_places
             WHERE LENGTH(rev_host) > 1)
    `);

  Assert.equal(rows.length, 1);
  let mozHostsCount = rows[0].getResultByIndex(0);
  let mozPlacesCount = rows[0].getResultByIndex(1);

  Assert.ok(mozPlacesCount > 0, "There is some url in the database");
  Assert.equal(mozPlacesCount, mozHostsCount, "moz_hosts has the expected number of entries");
});

add_task(async function test_journal() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute("PRAGMA journal_mode");
  Assert.equal(rows.length, 1);
  // WAL journal mode should be set on this database.
  Assert.equal(rows[0].getResultByIndex(0), "wal");
});
