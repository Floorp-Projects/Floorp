/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we migrate data from future versions of the database

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "2.0",
  name: "Test 2",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "2.0",
  name: "Test 3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "2.0",
  name: "Test 4",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  var dest = profileDir.clone();
  dest.append("addon1@tests.mozilla.org");
  writeInstallRDFToDir(addon1, dest);
  dest = profileDir.clone();
  dest.append("addon2@tests.mozilla.org");
  writeInstallRDFToDir(addon2, dest);
  dest = profileDir.clone();
  dest.append("addon3@tests.mozilla.org");
  writeInstallRDFToDir(addon3, dest);
  dest = profileDir.clone();
  dest.append("addon4@tests.mozilla.org");
  writeInstallRDFToDir(addon4, dest);


  // Write out a minimal database
  let dbfile = gProfD.clone();
  dbfile.append("extensions.sqlite");
  let db = AM_Cc["@mozilla.org/storage/service;1"].
           getService(AM_Ci.mozIStorageService).
           openDatabase(dbfile);
  db.createTable("addon", "id TEXT, location TEXT, active INTEGER, " +
                          "userDisabled INTEGER, installDate INTEGER, " +
                          "UNIQUE (id, location)");
  let stmt = db.createStatement("INSERT INTO addon VALUES (:id, :location, " +
                                ":active, :userDisabled, :installDate)");

  [["addon1@tests.mozilla.org", "app-profile", "1", "0", "0"],
   ["addon2@tests.mozilla.org", "app-profile", "0", "1", "0"],
   ["addon3@tests.mozilla.org", "app-profile", "1", "1", "0"],
   ["addon4@tests.mozilla.org", "app-profile", "0", "0", "0"]].forEach(function(a) {
    stmt.params.id = a[0];
    stmt.params.location = a[1];
    stmt.params.active = a[2];
    stmt.params.userDisabled = a[3];
    stmt.params.installDate = a[4];
    stmt.execute();
  });
  stmt.finalize();
  db.close();
  Services.prefs.setIntPref("extensions.databaseSchema", 100);

  startupManager(1);
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"], function([a1, a2, a3, a4]) {
    // addon1 was enabled in the database
    do_check_neq(a1, null);
    do_check_false(a1.userDisabled);
    // addon1 was disabled in the database
    do_check_neq(a2, null);
    do_check_true(a2.userDisabled);
    // addon1 was pending-disable in the database
    do_check_neq(a3, null);
    do_check_true(a3.userDisabled);
    // addon1 was pending-enable in the database
    do_check_neq(a4, null);
    do_check_false(a4.userDisabled);
    do_test_finished();
  });
}
