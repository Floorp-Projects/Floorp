/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const EXPECTED_SCHEMA_VERSION = 4;
let dbfile;

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Write out a minimal database.
  dbfile = gProfD.clone();
  dbfile.append("addons.sqlite");
  let db = AM_Cc["@mozilla.org/storage/service;1"].
           getService(AM_Ci.mozIStorageService).
           openDatabase(dbfile);

  db.createTable("addon",
                 "internal_id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                 "id TEXT UNIQUE, " +
                 "type TEXT, " +
                 "name TEXT, " +
                 "version TEXT, " +
                 "creator TEXT, " +
                 "creatorURL TEXT, " +
                 "description TEXT, " +
                 "fullDescription TEXT, " +
                 "developerComments TEXT, " +
                 "eula TEXT, " +
                 "iconURL TEXT, " +
                 "homepageURL TEXT, " +
                 "supportURL TEXT, " +
                 "contributionURL TEXT, " +
                 "contributionAmount TEXT, " +
                 "averageRating INTEGER, " +
                 "reviewCount INTEGER, " +
                 "reviewURL TEXT, " +
                 "totalDownloads INTEGER, " +
                 "weeklyDownloads INTEGER, " +
                 "dailyUsers INTEGER, " +
                 "sourceURI TEXT, " +
                 "repositoryStatus INTEGER, " +
                 "size INTEGER, " +
                 "updateDate INTEGER");

  db.createTable("developer",
                 "addon_internal_id INTEGER, " +
                 "num INTEGER, " +
                 "name TEXT, " +
                 "url TEXT, " +
                 "PRIMARY KEY (addon_internal_id, num)");

  db.createTable("screenshot",
                 "addon_internal_id INTEGER, " +
                 "num INTEGER, " +
                 "url TEXT, " +
                 "thumbnailURL TEXT, " +
                 "caption TEXT, " +
                 "PRIMARY KEY (addon_internal_id, num)");

  let stmt = db.createStatement("INSERT INTO addon (id) VALUES (:id)");
  stmt.params.id = "test1@tests.mozilla.org";
  stmt.execute();
  stmt.finalize();

  stmt = db.createStatement("INSERT INTO screenshot VALUES " +
                            "(:addon_internal_id, :num, :url, :thumbnailURL, :caption)");

  stmt.params.addon_internal_id = 1;
  stmt.params.num = 0;
  stmt.params.url = "http://localhost/full1-1.png";
  stmt.params.thumbnailURL = "http://localhost/thumbnail1-1.png";
  stmt.params.caption = "Caption 1 - 1";
  stmt.execute();
  stmt.finalize();

  db.schemaVersion = 1;
  db.close();

  Services.obs.addObserver({
    observe: function () {
      Services.obs.removeObserver(this, "addon-repository-shutdown");
      // Check the DB schema has changed once AddonRepository has freed it.
      db = AM_Cc["@mozilla.org/storage/service;1"].
           getService(AM_Ci.mozIStorageService).
           openDatabase(dbfile);
      do_check_eq(db.schemaVersion, EXPECTED_SCHEMA_VERSION);
      do_check_true(db.indexExists("developer_idx"));
      do_check_true(db.indexExists("screenshot_idx"));
      do_check_true(db.indexExists("compatibility_override_idx"));
      do_check_true(db.tableExists("compatibility_override"));
      do_check_true(db.indexExists("icon_idx"));
      do_check_true(db.tableExists("icon"));

      // Check the trigger is working
      db.executeSimpleSQL("INSERT INTO addon (id, type, name) VALUES('test_addon', 'extension', 'Test Addon')");
      let internalID = db.lastInsertRowID;
      db.executeSimpleSQL("INSERT INTO compatibility_override (addon_internal_id, num, type) VALUES('" + internalID + "', '1', 'incompatible')");

      let stmt = db.createStatement("SELECT COUNT(*) AS count FROM compatibility_override");
      stmt.executeStep();
      do_check_eq(stmt.row.count, 1);
      stmt.reset();

      db.executeSimpleSQL("DELETE FROM addon");
      stmt.executeStep();
      do_check_eq(stmt.row.count, 0);
      stmt.finalize();

      db.close();
      run_test_2();
    }
  }, "addon-repository-shutdown", null);

  Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", true);
  AddonRepository.getCachedAddonByID("test1@tests.mozilla.org", function (aAddon) {
    do_check_neq(aAddon, null);
    do_check_eq(aAddon.screenshots.length, 1);
    do_check_true(aAddon.screenshots[0].width === null);
    do_check_true(aAddon.screenshots[0].height === null);
    do_check_true(aAddon.screenshots[0].thumbnailWidth === null);
    do_check_true(aAddon.screenshots[0].thumbnailHeight === null);
    do_check_eq(aAddon.iconURL, undefined);
    do_check_eq(JSON.stringify(aAddon.icons), "{}");
    AddonRepository.shutdown();
  });
}

function run_test_2() {
  // Write out a minimal database.
  let db = AM_Cc["@mozilla.org/storage/service;1"].
           getService(AM_Ci.mozIStorageService).
           openDatabase(dbfile);

  db.createTable("futuristicSchema",
                 "id INTEGER, " +
                 "sharks TEXT, " +
                 "lasers TEXT");

  db.schemaVersion = 1000;
  db.close();

  Services.obs.addObserver({
    observe: function () {
      Services.obs.removeObserver(this, "addon-repository-shutdown");
      // Check the DB schema has changed once AddonRepository has freed it.
      db = AM_Cc["@mozilla.org/storage/service;1"].
           getService(AM_Ci.mozIStorageService).
           openDatabase(dbfile);
      do_check_eq(db.schemaVersion, EXPECTED_SCHEMA_VERSION);
      db.close();
      do_test_finished();
    }
  }, "addon-repository-shutdown", null);

  // Force a connection to the addon database to be opened.
  Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", true);
  AddonRepository.getCachedAddonByID("test1@tests.mozilla.org", function (aAddon) {
    AddonRepository.shutdown();
  });
}
