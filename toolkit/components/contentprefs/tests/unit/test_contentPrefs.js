/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


add_task(async function() {
  // Database Creation, Schema Migration, and Backup

  // Note: in these tests we use createInstance instead of getService
  // so we can instantiate the service multiple times and make it run
  // its database initialization code each time.

  function with_cps_instance(testFn) {
    let cps = Cc["@mozilla.org/content-pref/service;1"]
                .createInstance(Ci.nsIContentPrefService)
                .QueryInterface(Ci.nsIObserver);
    testFn(cps);
    let promiseClosed = TestUtils.topicObserved("content-prefs-db-closed");
    cps.observe(null, "xpcom-shutdown", "");
    return promiseClosed;
  }

  // Create a new database.
  ContentPrefTest.deleteDatabase();
  await with_cps_instance(cps => {
    do_check_true(cps.DBConnection.connectionReady);
  });

  // Open an existing database.

  ContentPrefTest.deleteDatabase();
  await with_cps_instance(cps => {});

  // Get the service and make sure it has a ready database connection.
  await with_cps_instance(cps => {
    do_check_true(cps.DBConnection.connectionReady);
  });

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
    await with_cps_instance(cps => {
      do_check_neq(cps.DBConnection.schemaVersion, 0);
    });
  }

  // Open a corrupted database.
  {
    let dbFile = ContentPrefTest.deleteDatabase();
    let backupDBFile = ContentPrefTest.deleteBackupDatabase();

    // Create a corrupted database.
    let foStream = Cc["@mozilla.org/network/file-output-stream;1"].
                   createInstance(Ci.nsIFileOutputStream);
    foStream.init(dbFile, 0x02 | 0x08 | 0x20, 0o666, 0);
    let garbageData = "garbage that makes SQLite think the file is corrupted";
    foStream.write(garbageData, garbageData.length);
    foStream.close();

    // Get the service and make sure it backs up and recreates the database.
    await with_cps_instance(cps => {
      do_check_true(backupDBFile.exists());
      do_check_true(cps.DBConnection.connectionReady);
    });
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
    await with_cps_instance(cps => {
      do_check_true(backupDBFile.exists());
      do_check_true(cps.DBConnection.connectionReady);
    });
  }


  // Now get the content pref service for real for use by the rest of the tests.
  let cps = new ContentPrefInstance(null);

  var uri = ContentPrefTest.getURI("http://www.example.com/");

  // Make sure disk synchronization checking is turned off by default.
  var statement = cps.DBConnection.createStatement("PRAGMA synchronous");
  try {
    statement.executeStep();
    do_check_eq(0, statement.getInt32(0));
  } finally {
    statement.finalize();
  }

  // Nonexistent Pref

  do_check_eq(cps.getPref(uri, "test.nonexistent.getPref"), undefined);
  do_check_eq(cps.setPref(uri, "test.nonexistent.setPref", 5), undefined);
  do_check_false(cps.hasPref(uri, "test.nonexistent.hasPref"));
  do_check_eq(cps.removePref(uri, "test.nonexistent.removePref"), undefined);


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


  // Site-Specificity

  {
    // These are all different sites, and setting a pref for one of them
    // shouldn't set it for the others.
    let uri1 = ContentPrefTest.getURI("http://www.domain1.com/");
    let uri2 = ContentPrefTest.getURI("http://foo.domain1.com/");
    let uri3 = ContentPrefTest.getURI("http://domain1.com/");
    let uri4 = ContentPrefTest.getURI("http://www.domain2.com/");

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
  }

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
    onContentPrefSet: function genericObserver_onContentPrefSet(group, name, value, isPrivate) {
      ++this.numTimesSetCalled;
      do_check_eq(group, "www.example.com");
      if (name == "test.observer.private")
        do_check_true(isPrivate);
      else if (name == "test.observer.normal")
        do_check_false(isPrivate);
      else if (name != "test.observer.1" && name != "test.observer.2")
        do_throw("genericObserver.onContentPrefSet: " +
                 "name not in (test.observer.(1|2|normal|private))");
      do_check_eq(value, "test value");
    },

    numTimesRemovedCalled: 0,
    onContentPrefRemoved: function genericObserver_onContentPrefRemoved(group, name, isPrivate) {
      ++this.numTimesRemovedCalled;
      do_check_eq(group, "www.example.com");
      if (name == "test.observer.private")
        do_check_true(isPrivate);
      else if (name == "test.observer.normal")
        do_check_false(isPrivate);
      if (name != "test.observer.1" && name != "test.observer.2" &&
          name != "test.observer.normal" && name != "test.observer.private") {
        do_throw("genericObserver.onContentPrefSet: " +
                 "name not in (test.observer.(1|2|normal|private))");
      }
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

  // Make sure information about private context is properly
  // retrieved by the observer.
  cps.setPref(uri, "test.observer.private", "test value", privateLoadContext);
  cps.setPref(uri, "test.observer.normal", "test value", loadContext);
  cps.removePref(uri, "test.observer.private");
  cps.removePref(uri, "test.observer.normal");

  // Make sure we can remove observers and they don't get notified
  // about changes anymore.
  cps.removeObserver("test.observer.1", specificObserver);
  cps.removeObserver(null, genericObserver);
  cps.setPref(uri, "test.observer.1", "test value");
  cps.removePref(uri, "test.observer.1", "test value");
  do_check_eq(specificObserver.numTimesSetCalled, 1);
  do_check_eq(genericObserver.numTimesSetCalled, 4);
  do_check_eq(specificObserver.numTimesRemovedCalled, 1);
  do_check_eq(genericObserver.numTimesRemovedCalled, 3);


  // Get/Remove Prefs By Name

  {
    var anObserver = {
      interfaces: [Ci.nsIContentPrefObserver, Ci.nsISupports],

      QueryInterface: function ContentPrefTest_QueryInterface(iid) {
        if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
          throw Cr.NS_ERROR_NO_INTERFACE;
        return this;
      },

      onContentPrefSet: function anObserver_onContentPrefSet(group, name, value) {
      },

      expectedDomains: [],
      numTimesRemovedCalled: 0,
      onContentPrefRemoved: function anObserver_onContentPrefRemoved(group, name) {
        ++this.numTimesRemovedCalled;

        // remove the domain from the list of expected domains
        var index = this.expectedDomains.indexOf(group);
        do_check_true(index >= 0);
        this.expectedDomains.splice(index, 1);
      }
    };

    let uri1 = ContentPrefTest.getURI("http://www.domain1.com/");
    let uri2 = ContentPrefTest.getURI("http://foo.domain1.com/");
    let uri3 = ContentPrefTest.getURI("http://domain1.com/");
    let uri4 = ContentPrefTest.getURI("http://www.domain2.com/");

    cps.setPref(uri1, "test.byname.1", 1);
    cps.setPref(uri1, "test.byname.2", 2);
    cps.setPref(uri2, "test.byname.1", 4);
    cps.setPref(uri3, "test.byname.3", 8);
    cps.setPref(uri4, "test.byname.1", 16);
    cps.setPref(null, "test.byname.1", 32);
    cps.setPref(null, "test.byname.2", false);

    function enumerateAndCheck(testName, expectedSum, expectedDomains) {
      var prefsByName = cps.getPrefsByName(testName);
      var enumerator = prefsByName.enumerator;
      var sum = 0;
      while (enumerator.hasMoreElements()) {
        var property = enumerator.getNext().QueryInterface(Components.interfaces.nsIProperty);
        sum += parseInt(property.value);

        // remove the domain from the list of expected domains
        var index = expectedDomains.indexOf(property.name);
        do_check_true(index >= 0);
        expectedDomains.splice(index, 1);
      }
      do_check_eq(sum, expectedSum);
      // check all domains have been removed from the array
      do_check_eq(expectedDomains.length, 0);
    }

    enumerateAndCheck("test.byname.1", 53,
      ["foo.domain1.com", null, "www.domain1.com", "www.domain2.com"]);
    enumerateAndCheck("test.byname.2", 2, ["www.domain1.com", null]);
    enumerateAndCheck("test.byname.3", 8, ["domain1.com"]);

    cps.addObserver("test.byname.1", anObserver);
    anObserver.expectedDomains = ["foo.domain1.com", null, "www.domain1.com", "www.domain2.com"];

    cps.removePrefsByName("test.byname.1");
    do_check_false(cps.hasPref(uri1, "test.byname.1"));
    do_check_false(cps.hasPref(uri2, "test.byname.1"));
    do_check_false(cps.hasPref(uri3, "test.byname.1"));
    do_check_false(cps.hasPref(uri4, "test.byname.1"));
    do_check_false(cps.hasPref(null, "test.byname.1"));
    do_check_true(cps.hasPref(uri1, "test.byname.2"));
    do_check_true(cps.hasPref(uri3, "test.byname.3"));

    do_check_eq(anObserver.numTimesRemovedCalled, 4);
    do_check_eq(anObserver.expectedDomains.length, 0);

    cps.removeObserver("test.byname.1", anObserver);

    // Clean up after ourselves
    cps.removePref(uri1, "test.byname.2");
    cps.removePref(uri3, "test.byname.3");
    cps.removePref(null, "test.byname.2");
  }


  // Clear Private Data Pref Removal

  {
    let uri1 = ContentPrefTest.getURI("http://www.domain1.com/");
    let uri2 = ContentPrefTest.getURI("http://www.domain2.com/");
    let uri3 = ContentPrefTest.getURI("http://www.domain3.com/");

    let dbConnection = cps.DBConnection;

    let prefCount = dbConnection.createStatement("SELECT COUNT(*) AS count FROM prefs");

    let groupCount = dbConnection.createStatement("SELECT COUNT(*) AS count FROM groups");

    // Add some prefs for multiple domains.
    cps.setPref(uri1, "test.removeAllGroups", 1);
    cps.setPref(uri2, "test.removeAllGroups", 2);
    cps.setPref(uri3, "test.removeAllGroups", 3);

    // Add a global pref.
    cps.setPref(null, "test.removeAllGroups", 1);

    // Make sure there are some prefs and groups in the database.
    prefCount.executeStep();
    do_check_true(prefCount.row.count > 0);
    prefCount.reset();
    groupCount.executeStep();
    do_check_true(groupCount.row.count > 0);
    groupCount.reset();

    // Remove all prefs and groups from the database using the same routine
    // the Clear Private Data dialog uses.
    cps.removeGroupedPrefs();

    // Make sure there are no longer any groups in the database and the only pref
    // is the global one.
    prefCount.executeStep();
    do_check_true(prefCount.row.count == 1);
    prefCount.reset();
    groupCount.executeStep();
    do_check_true(groupCount.row.count == 0);
    groupCount.reset();
    let globalPref = dbConnection.createStatement("SELECT groupID FROM prefs");
    globalPref.executeStep();
    do_check_true(globalPref.row.groupID == null);
    globalPref.reset();
  }
});
