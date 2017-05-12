// Add pages.
let shorturl = "http://example.com/" + "a".repeat(1981);
let longurl = "http://example.com/" + "a".repeat(1982);
let bmurl = "http://example.com/" + "a".repeat(1983);

add_task(async function setup() {
  await setupPlacesDatabase("places_v31.sqlite");
  // Setup database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });

  await db.execute(`INSERT INTO moz_places (url, guid, foreign_count)
                    VALUES (:shorturl, "test1_______", 0)
                         , (:longurl, "test2_______", 0)
                         , (:bmurl, "test3_______", 1)
                   `, { shorturl, longurl, bmurl });
  // Add visits.
  await db.execute(`INSERT INTO moz_historyvisits (place_id)
                    VALUES ((SELECT id FROM moz_places WHERE url = :shorturl))
                         , ((SELECT id FROM moz_places WHERE url = :longurl))
                   `, { shorturl, longurl });
  await db.close();
});

add_task(async function database_is_valid() {
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal((await db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(async function test_longurls() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`SELECT 1 FROM moz_places where url = :longurl`,
                              { longurl });
  Assert.equal(rows.length, 0, "Long url should have been removed");
  rows = await db.execute(`SELECT 1 FROM moz_places where url = :shorturl`,
                          { shorturl });
  Assert.equal(rows.length, 1, "Short url should have been retained");
  rows = await db.execute(`SELECT 1 FROM moz_places where url = :bmurl`,
                          { bmurl });
  Assert.equal(rows.length, 1, "Bookmarked url should have been retained");
  rows = await db.execute(`SELECT count(*) FROM moz_historyvisits`);
  Assert.equal(rows.length, 1, "Orphan visists should have been removed");
});
