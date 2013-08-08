/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we fail to migrate but still start up ok when there is a SQLITE database
// with no useful data in it.

const PREF_GENERAL_SKINS_SELECTEDSKIN = "general.skins.selectedSkin";

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
  name: "Test 5",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0",
    maxVersion: "0"
  }]
};

var defaultTheme = {
  id: "default@tests.mozilla.org",
  version: "2.0",
  name: "Default theme",
  internalName: "classic/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var theme1 = {
  id: "theme1@tests.mozilla.org",
  version: "2.0",
  name: "Test theme",
  internalName: "theme1/1.0",
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

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(defaultTheme, profileDir);
  writeInstallRDFForExtension(theme1, profileDir);

  Services.prefs.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN, "theme1/1.0");

  // Write out a broken database (no userDisabled field)
  let dbfile = gProfD.clone();
  dbfile.append("extensions.sqlite");
  let db = AM_Cc["@mozilla.org/storage/service;1"].
           getService(AM_Ci.mozIStorageService).
           openDatabase(dbfile);
  db.createTable("addon", "internal_id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                          "id TEXT, location TEXT, version TEXT, active INTEGER, " +
                          "installDate INTEGER");
  db.createTable("targetApplication", "addon_internal_id INTEGER, " +
                                      "id TEXT, minVersion TEXT, maxVersion TEXT");
  let stmt = db.createStatement("INSERT INTO addon VALUES (NULL, :id, :location, " +
                                ":version, :active, :installDate)");

  let internal_ids = {};

  [["addon1@tests.mozilla.org", "app-profile", "1.0", "1", "0"],
   ["addon2@tests.mozilla.org", "app-profile", "2.0", "0", "0"],
   ["default@tests.mozilla.org", "app-profile", "2.0", "1", "0"],
   ["theme1@tests.mozilla.org", "app-profile", "2.0", "0", "0"]].forEach(function(a) {
    stmt.params.id = a[0];
    stmt.params.location = a[1];
    stmt.params.version = a[2];
    stmt.params.active = a[3];
    stmt.params.installDate = a[4];
    stmt.execute();
    internal_ids[a[0]] = db.lastInsertRowID;
  });
  stmt.finalize();

  db.schemaVersion = 100;
  Services.prefs.setIntPref("extensions.databaseSchema", 100);
  db.close();

  startupManager();
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", []);
  check_startup_changes("enabled", []);

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "default@tests.mozilla.org",
                               "theme1@tests.mozilla.org"],
                               function([a1, a2, d, t1]) {
    do_check_neq(a1, null);
    do_check_false(a1.userDisabled);
    do_check_false(a1.appDisabled);
    do_check_true(a1.isActive);

    do_check_neq(a2, null);
    do_check_false(a2.userDisabled);
    do_check_true(a2.appDisabled);
    do_check_false(a2.isActive);

    // Should have enabled the selected theme
    do_check_neq(t1, null);
    do_check_false(t1.userDisabled);
    do_check_false(t1.appDisabled);
    do_check_true(t1.isActive);

    do_check_neq(d, null);
    do_check_true(d.userDisabled);
    do_check_false(d.appDisabled);
    do_check_false(d.isActive);

    do_test_finished();
  });
}
