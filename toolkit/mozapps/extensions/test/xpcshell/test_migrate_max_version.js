/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we don't migrate data from SQLITE if
// the "extensions.databaseSchema" preference shows we've
// already upgraded to JSON

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

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  writeInstallRDFForExtension(addon1, profileDir);

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

  let a = ["addon1@tests.mozilla.org", "app-profile", "1.0", "0", "1", "0"];
  stmt.params.id = a[0];
  stmt.params.location = a[1];
  stmt.params.version = a[2];
  stmt.params.active = a[3];
  stmt.params.userDisabled = a[4];
  stmt.params.installDate = a[5];
  stmt.execute();
  internal_ids[a[0]] = db.lastInsertRowID;
  stmt.finalize();

  db.schemaVersion = 14;
  Services.prefs.setIntPref("extensions.databaseSchema", 14);
  db.close();

  startupManager();
  run_next_test();
}

add_test(function before_rebuild() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org",
                            function check_before_rebuild (a1) {
    // First check that it migrated OK once
    // addon1 was disabled in the database
    do_check_neq(a1, null);
    do_check_true(a1.userDisabled);
    do_check_false(a1.appDisabled);
    do_check_false(a1.isActive);
    do_check_false(a1.strictCompatibility);
    do_check_false(a1.foreignInstall);

    run_next_test();
  });
});

// now shut down, remove the JSON database, 
// start up again, and make sure the data didn't migrate this time
add_test(function rebuild_again() {
  shutdownManager();
  gExtensionsJSON.remove(true);
  startupManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org",
                            function check_after_rebuild(a1) {
    // addon1 was rebuilt from extensions directory,
    // so it appears enabled as a foreign install
    do_check_neq(a1, null);
    do_check_false(a1.userDisabled);
    do_check_false(a1.appDisabled);
    do_check_true(a1.isActive);
    do_check_false(a1.strictCompatibility);
    do_check_true(a1.foreignInstall);

    run_next_test();
  });
});
