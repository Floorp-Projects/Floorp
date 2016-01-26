/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we migrate data from SQLITE databases
// Note that since the database doesn't contain the foreignInstall field we
// should just assume that no add-ons  in the user profile were foreignInstalls

// Enable loading extensions from the user and system scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_USER +
                          AddonManager.SCOPE_SYSTEM);

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
  strictCompatibility: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "2.0",
  name: "Test 5",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0",
    maxVersion: "0"
  }]
};

var addon6 = {
  id: "addon6@tests.mozilla.org",
  version: "2.0",
  name: "Test 6",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0",
    maxVersion: "0"
  }]
};

var addon7 = {
  id: "addon7@tests.mozilla.org",
  version: "2.0",
  name: "Test 7",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon8 = {
  id: "addon8@tests.mozilla.org",
  version: "2.0",
  name: "Test 8",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
const profileDir = gProfD.clone();
profileDir.append("extensions");
const globalDir = gProfD.clone();
globalDir.append("extensions2");
globalDir.append(gAppInfo.ID);
registerDirectory("XRESysSExtPD", globalDir.parent);
const userDir = gProfD.clone();
userDir.append("extensions3");
userDir.append(gAppInfo.ID);
registerDirectory("XREUSysExt", userDir.parent);

function run_test() {
  do_test_pending();

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);
  writeInstallRDFForExtension(addon6, profileDir);
  writeInstallRDFForExtension(addon7, globalDir);
  writeInstallRDFForExtension(addon8, userDir);

  // Write out a minimal database
  let dbfile = gProfD.clone();
  dbfile.append("extensions.sqlite");
  let db = AM_Cc["@mozilla.org/storage/service;1"].
           getService(AM_Ci.mozIStorageService).
           openDatabase(dbfile);
  db.createTable("addon", "internal_id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                          "id TEXT, location TEXT, version TEXT, active INTEGER, " +
                          "userDisabled INTEGER, installDate INTEGER");
  db.createTable("targetApplication", "addon_internal_id INTEGER, " +
                                      "id TEXT, minVersion TEXT, maxVersion TEXT");
  let stmt = db.createStatement("INSERT INTO addon VALUES (NULL, :id, :location, " +
                                ":version, :active, :userDisabled, :installDate)");

  let internal_ids = {};

  [["addon1@tests.mozilla.org", "app-profile", "1.0", "1", "0", "0"],
   ["addon2@tests.mozilla.org", "app-profile", "2.0", "0", "1", "0"],
   ["addon3@tests.mozilla.org", "app-profile", "2.0", "1", "1", "0"],
   ["addon4@tests.mozilla.org", "app-profile", "2.0", "0", "0", "0"],
   ["addon5@tests.mozilla.org", "app-profile", "2.0", "1", "0", "0"],
   ["addon6@tests.mozilla.org", "app-profile", "1.0", "0", "1", "0"],
   ["addon7@tests.mozilla.org", "app-system-share", "1.0", "1", "0", "0"],
   ["addon8@tests.mozilla.org", "app-system-user", "1.0", "1", "0", "0"]].forEach(function(a) {
    stmt.params.id = a[0];
    stmt.params.location = a[1];
    stmt.params.version = a[2];
    stmt.params.active = a[3];
    stmt.params.userDisabled = a[4];
    stmt.params.installDate = a[5];
    stmt.execute();
    internal_ids[a[0]] = db.lastInsertRowID;
  });
  stmt.finalize();

  // Add updated target application into for addon5
  stmt = db.createStatement("INSERT INTO targetApplication VALUES " +
                            "(:internal_id, :id, :minVersion, :maxVersion)");
  stmt.params.internal_id = internal_ids["addon5@tests.mozilla.org"];
  stmt.params.id = "xpcshell@tests.mozilla.org";
  stmt.params.minVersion = "0";
  stmt.params.maxVersion = "1";
  stmt.execute();

  // Add updated target application into for addon6
  stmt.params.internal_id = internal_ids["addon6@tests.mozilla.org"];
  stmt.params.id = "xpcshell@tests.mozilla.org";
  stmt.params.minVersion = "0";
  stmt.params.maxVersion = "1";
  stmt.execute();
  stmt.finalize();

  db.schemaVersion = 10000;
  Services.prefs.setIntPref("extensions.databaseSchema", 14);
  db.close();

  startupManager();
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", []);
  check_startup_changes("enabled", []);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org",
                               "addon8@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5, a6, a7, a8]) {
    // addon1 was enabled in the database
    do_check_neq(a1, null);
    do_check_false(a1.userDisabled);
    do_check_false(a1.appDisabled);
    do_check_true(a1.isActive);
    do_check_false(a1.strictCompatibility);
    do_check_false(a1.foreignInstall);
    do_check_true(a1.seen);
    // addon2 was disabled in the database
    do_check_neq(a2, null);
    do_check_true(a2.userDisabled);
    do_check_false(a2.appDisabled);
    do_check_false(a2.isActive);
    do_check_false(a2.strictCompatibility);
    do_check_false(a2.foreignInstall);
    do_check_true(a2.seen);
    // addon3 was pending-disable in the database
    do_check_neq(a3, null);
    do_check_true(a3.userDisabled);
    do_check_false(a3.appDisabled);
    do_check_false(a3.isActive);
    do_check_false(a3.strictCompatibility);
    do_check_false(a3.foreignInstall);
    do_check_true(a3.seen);
    // addon4 was pending-enable in the database
    do_check_neq(a4, null);
    do_check_false(a4.userDisabled);
    do_check_false(a4.appDisabled);
    do_check_true(a4.isActive);
    do_check_true(a4.strictCompatibility);
    do_check_false(a4.foreignInstall);
    do_check_true(a4.seen);
    // addon5 was enabled in the database but needed a compatibility update
    do_check_neq(a5, null);
    do_check_false(a5.userDisabled);
    do_check_false(a5.appDisabled);
    do_check_true(a5.isActive);
    do_check_false(a5.strictCompatibility);
    do_check_false(a5.foreignInstall);
    do_check_true(a5.seen);
    // addon6 was disabled and compatible but a new version has been installed
    // since, it should still be disabled but should be incompatible
    do_check_neq(a6, null);
    do_check_true(a6.userDisabled);
    do_check_true(a6.appDisabled);
    do_check_false(a6.isActive);
    do_check_false(a6.strictCompatibility);
    do_check_false(a6.foreignInstall);
    do_check_true(a6.seen);
    // addon7 is in the global install location so should be a foreignInstall
    do_check_neq(a7, null);
    do_check_false(a7.userDisabled);
    do_check_false(a7.appDisabled);
    do_check_true(a7.isActive);
    do_check_false(a7.strictCompatibility);
    do_check_true(a7.foreignInstall);
    do_check_true(a7.seen);
    // addon8 is in the user install location so should be a foreignInstall
    do_check_neq(a8, null);
    do_check_false(a8.userDisabled);
    do_check_false(a8.appDisabled);
    do_check_true(a8.isActive);
    do_check_false(a8.strictCompatibility);
    do_check_true(a8.foreignInstall);
    do_check_true(a8.seen);

    do_execute_soon(do_test_finished);
  });
}
