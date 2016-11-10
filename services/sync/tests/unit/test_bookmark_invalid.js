Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

Service.engineManager.register(BookmarksEngine);

var engine = Service.engineManager.get("bookmarks");
var store = engine._store;
var tracker = engine._tracker;

add_task(async function test_ignore_invalid_uri() {
  _("Ensure that we don't die with invalid bookmarks.");

  // First create a valid bookmark.
  let bmid = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                  Services.io.newURI("http://example.com/", null, null),
                                                  PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                  "the title");

  // Now update moz_places with an invalid url.
  await PlacesUtils.withConnectionWrapper("test_ignore_invalid_uri", async function(db) {
    await db.execute(
      `UPDATE moz_places SET url = :url, url_hash = hash(:url)
       WHERE id = (SELECT b.fk FROM moz_bookmarks b
       WHERE b.id = :id LIMIT 1)`,
      { id: bmid, url: "<invalid url>" });
  });

  // Ensure that this doesn't throw even though the DB is now in a bad state (a
  // bookmark has an illegal url).
  engine._buildGUIDMap();
});

add_task(async function test_ignore_missing_uri() {
  _("Ensure that we don't die with a bookmark referencing an invalid bookmark id.");

  // First create a valid bookmark.
  let bmid = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                  Services.io.newURI("http://example.com/", null, null),
                                                  PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                  "the title");

  // Now update moz_bookmarks to reference a non-existing places ID
  await PlacesUtils.withConnectionWrapper("test_ignore_missing_uri", async function(db) {
    await db.execute(
      `UPDATE moz_bookmarks SET fk = 999999
       WHERE id = :id`
      , { id: bmid });
  });

  // Ensure that this doesn't throw even though the DB is now in a bad state (a
  // bookmark has an illegal url).
  engine._buildGUIDMap();
});

function run_test() {
  initTestLogging('Trace');
  run_next_test();
}
