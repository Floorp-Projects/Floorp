const NS_PLACES_INIT_COMPLETE_TOPIC = "places-init-complete";
let gLockedConn;

add_task(async function setup() {
  // Create a dummy places.sqlite and open an unshared connection on it
  let db = Services.dirsvc.get("ProfD", Ci.nsIFile);
  db.append("places.sqlite");
  gLockedConn = Services.storage.openUnsharedDatabase(db);
  Assert.ok(db.exists(), "The database should have been created");

  // We need an exclusive lock on the db
  gLockedConn.executeSimpleSQL("PRAGMA locking_mode = EXCLUSIVE");
  // Exclusive locking is lazy applied, we need to make a write to activate it
  gLockedConn.executeSimpleSQL("PRAGMA USER_VERSION = 1");
});

add_task(async function locked() {
  // Try to create history service while the db is locked.
  // It should be possible to create the service, but any method using the
  // database will fail.
  let resolved = false;
  let promiseComplete = promiseTopicObserved(NS_PLACES_INIT_COMPLETE_TOPIC)
                          .then(() => resolved = true);
  let history = Cc["@mozilla.org/browser/nav-history-service;1"]
                  .createInstance(Ci.nsINavHistoryService);
  // The notification shouldn't happen until something tries to use the database.
  await new Promise(resolve => do_timeout(100, resolve));
  Assert.equal(resolved, false, "The notification should not have been fired yet");
  // This will initialize the database.
  Assert.equal(history.databaseStatus, history.DATABASE_STATUS_LOCKED);
  await promiseComplete;

  // Close our connection and try to cleanup the file (could fail on Windows)
  gLockedConn.close();
  let db = Services.dirsvc.get("ProfD", Ci.nsIFile);
  db.append("places.sqlite");
  if (db.exists()) {
    try {
      db.remove(false);
    } catch (e) {
      info("Unable to remove dummy places.sqlite");
    }
  }
});
