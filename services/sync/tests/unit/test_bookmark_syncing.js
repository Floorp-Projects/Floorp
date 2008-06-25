Cu.import("resource://weave/engines/bookmarks.js");

load("bookmark_setup.js");

// ----------------------------------------
// Test Logic
// ----------------------------------------

function FakeMicrosummaryService() {
  return {hasMicrosummary: function() { return false; }};
}

function run_test() {
  var syncTesting = new SyncTestingInfrastructure();

  function freshEngineSync(cb) {
    let engine = new BookmarksEngine();
    engine._store.__ms = new FakeMicrosummaryService();
    engine.sync(cb);
  };

  function resetProfile() {
    // Simulate going to another computer by removing stuff from our
    // objects.
    syncTesting.fakeFilesystem.fakeContents = {};
    bms.removeItem(boogleBm);
    bms.removeItem(yoogleBm);
  }

  let bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
    getService(Ci.nsINavBookmarksService);

  cleanUp();

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

  resetProfile();

  syncTesting.runAsyncFunc("re-sync on second computer", freshEngineSync);

  cleanUp();
}
