/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPECTED_REMAINING_ROOTS = [
  ...PlacesUtils.bookmarks.userContentRoots,
  PlacesUtils.bookmarks.tagsGuid,
];

const EXPECTED_REMOVED_BOOKMARK_GUIDS = [
  // These first ones are the old left-pane folder queries
  "SNLmwJH6GtW9", // Root Query
  "r0dY_2_y4mlx", // History
  "xGGhZK3b6GnW", // Downloads
  "EJG6I1nKkQFQ", // Tags
  "gSyHo5oNSUJV", // All Bookmarks
  // These are simulated add-on injections that we expect to be removed.
  "exaddon_____",
  "exaddon1____",
  "exaddon2____",
  "exaddon3____",
  "test________",
];

const EXPECTED_REMOVED_ANNOTATIONS = [
  "PlacesOrganizer/OrganizerFolder",
  "PlacesOrganizer/OrganizerQuery",
];

const EXPECTED_REMOVED_PLACES_ENTRIES = ["exaddonh____", "exaddonh3___"];
const EXPECTED_KEPT_PLACES_ENTRY = "exaddonh2___";
const EXPECTED_REMOVED_KEYWORDS = ["exaddon", "exaddon2"];

async function assertItemIn(db, table, field, expectedItems) {
  let rows = await db.execute(`SELECT ${field} from ${table}`);

  Assert.ok(rows.length >= expectedItems.length,
    "Should be at least the number of annotations we expect to be removed.");

  let fieldValues = rows.map(row => row.getResultByName(field));

  for (let item of expectedItems) {
    Assert.ok(fieldValues.includes(item),
      `${table} should have ${expectedItems}`);
  }
}

add_task(async function setup() {
  await setupPlacesDatabase("places_v43.sqlite");

  // Setup database contents to be migrated.
  let path = OS.Path.join(OS.Constants.Path.profileDir, DB_FILENAME);
  let db = await Sqlite.openConnection({ path });

  let rows = await db.execute(`SELECT * FROM moz_bookmarks_deleted`);
  Assert.equal(rows.length, 0, "Should be nothing in moz_bookmarks_deleted");


  // Break roots parenting, to test for Bug 1472127.
  await db.execute(`INSERT INTO moz_bookmarks (title, parent, position, guid)
                    VALUES ("test", 1, 0, "test________")`);
  await db.execute(`UPDATE moz_bookmarks
                    SET parent = (SELECT id FROM moz_bookmarks WHERE guid = "test________")
                    WHERE guid = "menu________"`);

  await assertItemIn(db, "moz_anno_attributes", "name", EXPECTED_REMOVED_ANNOTATIONS);
  await assertItemIn(db, "moz_bookmarks", "guid", EXPECTED_REMOVED_BOOKMARK_GUIDS);
  await assertItemIn(db, "moz_keywords", "keyword", EXPECTED_REMOVED_KEYWORDS);
  await assertItemIn(db, "moz_places", "guid", EXPECTED_REMOVED_PLACES_ENTRIES);

  await db.close();
});

add_task(async function database_is_valid() {
  // Accessing the database for the first time triggers migration.
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_UPGRADED);

  let db = await PlacesUtils.promiseDBConnection();
  Assert.equal((await db.getSchemaVersion()), CURRENT_SCHEMA_VERSION);
});

add_task(async function test_roots_removed() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT id FROM moz_bookmarks
    WHERE guid = :guid
  `, {guid: PlacesUtils.bookmarks.rootGuid});
  Assert.equal(rows.length, 1, "Should have exactly one root row.");
  let rootId = rows[0].getResultByName("id");

  rows = await db.execute(`
    SELECT guid FROM moz_bookmarks
    WHERE parent = :rootId`, {rootId});

  Assert.equal(rows.length, EXPECTED_REMAINING_ROOTS.length,
    "Should only have the built-in folder roots.");

  for (let row of rows) {
    let guid = row.getResultByName("guid");
    Assert.ok(EXPECTED_REMAINING_ROOTS.includes(guid),
      `Should have only the expected guids remaining, unexpected guid: ${guid}`);
  }

  // Check the reparented menu now.
  rows = await db.execute(`
    SELECT id, parent FROM moz_bookmarks
    WHERE guid = :guid
  `, {guid: PlacesUtils.bookmarks.menuGuid});
  Assert.equal(rows.length, 1, "Should have found the menu root.");
  Assert.equal(rows[0].getResultByName("parent"), PlacesUtils.placesRootId,
    "Should have moved the menu back to the Places root.");
});

add_task(async function test_tombstones_added() {
  let db = await PlacesUtils.promiseDBConnection();

  let rows = await db.execute(`
    SELECT guid FROM moz_bookmarks_deleted
  `);

  for (let row of rows) {
    let guid = row.getResultByName("guid");
    Assert.ok(EXPECTED_REMOVED_BOOKMARK_GUIDS.includes(guid),
      `Should have tombstoned the expected guids, unexpected guid: ${guid}`);
  }

  Assert.equal(rows.length, EXPECTED_REMOVED_BOOKMARK_GUIDS.length,
    "Should have removed all the expected bookmarks.");
});

add_task(async function test_annotations_removed() {
  let db = await PlacesUtils.promiseDBConnection();

  await assertAnnotationsRemoved(db, EXPECTED_REMOVED_ANNOTATIONS);
});

add_task(async function test_check_history_entries() {
  let db = await PlacesUtils.promiseDBConnection();

  for (let entry of EXPECTED_REMOVED_PLACES_ENTRIES) {
    let rows = await db.execute(`
      SELECT id FROM moz_places
      WHERE guid = '${entry}'`);

    Assert.equal(rows.length, 0,
      `Should have removed an orphaned history entry ${EXPECTED_REMOVED_PLACES_ENTRIES}.`);
  }

  let rows = await db.execute(`
    SELECT foreign_count FROM moz_places
    WHERE guid = :guid
  `, {guid: EXPECTED_KEPT_PLACES_ENTRY});

  Assert.equal(rows.length, 1,
    `Should have kept visited history entry ${EXPECTED_KEPT_PLACES_ENTRY}`);

  let foreignCount = rows[0].getResultByName("foreign_count");
  Assert.equal(foreignCount, 0,
    `Should have updated the foreign_count for ${EXPECTED_KEPT_PLACES_ENTRY}`);
});

add_task(async function test_check_keyword_removed() {
  let db = await PlacesUtils.promiseDBConnection();

  for (let keyword of EXPECTED_REMOVED_KEYWORDS) {
    let rows = await db.execute(`
      SELECT keyword FROM moz_keywords
      WHERE keyword = :keyword
    `, {keyword});

    Assert.equal(rows.length, 0,
      `Should have removed the expected keyword: ${keyword}.`);
  }
});

add_task(async function test_no_orphan_annotations() {
  let db = await PlacesUtils.promiseDBConnection();

  await assertNoOrphanAnnotations(db);
});

add_task(async function test_no_orphan_keywords() {
  let db = await PlacesUtils.promiseDBConnection();

  let rows = await db.execute(`
    SELECT place_id FROM moz_keywords
    WHERE place_id NOT IN (SELECT id from moz_places)
  `);

  Assert.equal(rows.length, 0,
    `Should have no orphan keywords.`);
});

add_task(async function test_meta_exists() {
  let db = await PlacesUtils.promiseDBConnection();
  await db.execute(`SELECT 1 FROM moz_meta`);
});
