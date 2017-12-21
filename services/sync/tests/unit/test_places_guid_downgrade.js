/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/history.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/service.js");

const kDBName = "places.sqlite";

function setPlacesDatabase(aFileName) {
  removePlacesDatabase();
  _("Copying over places.sqlite.");
  let file = do_get_file(aFileName);
  file.copyTo(gSyncProfile, kDBName);
}

function removePlacesDatabase() {
  _("Removing places.sqlite.");
  let file = gSyncProfile.clone();
  file.append(kDBName);
  try {
    file.remove(false);
  } catch (ex) {
    // Windows is awesome. NOT.
  }
}

Svc.Obs.add("places-shutdown", function() {
  do_timeout(0, removePlacesDatabase);
});


// Verify initial database state. Function borrowed from places tests.
add_test(function test_initial_state() {
  _("Verify initial setup: v11 database is available");

  // Mostly sanity checks our starting DB to make sure it's setup as we expect
  // it to be.
  let dbFile = gSyncProfile.clone();
  dbFile.append(kDBName);
  let db = Services.storage.openUnsharedDatabase(dbFile);

  let stmt = db.createStatement("PRAGMA journal_mode");
  Assert.ok(stmt.executeStep());
  // WAL journal mode should have been unset this database when it was migrated
  // down to v10.
  Assert.notEqual(stmt.getString(0).toLowerCase(), "wal");
  stmt.finalize();

  Assert.ok(db.indexExists("moz_bookmarks_guid_uniqueindex"));
  Assert.ok(db.indexExists("moz_places_guid_uniqueindex"));

  // There should be a non-zero amount of bookmarks without a guid.
  stmt = db.createStatement(
    "SELECT COUNT(1) "
  + "FROM moz_bookmarks "
  + "WHERE guid IS NULL "
  );
  Assert.ok(stmt.executeStep());
  Assert.notEqual(stmt.getInt32(0), 0);
  stmt.finalize();

  // There should be a non-zero amount of places without a guid.
  stmt = db.createStatement(
    "SELECT COUNT(1) "
  + "FROM moz_places "
  + "WHERE guid IS NULL "
  );
  Assert.ok(stmt.executeStep());
  Assert.notEqual(stmt.getInt32(0), 0);
  stmt.finalize();

  // Check our schema version to make sure it is actually at 10.
  Assert.equal(db.schemaVersion, 10);

  db.close();

  run_next_test();
});

add_task(async function test_history_guids() {
  let engine = new HistoryEngine(Service);
  await engine.initialize();
  let store = engine._store;

  let places = [
    {
      url: "http://getfirefox.com/",
      title: "Get Firefox!",
      visits: [{
        date: new Date(),
        transition: Ci.nsINavHistoryService.TRANSITION_LINK
      }]
    },
    {
      url: "http://getthunderbird.com/",
      title: "Get Thunderbird!",
      visits: [{
        date: new Date(),
        transition: Ci.nsINavHistoryService.TRANSITION_LINK
      }]
    }
  ];

  async function onVisitAdded() {
    let fxguid = await store.GUIDForUri("http://getfirefox.com/", true);
    let tbguid = await store.GUIDForUri("http://getthunderbird.com/", true);
    dump("fxguid: " + fxguid + "\n");
    dump("tbguid: " + tbguid + "\n");

    _("History: Verify GUIDs are added to the guid column.");
    let db = await PlacesUtils.promiseDBConnection();
    let result = await db.execute(
      "SELECT id FROM moz_places WHERE guid = :guid",
      {guid: fxguid}
    );
    Assert.equal(result.length, 1);

    result = await db.execute(
      "SELECT id FROM moz_places WHERE guid = :guid",
      {guid: tbguid}
    );
    Assert.equal(result.length, 1);

    _("History: Verify GUIDs weren't added to annotations.");
    result = await db.execute(
      "SELECT a.content AS guid FROM moz_annos a WHERE guid = :guid",
      {guid: fxguid}
    );
    Assert.equal(result.length, 0);

    result = await db.execute(
      "SELECT a.content AS guid FROM moz_annos a WHERE guid = :guid",
      {guid: tbguid}
    );
    Assert.equal(result.length, 0);
  }

  await PlacesUtils.history.insertMany(places);
  await onVisitAdded();
});

add_task(async function test_bookmark_guids() {
  let fx = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://getfirefox.com/",
    title: "Get Firefox!",
  });
  let fxid = await PlacesUtils.promiseItemId(fx.guid);
  let tb = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://getthunderbird.com/",
    title: "Get Thunderbird!",
  });
  let tbid = await PlacesUtils.promiseItemId(tb.guid);

  _("Bookmarks: Verify GUIDs are added to the guid column.");
  let db = await PlacesUtils.promiseDBConnection();
  let result = await db.execute(
    "SELECT id FROM moz_bookmarks WHERE guid = :guid",
    {guid: fx.guid}
  );
  Assert.equal(result.length, 1);
  Assert.equal(result[0].getResultByName("id"), fxid);

  result = await db.execute(
    "SELECT id FROM moz_bookmarks WHERE guid = :guid",
    {guid: tb.guid}
  );
  Assert.equal(result.length, 1);
  Assert.equal(result[0].getResultByName("id"), tbid);

  _("Bookmarks: Verify GUIDs weren't added to annotations.");
  result = await db.execute(
    "SELECT a.content AS guid FROM moz_items_annos a WHERE guid = :guid",
    {guid: fx.guid}
  );
  Assert.equal(result.length, 0);

  result = await db.execute(
    "SELECT a.content AS guid FROM moz_items_annos a WHERE guid = :guid",
    {guid: tb.guid}
  );
  Assert.equal(result.length, 0);
});

function run_test() {
  setPlacesDatabase("places_v10_from_v11.sqlite");

  run_next_test();
}
