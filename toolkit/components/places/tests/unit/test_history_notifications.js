const NS_PLACES_INIT_COMPLETE_TOPIC = "places-init-complete";
const NS_PLACES_DATABASE_LOCKED_TOPIC = "places-database-locked";

add_task(function* () {
  // Create a dummy places.sqlite and open an unshared connection on it
  let db = Services.dirsvc.get('ProfD', Ci.nsIFile);
  db.append("places.sqlite");
  let dbConn = Services.storage.openUnsharedDatabase(db);
  Assert.ok(db.exists(), "The database should have been created");

  // We need an exclusive lock on the db
  dbConn.executeSimpleSQL("PRAGMA locking_mode = EXCLUSIVE");
  // Exclusive locking is lazy applied, we need to make a write to activate it
  dbConn.executeSimpleSQL("PRAGMA USER_VERSION = 1");

  // Try to create history service while the db is locked
  let promiseLocked = promiseTopicObserved(NS_PLACES_DATABASE_LOCKED_TOPIC);
  Assert.throws(() => Cc["@mozilla.org/browser/nav-history-service;1"]
                        .getService(Ci.nsINavHistoryService),
                /NS_ERROR_XPC_GS_RETURNED_FAILURE/);
  yield promiseLocked;

  // Close our connection and try to cleanup the file (could fail on Windows)
  dbConn.close();
  if (db.exists()) {
    try {
      db.remove(false);
    } catch (e) {
      do_print("Unable to remove dummy places.sqlite");
    }
  }

  // Create history service correctly
  let promiseComplete = promiseTopicObserved(NS_PLACES_INIT_COMPLETE_TOPIC);
  Cc["@mozilla.org/browser/nav-history-service;1"]
    .getService(Ci.nsINavHistoryService);
  yield promiseComplete;
});
