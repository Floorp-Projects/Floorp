/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
