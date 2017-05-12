/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that a pending upgrade during a schema update doesn't break things

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "2.0",
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
    maxVersion: "2"
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
    minVersion: "2",
    maxVersion: "2"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  run_test_1();
}

// Tests whether a schema migration without app version change works
async function run_test_1() {
  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);

  await promiseStartupManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                              function([a1, a2, a3, a4]) {
    do_check_neq(a1, null);
    do_check_eq(a1.version, "2.0");
    do_check_false(a1.appDisabled);
    do_check_false(a1.userDisabled);
    do_check_true(a1.isActive);
    do_check_true(isExtensionInAddonsList(profileDir, addon1.id));

    do_check_neq(a2, null);
    do_check_eq(a2.version, "2.0");
    do_check_false(a2.appDisabled);
    do_check_false(a2.userDisabled);
    do_check_true(a2.isActive);
    do_check_true(isExtensionInAddonsList(profileDir, addon2.id));

    do_check_neq(a3, null);
    do_check_eq(a3.version, "2.0");
    do_check_false(a3.appDisabled);
    do_check_false(a3.userDisabled);
    do_check_true(a3.isActive);
    do_check_true(isExtensionInAddonsList(profileDir, addon3.id));

    do_check_neq(a4, null);
    do_check_eq(a4.version, "2.0");
    do_check_true(a4.appDisabled);
    do_check_false(a4.userDisabled);
    do_check_false(a4.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, addon4.id));

    // Prepare the add-on update, and a bootstrapped addon (bug 693714)
    installAllFiles([
      do_get_addon("test_bug659772"),
      do_get_addon("test_bootstrap1_1")
    ], async function() {
      shutdownManager();

      // Make it look like the next time the app is started it has a new DB schema
      changeXPIDBVersion(1);
      Services.prefs.setIntPref("extensions.databaseSchema", 1);

      let jsonfile = gProfD.clone();
      jsonfile.append("extensions");
      jsonfile.append("staged");
      jsonfile.append("addon3@tests.mozilla.org.json");
      do_check_true(jsonfile.exists());

      // Remove an unnecessary property from the cached manifest
      let fis = AM_Cc["@mozilla.org/network/file-input-stream;1"].
                   createInstance(AM_Ci.nsIFileInputStream);
      let json = AM_Cc["@mozilla.org/dom/json;1"].
                 createInstance(AM_Ci.nsIJSON);
      fis.init(jsonfile, -1, 0, 0);
      let addonObj = json.decodeFromStream(fis, jsonfile.fileSize);
      fis.close();
      delete addonObj.optionsType;

      let stream = AM_Cc["@mozilla.org/network/file-output-stream;1"].
                   createInstance(AM_Ci.nsIFileOutputStream);
      let converter = AM_Cc["@mozilla.org/intl/converter-output-stream;1"].
                      createInstance(AM_Ci.nsIConverterOutputStream);
      stream.init(jsonfile, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
                            FileUtils.MODE_TRUNCATE, FileUtils.PERMS_FILE,
                            0);
      converter.init(stream, "UTF-8", 0, 0x0000);
      converter.writeString(JSON.stringify(addonObj));
      converter.close();
      stream.close();

      Services.prefs.clearUserPref("bootstraptest.install_reason");
      Services.prefs.clearUserPref("bootstraptest.uninstall_reason");

      await promiseStartupManager(false);

      AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                   "addon2@tests.mozilla.org",
                                   "addon3@tests.mozilla.org",
                                   "addon4@tests.mozilla.org"],
                                  function([a1_2, a2_2, a3_2, a4_2]) {
        do_check_neq(a1_2, null);
        do_check_eq(a1_2.version, "2.0");
        do_check_false(a1_2.appDisabled);
        do_check_false(a1_2.userDisabled);
        do_check_true(a1_2.isActive);
        do_check_true(isExtensionInAddonsList(profileDir, addon1.id));

        do_check_neq(a2_2, null);
        do_check_eq(a2_2.version, "2.0");
        do_check_false(a2_2.appDisabled);
        do_check_false(a2_2.userDisabled);
        do_check_true(a2_2.isActive);
        do_check_true(isExtensionInAddonsList(profileDir, addon2.id));

        // Should stay enabled because we migrate the compat info from
        // the previous version of the DB
        do_check_neq(a3_2, null);
        do_check_eq(a3_2.version, "2.0");
        todo_check_false(a3_2.appDisabled); // XXX unresolved issue
        do_check_false(a3_2.userDisabled);
        todo_check_true(a3_2.isActive); // XXX same
        todo_check_true(isExtensionInAddonsList(profileDir, addon3.id)); // XXX same

        do_check_neq(a4_2, null);
        do_check_eq(a4_2.version, "2.0");
        do_check_true(a4_2.appDisabled);
        do_check_false(a4_2.userDisabled);
        do_check_false(a4_2.isActive);
        do_check_false(isExtensionInAddonsList(profileDir, addon4.id));

        // Check that install and uninstall haven't been called on the bootstrapped addon
        do_check_false(Services.prefs.prefHasUserValue("bootstraptest.install_reason"));
        do_check_false(Services.prefs.prefHasUserValue("bootstraptest.uninstall_reason"));

        a1_2.uninstall();
        a2_2.uninstall();
        a3_2.uninstall();
        a4_2.uninstall();
        do_execute_soon(run_test_2);
      });
    });
  });
}

