/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines/bookmarks.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

Service.engineManager.register(BookmarksEngine);
let engine = Service.engineManager.get("bookmarks");
let store  = engine._store;
let tracker = engine._tracker;

store.wipe();
tracker.persistChangedIDs = false;

function test_tracking() {
  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
  do_check_empty(tracker.changedIDs);

  let folder = PlacesUtils.bookmarks.createFolder(
    PlacesUtils.bookmarks.bookmarksMenuFolder,
    "Test Folder", PlacesUtils.bookmarks.DEFAULT_INDEX);
  function createBmk() {
    return PlacesUtils.bookmarks.insertBookmark(
      folder, Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
  }

  try {
    _("Create bookmark. Won't show because we haven't started tracking yet");
    createBmk();
    do_check_empty(tracker.changedIDs);
    do_check_eq(tracker.score, 0);

    _("Tell the tracker to start tracking changes.");
    Svc.Obs.notify("weave:engine:start-tracking");
    createBmk();
    // We expect two changed items because the containing folder
    // changed as well (new child).
    do_check_attribute_count(tracker.changedIDs, 2);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:start-tracking");
    createBmk();
    do_check_attribute_count(tracker.changedIDs, 3);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 4);

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    tracker.resetScore();
    Svc.Obs.notify("weave:engine:stop-tracking");
    createBmk();
    do_check_empty(tracker.changedIDs);
    do_check_eq(tracker.score, 0);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    createBmk();
    do_check_empty(tracker.changedIDs);
    do_check_eq(tracker.score, 0);

  } finally {
    _("Clean up.");
    store.wipe();
    tracker.clearChangedIDs();
    tracker.resetScore();
    Svc.Obs.notify("weave:engine:stop-tracking");
  }
}

function test_onItemChanged() {
  // Anno that's in ANNOS_TO_TRACK.
  const DESCRIPTION_ANNO = "bookmarkProperties/description";

  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
  do_check_empty(tracker.changedIDs);
  do_check_eq(tracker.score, 0);

  try {
    Svc.Obs.notify("weave:engine:stop-tracking");
    let folder = PlacesUtils.bookmarks.createFolder(
      PlacesUtils.bookmarks.bookmarksMenuFolder, "Parent",
      PlacesUtils.bookmarks.DEFAULT_INDEX);
    _("Track changes to annos.");
    let b = PlacesUtils.bookmarks.insertBookmark(
      folder, Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");
    let bGUID = engine._store.GUIDForId(b);
    _("New item is " + b);
    _("GUID: " + bGUID);

    Svc.Obs.notify("weave:engine:start-tracking");
    PlacesUtils.annotations.setItemAnnotation(
      b, DESCRIPTION_ANNO, "A test description", 0,
      PlacesUtils.annotations.EXPIRE_NEVER);
    do_check_true(tracker.changedIDs[bGUID] > 0);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);

  } finally {
    _("Clean up.");
    store.wipe();
    tracker.clearChangedIDs();
    tracker.resetScore();
    Svc.Obs.notify("weave:engine:stop-tracking");
  }
}

function test_onItemMoved() {
  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
  do_check_empty(tracker.changedIDs);
  do_check_eq(tracker.score, 0);

  try {
    let fx_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getfirefox.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Firefox!");
    let fx_guid = engine._store.GUIDForId(fx_id);
    let tb_id = PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.bookmarks.bookmarksMenuFolder,
      Utils.makeURI("http://getthunderbird.com"),
      PlacesUtils.bookmarks.DEFAULT_INDEX,
      "Get Thunderbird!");
    let tb_guid = engine._store.GUIDForId(tb_id);

    Svc.Obs.notify("weave:engine:start-tracking");

    // Moving within the folder will just track the folder.
    PlacesUtils.bookmarks.moveItem(
      tb_id, PlacesUtils.bookmarks.bookmarksMenuFolder, 0);
    do_check_true(tracker.changedIDs['menu'] > 0);
    do_check_eq(tracker.changedIDs['toolbar'], undefined);
    do_check_eq(tracker.changedIDs[fx_guid], undefined);
    do_check_eq(tracker.changedIDs[tb_guid], undefined);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
    tracker.clearChangedIDs();
    tracker.resetScore();

    // Moving a bookmark to a different folder will track the old
    // folder, the new folder and the bookmark.
    PlacesUtils.bookmarks.moveItem(tb_id, PlacesUtils.bookmarks.toolbarFolder,
                                   PlacesUtils.bookmarks.DEFAULT_INDEX);
    do_check_true(tracker.changedIDs['menu'] > 0);
    do_check_true(tracker.changedIDs['toolbar'] > 0);
    do_check_eq(tracker.changedIDs[fx_guid], undefined);
    do_check_true(tracker.changedIDs[tb_guid] > 0);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 3);

  } finally {
    _("Clean up.");
    store.wipe();
    tracker.clearChangedIDs();
    tracker.resetScore();
    Svc.Obs.notify("weave:engine:stop-tracking");
  }

}

function run_test() {
  initTestLogging("Trace");

  Log.repository.getLogger("Sync.Engine.Bookmarks").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Store.Bookmarks").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Tracker.Bookmarks").level = Log.Level.Trace;

  test_tracking();
  test_onItemChanged();
  test_onItemMoved();
}

