Cu.import("resource://weave/engines/bookmarks.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

load("bookmark_setup.js");

// ----------------------------------------
// Test Logic
// ----------------------------------------

function FakeMicrosummaryService() {
  return {hasMicrosummary: function() { return false; }};
}

function run_test() {
  // -----
  // Setup
  // -----

  var syncTesting = new SyncTestingInfrastructure(BookmarksEngine);

  function freshEngineSync(cb) {
    let engine = new BookmarksEngine();
    engine._store.__ms = new FakeMicrosummaryService();
    engine.sync(cb);
  };

  let bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
    getService(Ci.nsINavBookmarksService);

  cleanUp();

  // -----------
  // Test Proper
  // -----------

  let boogleBm = bms.insertBookmark(bms.bookmarksMenuFolder,
                                    uri("http://www.boogle.com"),
                                    -1,
                                    "Boogle");
  bms.setItemGUID(boogleBm, "boogle-bookmark-guid");

  syncTesting.runAsyncFunc("initial sync w/ one bookmark", freshEngineSync);

  syncTesting.runAsyncFunc("trivial re-sync", freshEngineSync);

  let yoogleBm = bms.insertBookmark(bms.bookmarksMenuFolder,
                                    uri("http://www.yoogle.com"),
                                    -1,
                                    "Yoogle");
  bms.setItemGUID(yoogleBm, "yoogle-bookmark-guid");

  syncTesting.runAsyncFunc("add bookmark and re-sync", freshEngineSync);

  bms.moveItem(yoogleBm,
               bms.bookmarksMenuFolder,
               0);

  syncTesting.runAsyncFunc("swap bookmark order and re-sync",
                           freshEngineSync);

  syncTesting.saveClientState("first computer");

  syncTesting.resetClientState();

  syncTesting.runAsyncFunc("re-sync on second computer", freshEngineSync);

  let zoogleBm = bms.insertBookmark(bms.bookmarksMenuFolder,
                                    uri("http://www.zoogle.com"),
                                    -1,
                                    "Zoogle");
  bms.setItemGUID(zoogleBm, "zoogle-bookmark-guid");

  syncTesting.runAsyncFunc("add bookmark on second computer and resync",
                           freshEngineSync);

  syncTesting.restoreClientState("first computer");
  syncTesting.runAsyncFunc("re-sync on first computer", freshEngineSync);

  // --------
  // Teardown
  // --------

  cleanUp();
}
