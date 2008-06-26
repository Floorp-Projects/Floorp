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
  var syncTesting = new SyncTestingInfrastructure();

  function freshEngineSync(cb) {
    let engine = new BookmarksEngine();
    engine._store.__ms = new FakeMicrosummaryService();
    engine.sync(cb);
  };

  function resetProfile() {
    syncTesting.fakeFilesystem.fakeContents = {};
    let engine = new BookmarksEngine();
    engine._store.wipe();
  }

  function saveClientState() {
    return Utils.deepCopy(syncTesting.fakeFilesystem.fakeContents);
  }

  function restoreClientState(state, label) {
    function _restoreState() {
      let self = yield;

      syncTesting.fakeFilesystem.fakeContents = Utils.deepCopy(state);
      let engine = new BookmarksEngine();
      engine._store.wipe();
      let originalSnapshot = Utils.deepCopy(engine._store.wrap());
      engine._snapshot.load();
      let snapshot = engine._snapshot.data;

      engine._core.detectUpdates(self.cb, originalSnapshot, snapshot);
      let commands = yield;

      engine._store.applyCommands.async(engine._store, self.cb, commands);
      yield;
    }

    function restoreState(cb) {
      _restoreState.async(this, cb);
    }

    syncTesting.runAsyncFunc("restore client state of " + label,
                             restoreState);
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

  var firstComputerState = saveClientState();

  resetProfile();

  syncTesting.runAsyncFunc("re-sync on second computer", freshEngineSync);

  let zoogleBm = bms.insertBookmark(bms.bookmarksMenuFolder,
                                    uri("http://www.zoogle.com"),
                                    -1,
                                    "Zoogle");
  bms.setItemGUID(zoogleBm, "zoogle-bookmark-guid");

  syncTesting.runAsyncFunc("add bookmark on second computer and resync",
                           freshEngineSync);

  restoreClientState(firstComputerState, "first computer");
  syncTesting.runAsyncFunc("re-sync on first computer", freshEngineSync);

  cleanUp();
}
