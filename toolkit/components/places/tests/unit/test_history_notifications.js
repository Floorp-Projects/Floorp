/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const NS_PLACES_INIT_COMPLETE_TOPIC = "places-init-complete";
const NS_PLACES_DATABASE_LOCKED_TOPIC = "places-database-locked";

function run_test() {
  do_test_pending();

  // Create an observer for the Places notifications
  var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
  var observer = {
    _lockedNotificationReceived: false,
    observe: function thn_observe(aSubject, aTopic, aData)
    {
      switch (aTopic) {
        case NS_PLACES_INIT_COMPLETE_TOPIC:
          do_check_true(this._lockedNotificationReceived);
          os.removeObserver(this, NS_PLACES_INIT_COMPLETE_TOPIC);
          os.removeObserver(this, NS_PLACES_DATABASE_LOCKED_TOPIC);
          do_test_finished();
          break;
        case NS_PLACES_DATABASE_LOCKED_TOPIC:
          if (this._lockedNotificationReceived)
            do_throw("Locked notification should be observed only one time");
          this._lockedNotificationReceived = true;
          break;
      }
    }
  };
  os.addObserver(observer, NS_PLACES_INIT_COMPLETE_TOPIC, false);
  os.addObserver(observer, NS_PLACES_DATABASE_LOCKED_TOPIC, false);

  // Create a dummy places.sqlite and open an unshared connection on it
  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  var db = dirSvc.get('ProfD', Ci.nsIFile);
  db.append("places.sqlite");
  var storage = Cc["@mozilla.org/storage/service;1"].
                getService(Ci.mozIStorageService);
  var dbConn = storage.openUnsharedDatabase(db);
  do_check_true(db.exists());

  // We need an exclusive lock on the db
  dbConn.executeSimpleSQL("PRAGMA locking_mode = EXCLUSIVE");
  // Exclusive locking is lazy applied, we need to make a write to activate it
  dbConn.executeSimpleSQL("PRAGMA USER_VERSION = 1");

  // Try to create history service while the db is locked
  try {
    var hs1 = Cc["@mozilla.org/browser/nav-history-service;1"].
              getService(Ci.nsINavHistoryService);
    do_throw("Creating an instance of history service on a locked db should throw");
  } catch (ex) {}

  // Close our connection and try to cleanup the file (could fail on Windows)
  dbConn.close();
  if (db.exists()) {
    try {
      db.remove(false);
    } catch(e) { dump("Unable to remove dummy places.sqlite"); }
  }

  // Create history service correctly
  try {
    var hs2 = Cc["@mozilla.org/browser/nav-history-service;1"].
              getService(Ci.nsINavHistoryService);
  } catch (ex) {
    do_throw("Creating an instance of history service on a not locked db should not throw");
  }
}
