Cu.import("resource://weave/engines/bookmarks.js");

// ----------------------------------------
// Test Logic
// ----------------------------------------

function run_test() {
  var syncTesting = new SyncTestingInfrastructure();

  function freshEngineSync(cb) {
    let engine = new BookmarksEngine();
    engine.sync(cb);
  };

  syncTesting.runAsyncFunc("initial sync", freshEngineSync);

  syncTesting.runAsyncFunc("trivial re-sync", freshEngineSync);
}
