/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await setupPlacesDatabase("places_v42.sqlite");
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });
  // Remove pre-existing keywords to avoid conflicts.
  await db.execute(`DELETE FROM moz_keywords`);
  await db.execute(`
    INSERT INTO moz_keywords (id, keyword, place_id, post_data)
    VALUES (1, "keyword1", 1, NULL),
           (2, "keyword2", 1, NULL),
           (3, "keyword3", 1, "post3"),
           (4, "keyword4", 2, NULL),
           (5, "keyword5", 3, "post5"),
           (6, "keyword6", 3, "post6")
  `);
  await db.close();
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_UPGRADED
  );

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal(await db.getSchemaVersion(), CURRENT_SCHEMA_VERSION);
});

add_task(async function check_keywords() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT k.id, keyword, post_data, foreign_count
    FROM moz_keywords k
    JOIN moz_places h ON k.place_id = h.id
    ORDER BY k.id ASC
  `);
  let entries = rows.map(r => ({
    id: r.getResultByName("id"),
    keyword: r.getResultByName("keyword"),
    post_data: r.getResultByName("post_data"),
    foreign_count: r.getResultByName("foreign_count"),
  }));

  // Useful for debugging purposes.
  info(JSON.stringify(entries));
  Assert.deepEqual(entries, [
    {
      id: 2,
      keyword: "keyword2",
      post_data: "",
      foreign_count: 3, // 1 bookmark, 2 keywords
    },
    {
      id: 3,
      keyword: "keyword3",
      post_data: "post3",
      foreign_count: 3, // 1 bookmark, 2 keywords
    },
    {
      id: 4,
      keyword: "keyword4",
      post_data: "",
      foreign_count: 2, // 1 bookmark, 1 keywords
    },
    {
      id: 5,
      keyword: "keyword5",
      post_data: "post5",
      foreign_count: 3, // 1 bookmark, 2 keywords
    },
    {
      id: 6,
      keyword: "keyword6",
      post_data: "post6",
      foreign_count: 3, // 1 bookmark, 2 keywords
    },
  ]);
});
