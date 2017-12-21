Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

let engine;
let store;
let tracker;

add_task(async function setup() {
  engine = new BookmarksEngine(Service);
  store = engine._store;
  tracker = engine._tracker;
});

add_task(async function test_ignore_invalid_uri() {
  _("Ensure that we don't die with invalid bookmarks.");

  // First create a valid bookmark.
  let bmInfo = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.com/",
    title: "the title",
  });

  // Now update moz_places with an invalid url.
  await PlacesUtils.withConnectionWrapper("test_ignore_invalid_uri", async function(db) {
    await db.execute(
      `UPDATE moz_places SET url = :url, url_hash = hash(:url)
       WHERE id = (SELECT b.fk FROM moz_bookmarks b
                   WHERE b.guid = :guid)`,
      { guid: bmInfo.guid, url: "<invalid url>" });
  });

  // Ensure that this doesn't throw even though the DB is now in a bad state (a
  // bookmark has an illegal url).
  await engine._buildGUIDMap();
});

add_task(async function test_ignore_missing_uri() {
  _("Ensure that we don't die with a bookmark referencing an invalid bookmark id.");

  // First create a valid bookmark.
  let bmInfo = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.com/",
    title: "the title",
  });

  // Now update moz_bookmarks to reference a non-existing places ID
  await PlacesUtils.withConnectionWrapper("test_ignore_missing_uri", async function(db) {
    await db.execute(
      `UPDATE moz_bookmarks SET fk = 999999
       WHERE guid = :guid`
      , { guid: bmInfo.guid });
  });

  // Ensure that this doesn't throw even though the DB is now in a bad state (a
  // bookmark has an illegal url).
  await engine._buildGUIDMap();
});
