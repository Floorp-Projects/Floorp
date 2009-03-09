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
 * The Original Code is Content Preferences (cpref).
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
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

function run_test() {
  //**************************************************************************//
  // Database Creation, Schema Migration, and Backup

  // Note: in these tests we use createInstance instead of getService
  // so we can instantiate the service multiple times and make it run
  // its database initialization code each time.

  // Create a new database.
  {
    ContentPrefTest.deleteDatabase();

    // Get the service and make sure it has a ready database connection.
    let cps = Cc["@mozilla.org/content-pref/service;1"].
              createInstance(Ci.nsIContentPrefService);
    do_check_true(cps.DBConnection.connectionReady);
    cps.DBConnection.close();
  }

  // Open an existing database.
  {
    let dbFile = ContentPrefTest.deleteDatabase();

    let cps = Cc["@mozilla.org/content-pref/service;1"].
               createInstance(Ci.nsIContentPrefService);
    cps.DBConnection.close();
    do_check_true(dbFile.exists());

    // Get the service and make sure it has a ready database connection.
    cps = Cc["@mozilla.org/content-pref/service;1"].
          createInstance(Ci.nsIContentPrefService);
    do_check_true(cps.DBConnection.connectionReady);
    cps.DBConnection.close();
  }

  // Open an empty database.
  {
    let dbFile = ContentPrefTest.deleteDatabase();

    // Create an empty database.
    let dbService = Cc["@mozilla.org/storage/service;1"].
                    getService(Ci.mozIStorageService);
    let dbConnection = dbService.openDatabase(dbFile);
    do_check_eq(dbConnection.schemaVersion, 0);
    dbConnection.close();
    do_check_true(dbFile.exists());

    // Get the service and make sure it has created the schema.
    let cps = Cc["@mozilla.org/content-pref/service;1"].
              createInstance(Ci.nsIContentPrefService);
    do_check_neq(cps.DBConnection.schemaVersion, 0);
    cps.DBConnection.close();
  }

  // Open a corrupted database.
  {
    let dbFile = ContentPrefTest.deleteDatabase();
    let backupDBFile = ContentPrefTest.deleteBackupDatabase();

    // Create a corrupted database.
    let foStream = Cc["@mozilla.org/network/file-output-stream;1"].
                   createInstance(Ci.nsIFileOutputStream);
    foStream.init(dbFile, 0x02 | 0x08 | 0x20, 0666, 0);
    let garbageData = "garbage that makes SQLite think the file is corrupted";
    foStream.write(garbageData, garbageData.length);
    foStream.close();

    // Get the service and make sure it backs up and recreates the database.
    let cps = Cc["@mozilla.org/content-pref/service;1"].
              createInstance(Ci.nsIContentPrefService);
    do_check_true(backupDBFile.exists());
    do_check_true(cps.DBConnection.connectionReady);

    cps.DBConnection.close();
  }

  // Open a database with a corrupted schema.
  {
    let dbFile = ContentPrefTest.deleteDatabase();
    let backupDBFile = ContentPrefTest.deleteBackupDatabase();

    // Create an empty database and set the schema version to a number
    // that will trigger a schema migration that will fail.
    let dbService = Cc["@mozilla.org/storage/service;1"].
                    getService(Ci.mozIStorageService);
    let dbConnection = dbService.openDatabase(dbFile);
    dbConnection.schemaVersion = -1;
    dbConnection.close();
    do_check_true(dbFile.exists());

    // Get the service and make sure it backs up and recreates the database.
    let cps = Cc["@mozilla.org/content-pref/service;1"].
              createInstance(Ci.nsIContentPrefService);
    do_check_true(backupDBFile.exists());
    do_check_true(cps.DBConnection.connectionReady);

    cps.DBConnection.close();
  }


  // Now get the content pref service for real for use by the rest of the tests.
  var cps = Cc["@mozilla.org/content-pref/service;1"].
            getService(Ci.nsIContentPrefService);

  var uri = ContentPrefTest.getURI("http://www.example.com/");

  // Make sure disk synchronization checking is turned off by default.
  var statement = cps.DBConnection.createStatement("PRAGMA synchronous");
  statement.executeStep();
  do_check_eq(0, statement.getInt32(0));

  //**************************************************************************//
  // Nonexistent Pref

  do_check_eq(cps.getPref(uri, "test.nonexistent.getPref"), undefined);
  do_check_eq(cps.setPref(uri, "test.nonexistent.setPref", 5), undefined);
  do_check_false(cps.hasPref(uri, "test.nonexistent.hasPref"));
  do_check_eq(cps.removePref(uri, "test.nonexistent.removePref"), undefined);


  //**************************************************************************//
  // Existing Pref

  cps.setPref(uri, "test.existing", 5);

  // getPref should return the pref value
  do_check_eq(cps.getPref(uri, "test.existing"), 5);

  // setPref should return undefined and change the value of the pref
  do_check_eq(cps.setPref(uri, "test.existing", 6), undefined);
  do_check_eq(cps.getPref(uri, "test.existing"), 6);

  // hasPref should return true
  do_check_true(cps.hasPref(uri, "test.existing"));

  // removePref should return undefined and remove the pref
  do_check_eq(cps.removePref(uri, "test.existing"), undefined);
  do_check_false(cps.hasPref(uri, "test.existing"));


  //**************************************************************************//
  // Round-Trip Data Integrity

  // Make sure pref values remain the same from setPref to getPref.

  cps.setPref(uri, "test.data-integrity.integer", 5);
  do_check_eq(cps.getPref(uri, "test.data-integrity.integer"), 5);

  cps.setPref(uri, "test.data-integrity.float", 5.5);
  do_check_eq(cps.getPref(uri, "test.data-integrity.float"), 5.5);

  cps.setPref(uri, "test.data-integrity.boolean", true);
  do_check_eq(cps.getPref(uri, "test.data-integrity.boolean"), true);

  cps.setPref(uri, "test.data-integrity.string", "test");
  do_check_eq(cps.getPref(uri, "test.data-integrity.string"), "test");

  cps.setPref(uri, "test.data-integrity.null", null);
  do_check_eq(cps.getPref(uri, "test.data-integrity.null"), null);

  // XXX Test arbitrary binary data.

  // Make sure hasPref and removePref work on all data types.

  do_check_true(cps.hasPref(uri, "test.data-integrity.integer"));
  do_check_true(cps.hasPref(uri, "test.data-integrity.float"));
  do_check_true(cps.hasPref(uri, "test.data-integrity.boolean"));
  do_check_true(cps.hasPref(uri, "test.data-integrity.string"));
  do_check_true(cps.hasPref(uri, "test.data-integrity.null"));

  do_check_eq(cps.removePref(uri, "test.data-integrity.integer"), undefined);
  do_check_eq(cps.removePref(uri, "test.data-integrity.float"), undefined);
  do_check_eq(cps.removePref(uri, "test.data-integrity.boolean"), undefined);
  do_check_eq(cps.removePref(uri, "test.data-integrity.string"), undefined);
  do_check_eq(cps.removePref(uri, "test.data-integrity.null"), undefined);

  do_check_false(cps.hasPref(uri, "test.data-integrity.integer"));
  do_check_false(cps.hasPref(uri, "test.data-integrity.float"));
  do_check_false(cps.hasPref(uri, "test.data-integrity.boolean"));
  do_check_false(cps.hasPref(uri, "test.data-integrity.string"));
  do_check_false(cps.hasPref(uri, "test.data-integrity.null"));


  //**************************************************************************//
  // getPrefs
  
  cps.setPref(uri, "test.getPrefs.a", 1);
  cps.setPref(uri, "test.getPrefs.b", 2);
  cps.setPref(uri, "test.getPrefs.c", 3);

  var prefs = cps.getPrefs(uri);
  do_check_true(prefs.hasKey("test.getPrefs.a"));
  do_check_eq(prefs.get("test.getPrefs.a"), 1);
  do_check_true(prefs.hasKey("test.getPrefs.b"));
  do_check_eq(prefs.get("test.getPrefs.b"), 2);
  do_check_true(prefs.hasKey("test.getPrefs.c"));
  do_check_eq(prefs.get("test.getPrefs.c"), 3);


  //**************************************************************************//
  // Site-Specificity

  // These are all different sites, and setting a pref for one of them
  // shouldn't set it for the others.
  var uri1 = ContentPrefTest.getURI("http://www.domain1.com/");
  var uri2 = ContentPrefTest.getURI("http://foo.domain1.com/");
  var uri3 = ContentPrefTest.getURI("http://domain1.com/");
  var uri4 = ContentPrefTest.getURI("http://www.domain2.com/");

  cps.setPref(uri1, "test.site-specificity.uri1", 5);
  do_check_false(cps.hasPref(uri2, "test.site-specificity.uri1"));
  do_check_false(cps.hasPref(uri3, "test.site-specificity.uri1"));
  do_check_false(cps.hasPref(uri4, "test.site-specificity.uri1"));

  cps.setPref(uri2, "test.site-specificity.uri2", 5);
  do_check_false(cps.hasPref(uri1, "test.site-specificity.uri2"));
  do_check_false(cps.hasPref(uri3, "test.site-specificity.uri2"));
  do_check_false(cps.hasPref(uri4, "test.site-specificity.uri2"));

  cps.setPref(uri3, "test.site-specificity.uri3", 5);
  do_check_false(cps.hasPref(uri1, "test.site-specificity.uri3"));
  do_check_false(cps.hasPref(uri2, "test.site-specificity.uri3"));
  do_check_false(cps.hasPref(uri4, "test.site-specificity.uri3"));

  cps.setPref(uri4, "test.site-specificity.uri4", 5);
  do_check_false(cps.hasPref(uri1, "test.site-specificity.uri4"));
  do_check_false(cps.hasPref(uri2, "test.site-specificity.uri4"));
  do_check_false(cps.hasPref(uri3, "test.site-specificity.uri4"));


  //**************************************************************************//
  // Observers

  var specificObserver = {
    interfaces: [Ci.nsIContentPrefObserver, Ci.nsISupports],
  
    QueryInterface: function ContentPrefTest_QueryInterface(iid) {
      if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
        throw Cr.NS_ERROR_NO_INTERFACE;
      return this;
    },

    numTimesSetCalled: 0,
    onContentPrefSet: function specificObserver_onContentPrefSet(group, name, value) {
      ++this.numTimesSetCalled;
      do_check_eq(group, "www.example.com");
      do_check_eq(name, "test.observer.1");
      do_check_eq(value, "test value");
    },

    numTimesRemovedCalled: 0,
    onContentPrefRemoved: function specificObserver_onContentPrefRemoved(group, name) {
      ++this.numTimesRemovedCalled;
      do_check_eq(group, "www.example.com");
      do_check_eq(name, "test.observer.1");
    }

  };

  var genericObserver = {
    interfaces: [Ci.nsIContentPrefObserver, Ci.nsISupports],
  
    QueryInterface: function ContentPrefTest_QueryInterface(iid) {
      if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
        throw Cr.NS_ERROR_NO_INTERFACE;
      return this;
    },

    numTimesSetCalled: 0,
    onContentPrefSet: function genericObserver_onContentPrefSet(group, name, value) {
      ++this.numTimesSetCalled;
      do_check_eq(group, "www.example.com");
      if (name != "test.observer.1" && name != "test.observer.2")
        do_throw("genericObserver.onContentPrefSet: " +
                 "name not in (test.observer.1, test.observer.2)");
      do_check_eq(value, "test value");
    },

    numTimesRemovedCalled: 0,
    onContentPrefRemoved: function genericObserver_onContentPrefRemoved(group, name) {
      ++this.numTimesRemovedCalled;
      do_check_eq(group, "www.example.com");
      if (name != "test.observer.1" && name != "test.observer.2")
        do_throw("genericObserver.onContentPrefSet: " +
                 "name not in (test.observer.1, test.observer.2)");
    }

  };

  // Make sure we can add observers, observers get notified about changes,
  // specific observers only get notified about changes to the specific setting,
  // and generic observers get notified about changes to all settings.
  cps.addObserver("test.observer.1", specificObserver);
  cps.addObserver(null, genericObserver);
  cps.setPref(uri, "test.observer.1", "test value");
  cps.setPref(uri, "test.observer.2", "test value");
  cps.removePref(uri, "test.observer.1");
  cps.removePref(uri, "test.observer.2");
  do_check_eq(specificObserver.numTimesSetCalled, 1);
  do_check_eq(genericObserver.numTimesSetCalled, 2);
  do_check_eq(specificObserver.numTimesRemovedCalled, 1);
  do_check_eq(genericObserver.numTimesRemovedCalled, 2);

  // Make sure we can remove observers and they don't get notified
  // about changes anymore.
  cps.removeObserver("test.observer.1", specificObserver);
  cps.removeObserver(null, genericObserver);
  cps.setPref(uri, "test.observer.1", "test value");
  cps.removePref(uri, "test.observer.1", "test value");
  do_check_eq(specificObserver.numTimesSetCalled, 1);
  do_check_eq(genericObserver.numTimesSetCalled, 2);
  do_check_eq(specificObserver.numTimesRemovedCalled, 1);
  do_check_eq(genericObserver.numTimesRemovedCalled, 2);


  //**************************************************************************//
  // Clear Private Data Pref Removal

  {
    let uri1 = ContentPrefTest.getURI("http://www.domain1.com/");
    let uri2 = ContentPrefTest.getURI("http://www.domain2.com/");
    let uri3 = ContentPrefTest.getURI("http://www.domain3.com/");

    let dbConnection = cps.DBConnection;

    let prefCount = Cc["@mozilla.org/storage/statement-wrapper;1"].
                    createInstance(Ci.mozIStorageStatementWrapper);
    prefCount.initialize(dbConnection.createStatement("SELECT COUNT(*) AS count FROM prefs"));

    let groupCount = Cc["@mozilla.org/storage/statement-wrapper;1"].
                     createInstance(Ci.mozIStorageStatementWrapper);
    groupCount.initialize(dbConnection.createStatement("SELECT COUNT(*) AS count FROM groups"));

    // Add some prefs for multiple domains.
    cps.setPref(uri1, "test.removeAllGroups", 1);
    cps.setPref(uri2, "test.removeAllGroups", 2);
    cps.setPref(uri3, "test.removeAllGroups", 3);

    // Add a global pref.
    cps.setPref(null, "test.removeAllGroups", 1);

    // Make sure there are some prefs and groups in the database.
    prefCount.step();
    do_check_true(prefCount.row.count > 0);
    prefCount.reset();
    groupCount.step();
    do_check_true(groupCount.row.count > 0);
    groupCount.reset();

    // Remove all prefs and groups from the database using the same routine
    // the Clear Private Data dialog uses.
    cps.removeGroupedPrefs();

    // Make sure there are no longer any groups in the database and the only pref
    // is the global one.
    prefCount.step();
    do_check_true(prefCount.row.count == 1);
    prefCount.reset();
    groupCount.step();
    do_check_true(groupCount.row.count == 0);
    groupCount.reset();
    let globalPref = Cc["@mozilla.org/storage/statement-wrapper;1"].
                     createInstance(Ci.mozIStorageStatementWrapper);
    globalPref.initialize(dbConnection.createStatement("SELECT groupID FROM prefs"));
    globalPref.step();
    do_check_true(globalPref.row.groupID == null);
    globalPref.reset();
  }
}
