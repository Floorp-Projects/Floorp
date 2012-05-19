/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests the data in extensions.sqlite for general sanity, making sure
// rows in one table only reference rows in another table that actually exist.


function check_db() {
  do_print("Checking DB sanity...");
  var dbfile = gProfD.clone();
  dbfile.append("extensions.sqlite");
  var db = Services.storage.openDatabase(dbfile);

  do_print("Checking locale_strings references rows in locale correctly...");
  let localeStringsStmt = db.createStatement("SELECT * FROM locale_strings");
  let localeStmt = db.createStatement("SELECT COUNT(*) AS count FROM locale WHERE id=:locale_id");
  let i = 0;
  while (localeStringsStmt.executeStep()) {
    i++;
    localeStmt.params.locale_id = localeStringsStmt.row.locale_id;
    do_check_true(localeStmt.executeStep());
    do_check_eq(localeStmt.row.count, 1);
    localeStmt.reset();
  }
  localeStmt.finalize();
  localeStringsStmt.finalize();
  do_print("Done. " + i + " rows in locale_strings checked.");


  do_print("Checking locale references rows in addon_locale and addon correctly...");
  localeStmt = db.createStatement("SELECT * FROM locale");
  let addonLocaleStmt = db.createStatement("SELECT COUNT(*) AS count FROM addon_locale WHERE locale_id=:locale_id");
  let addonStmt = db.createStatement("SELECT COUNT(*) AS count FROM addon WHERE defaultLocale=:locale_id");
  i = 0;
  while (localeStmt.executeStep()) {
    i++;
    addonLocaleStmt.params.locale_id = localeStmt.row.id;
    do_check_true(addonLocaleStmt.executeStep());
    if (addonLocaleStmt.row.count == 0) {
      addonStmt.params.locale_id = localeStmt.row.id;
      do_check_true(addonStmt.executeStep());
      do_check_eq(addonStmt.row.count, 1);
    } else {
      do_check_eq(addonLocaleStmt.row.count, 1);
    }
    addonLocaleStmt.reset();
    addonStmt.reset();
  }
  addonLocaleStmt.finalize();
  localeStmt.finalize();
  addonStmt.finalize();
  do_print("Done. " + i + " rows in locale checked.");


  do_print("Checking addon_locale references rows in locale correctly...");
  addonLocaleStmt = db.createStatement("SELECT * FROM addon_locale");
  localeStmt = db.createStatement("SELECT COUNT(*) AS count FROM locale WHERE id=:locale_id");
  i = 0;
  while (addonLocaleStmt.executeStep()) {
    i++;
    localeStmt.params.locale_id = addonLocaleStmt.row.locale_id;
    do_check_true(localeStmt.executeStep());
    do_check_eq(localeStmt.row.count, 1);
    localeStmt.reset();
  }
  addonLocaleStmt.finalize();
  localeStmt.finalize();
  do_print("Done. " + i + " rows in addon_locale checked.");


  do_print("Checking addon_locale references rows in addon correctly...");
  addonLocaleStmt = db.createStatement("SELECT * FROM addon_locale");
  addonStmt = db.createStatement("SELECT COUNT(*) AS count FROM addon WHERE internal_id=:addon_internal_id");
  i = 0;
  while (addonLocaleStmt.executeStep()) {
    i++;
    addonStmt.params.addon_internal_id = addonLocaleStmt.row.addon_internal_id;
    do_check_true(addonStmt.executeStep());
    do_check_eq(addonStmt.row.count, 1);
    addonStmt.reset();
  }
  addonLocaleStmt.finalize();
  addonStmt.finalize();
  do_print("Done. " + i + " rows in addon_locale checked.");


  do_print("Checking addon references rows in locale correctly...");
  addonStmt = db.createStatement("SELECT * FROM addon");
  localeStmt = db.createStatement("SELECT COUNT(*) AS count FROM locale WHERE id=:defaultLocale");
  i = 0;
  while (addonStmt.executeStep()) {
    i++;
    localeStmt.params.defaultLocale = addonStmt.row.defaultLocale;
    do_check_true(localeStmt.executeStep());
    do_check_eq(localeStmt.row.count, 1);
    localeStmt.reset();
  }
  addonStmt.finalize();
  localeStmt.finalize();
  do_print("Done. " + i + " rows in addon checked.");


  do_print("Checking targetApplication references rows in addon correctly...");
  let targetAppStmt = db.createStatement("SELECT * FROM targetApplication");
  addonStmt = db.createStatement("SELECT COUNT(*) AS count FROM addon WHERE internal_id=:addon_internal_id");
  i = 0;
  while (targetAppStmt.executeStep()) {
    i++;
    addonStmt.params.addon_internal_id = targetAppStmt.row.addon_internal_id;
    do_check_true(addonStmt.executeStep());
    do_check_eq(addonStmt.row.count, 1);
    addonStmt.reset();
  }
  targetAppStmt.finalize();
  addonStmt.finalize();
  do_print("Done. " + i + " rows in targetApplication checked.");


  do_print("Checking targetPlatform references rows in addon correctly...");
  let targetPlatformStmt = db.createStatement("SELECT * FROM targetPlatform");
  addonStmt = db.createStatement("SELECT COUNT(*) AS count FROM addon WHERE internal_id=:addon_internal_id");
  i = 0;
  while (targetPlatformStmt.executeStep()) {
    i++;
    addonStmt.params.addon_internal_id = targetPlatformStmt.row.addon_internal_id;
    do_check_true(addonStmt.executeStep());
    do_check_eq(addonStmt.row.count, 1);
    addonStmt.reset();
  }
  targetPlatformStmt.finalize();
  addonStmt.finalize();
  do_print("Done. " + i + " rows in targetPlatform checked.");


  db.close();
  do_print("Done checking DB sanity.");
}

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  startupManager();

  installAllFiles([do_get_addon("test_db_sanity_1_1")], run_test_1);
}

function run_test_1() {
  shutdownManager();
  check_db();
  startupManager();

  AddonManager.getAddonByID("test_db_sanity_1@tests.mozilla.org", function(aAddon) {
    aAddon.uninstall();

    shutdownManager();
    check_db();
    startupManager();

    installAllFiles([do_get_addon("test_db_sanity_1_1")], run_test_2);
  });
}

function run_test_2() {
  installAllFiles([do_get_addon("test_db_sanity_1_2")], function() {
    shutdownManager();
    check_db();
    startupManager();
    run_test_3();
  });
}

function run_test_3() {
  AddonManager.getAddonByID("test_db_sanity_1@tests.mozilla.org", function(aAddon) {
    aAddon.uninstall();

    shutdownManager();
    check_db();

    do_test_finished();
  });
}