// Tests whether a schema migration with app version change works
async function run_test_2() {
  restartManager();

  shutdownManager();

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);

  await promiseStartupManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                              function([a1, a2, a3, a4]) {
    do_check_neq(a1, null);
    do_check_eq(a1.version, "2.0");
    do_check_false(a1.appDisabled);
    do_check_false(a1.userDisabled);
    do_check_true(a1.isActive);
    do_check_true(isExtensionInAddonsList(profileDir, addon1.id));

    do_check_neq(a2, null);
    do_check_eq(a2.version, "2.0");
    do_check_false(a2.appDisabled);
    do_check_false(a2.userDisabled);
    do_check_true(a2.isActive);
    do_check_true(isExtensionInAddonsList(profileDir, addon2.id));

    do_check_neq(a3, null);
    do_check_eq(a3.version, "2.0");
    do_check_false(a3.appDisabled);
    do_check_false(a3.userDisabled);
    do_check_true(a3.isActive);
    do_check_true(isExtensionInAddonsList(profileDir, addon3.id));

    do_check_neq(a4, null);
    do_check_eq(a4.version, "2.0");
    do_check_true(a4.appDisabled);
    do_check_false(a4.userDisabled);
    do_check_false(a4.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, addon4.id));

    // Prepare the add-on update, and a bootstrapped addon (bug 693714)
    installAllFiles([
      do_get_addon("test_bug659772"),
      do_get_addon("test_bootstrap1_1")
    ], function() { do_execute_soon(prepare_schema_migrate); });

    async function prepare_schema_migrate() {
      shutdownManager();

      // Make it look like the next time the app is started it has a new DB schema
      changeXPIDBVersion(1);
      Services.prefs.setIntPref("extensions.databaseSchema", 1);

      let jsonfile = gProfD.clone();
      jsonfile.append("extensions");
      jsonfile.append("staged");
      jsonfile.append("addon3@tests.mozilla.org.json");
      do_check_true(jsonfile.exists());

      // Remove an unnecessary property from the cached manifest
      let fis = AM_Cc["@mozilla.org/network/file-input-stream;1"].
                   createInstance(AM_Ci.nsIFileInputStream);
      let json = AM_Cc["@mozilla.org/dom/json;1"].
                 createInstance(AM_Ci.nsIJSON);
      fis.init(jsonfile, -1, 0, 0);
      let addonObj = json.decodeFromStream(fis, jsonfile.fileSize);
      fis.close();
      delete addonObj.optionsType;

      let stream = AM_Cc["@mozilla.org/network/file-output-stream;1"].
                   createInstance(AM_Ci.nsIFileOutputStream);
      let converter = AM_Cc["@mozilla.org/intl/converter-output-stream;1"].
                      createInstance(AM_Ci.nsIConverterOutputStream);
      stream.init(jsonfile, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
                            FileUtils.MODE_TRUNCATE, FileUtils.PERMS_FILE,
                            0);
      converter.init(stream, "UTF-8", 0, 0x0000);
      converter.writeString(JSON.stringify(addonObj));
      converter.close();
      stream.close();

      Services.prefs.clearUserPref("bootstraptest.install_reason");
      Services.prefs.clearUserPref("bootstraptest.uninstall_reason");

      gAppInfo.version = "2";
      await promiseStartupManager(true);

      AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                   "addon2@tests.mozilla.org",
                                   "addon3@tests.mozilla.org",
                                   "addon4@tests.mozilla.org"],
                                  callback_soon(function([a1_2, a2_2, a3_2, a4_2]) {
        do_check_neq(a1_2, null);
        do_check_eq(a1_2.version, "2.0");
        do_check_true(a1_2.appDisabled);
        do_check_false(a1_2.userDisabled);
        do_check_false(a1_2.isActive);
        do_check_false(isExtensionInAddonsList(profileDir, addon1.id));

        do_check_neq(a2_2, null);
        do_check_eq(a2_2.version, "2.0");
        do_check_false(a2_2.appDisabled);
        do_check_false(a2_2.userDisabled);
        do_check_true(a2_2.isActive);
        do_check_true(isExtensionInAddonsList(profileDir, addon2.id));

        // Should become appDisabled because we migrate the compat info from
        // the previous version of the DB
        do_check_neq(a3_2, null);
        do_check_eq(a3_2.version, "2.0");
        todo_check_true(a3_2.appDisabled);
        do_check_false(a3_2.userDisabled);
        todo_check_false(a3_2.isActive);
        todo_check_false(isExtensionInAddonsList(profileDir, addon3.id));

        do_check_neq(a4_2, null);
        do_check_eq(a4_2.version, "2.0");
        do_check_false(a4_2.appDisabled);
        do_check_false(a4_2.userDisabled);
        do_check_true(a4_2.isActive);
        do_check_true(isExtensionInAddonsList(profileDir, addon4.id));

        // Check that install and uninstall haven't been called on the bootstrapped addon
        do_check_false(Services.prefs.prefHasUserValue("bootstraptest.install_reason"));
        do_check_false(Services.prefs.prefHasUserValue("bootstraptest.uninstall_reason"));

        a1_2.uninstall();
        a2_2.uninstall();
        a3_2.uninstall();
        a4_2.uninstall();
        restartManager();

        shutdownManager();

        do_test_finished();
      }));
    }
  });
}
