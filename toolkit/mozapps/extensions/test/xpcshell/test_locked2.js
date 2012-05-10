/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we handle a locked database when there are extension changes
// in progress

// Will be left alone
var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// Will be enabled
var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "1.0",
  name: "Test 2",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// Will be disabled
var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "1.0",
  name: "Test 3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// Will be uninstalled
var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "1.0",
  name: "Test 4",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};


// Will be updated
var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "1.0",
  name: "Test 5",
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
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);

  // Make it look like add-on 5 was installed some time in the past so the update is
  // detected
  setExtensionModifiedTime(getFileForAddon(profileDir, addon5.id), Date.now() - (60000));

  // Startup the profile and setup the initial state
  startupManager();

  check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
  check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);

  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
    a2.userDisabled = true;

    restartManager();

    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org",
                                 "addon3@tests.mozilla.org",
                                 "addon4@tests.mozilla.org",
                                 "addon5@tests.mozilla.org"],
                                function([a1, a2, a3, a4, a5]) {
      a2.userDisabled = false;
      a3.userDisabled = true;
      a4.uninstall();

      installAllFiles([do_get_addon("test_locked2_5"),
                       do_get_addon("test_locked2_6")], function() {
        do_check_neq(a1, null);
        do_check_true(a1.isActive);
        do_check_false(a1.userDisabled);
        do_check_false(a1.appDisabled);
        do_check_eq(a1.pendingOperations, AddonManager.PENDING_NONE);
        do_check_true(isExtensionInAddonsList(profileDir, a1.id));

        do_check_neq(a2, null);
        do_check_false(a2.isActive);
        do_check_false(a2.userDisabled);
        do_check_false(a2.appDisabled);
        do_check_eq(a2.pendingOperations, AddonManager.PENDING_ENABLE);
        do_check_false(isExtensionInAddonsList(profileDir, a2.id));

        do_check_neq(a3, null);
        do_check_true(a3.isActive);
        do_check_true(a3.userDisabled);
        do_check_false(a3.appDisabled);
        do_check_eq(a3.pendingOperations, AddonManager.PENDING_DISABLE);
        do_check_true(isExtensionInAddonsList(profileDir, a3.id));

        do_check_neq(a4, null);
        do_check_true(a4.isActive);
        do_check_false(a4.userDisabled);
        do_check_false(a4.appDisabled);
        do_check_eq(a4.pendingOperations, AddonManager.PENDING_UNINSTALL);
        do_check_true(isExtensionInAddonsList(profileDir, a4.id));

        do_check_neq(a5, null);
        do_check_eq(a5.version, "1.0");
        do_check_true(a5.isActive);
        do_check_false(a5.userDisabled);
        do_check_false(a5.appDisabled);
        do_check_eq(a5.pendingOperations, AddonManager.PENDING_UPGRADE);
        do_check_true(isExtensionInAddonsList(profileDir, a5.id));

        // After shutting down the database won't be open so we can lock it
        shutdownManager();
        var dbfile = gProfD.clone();
        dbfile.append("extensions.sqlite");
        let connection = Services.storage.openUnsharedDatabase(dbfile);
        connection.executeSimpleSQL("PRAGMA synchronous = FULL");
        connection.executeSimpleSQL("PRAGMA locking_mode = EXCLUSIVE");
        // Force the DB to become locked
        connection.beginTransactionAs(connection.TRANSACTION_EXCLUSIVE);
        connection.commitTransaction();

        startupManager(false);

        check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, []);
        check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, []);

        AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                     "addon2@tests.mozilla.org",
                                     "addon3@tests.mozilla.org",
                                     "addon4@tests.mozilla.org",
                                     "addon5@tests.mozilla.org",
                                     "addon6@tests.mozilla.org"],
                                    function([a1, a2, a3, a4, a5, a6]) {
          do_check_neq(a1, null);
          do_check_true(a1.isActive);
          do_check_false(a1.userDisabled);
          do_check_false(a1.appDisabled);
          do_check_eq(a1.pendingOperations, AddonManager.PENDING_NONE);
          do_check_true(isExtensionInAddonsList(profileDir, a1.id));

          do_check_neq(a2, null);
          do_check_true(a2.isActive);
          do_check_false(a2.userDisabled);
          do_check_false(a2.appDisabled);
          do_check_eq(a2.pendingOperations, AddonManager.PENDING_NONE);
          do_check_true(isExtensionInAddonsList(profileDir, a2.id));

          do_check_neq(a3, null);
          do_check_false(a3.isActive);
          do_check_true(a3.userDisabled);
          do_check_false(a3.appDisabled);
          do_check_eq(a3.pendingOperations, AddonManager.PENDING_NONE);
          do_check_false(isExtensionInAddonsList(profileDir, a3.id));

          do_check_eq(a4, null);

          do_check_neq(a5, null);
          do_check_eq(a5.version, "2.0");
          do_check_true(a5.isActive);
          do_check_false(a5.userDisabled);
          do_check_false(a5.appDisabled);
          do_check_eq(a5.pendingOperations, AddonManager.PENDING_NONE);
          do_check_true(isExtensionInAddonsList(profileDir, a5.id));

          do_check_neq(a6, null);
          do_check_true(a6.isActive);
          do_check_false(a6.userDisabled);
          do_check_false(a6.appDisabled);
          do_check_eq(a6.pendingOperations, AddonManager.PENDING_NONE);
          do_check_true(isExtensionInAddonsList(profileDir, a6.id));

          connection.close();

          // After allowing access to the original DB things should still be
          // applied correctly
          restartManager();

          // These things happened when we had no access to the database so
          // they are seen as external changes when we get the database back :(
          check_startup_changes(AddonManager.STARTUP_CHANGE_INSTALLED, ["addon6@tests.mozilla.org"]);
          check_startup_changes(AddonManager.STARTUP_CHANGE_UNINSTALLED, ["addon4@tests.mozilla.org"]);

          AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                       "addon2@tests.mozilla.org",
                                       "addon3@tests.mozilla.org",
                                       "addon4@tests.mozilla.org",
                                       "addon5@tests.mozilla.org",
                                       "addon6@tests.mozilla.org"],
                                      function([a1, a2, a3, a4, a5, a6]) {
            do_check_neq(a1, null);
            do_check_true(a1.isActive);
            do_check_false(a1.userDisabled);
            do_check_false(a1.appDisabled);
            do_check_eq(a1.pendingOperations, AddonManager.PENDING_NONE);
            do_check_true(isExtensionInAddonsList(profileDir, a1.id));

            do_check_neq(a2, null);
            do_check_true(a2.isActive);
            do_check_false(a2.userDisabled);
            do_check_false(a2.appDisabled);
            do_check_eq(a2.pendingOperations, AddonManager.PENDING_NONE);
            do_check_true(isExtensionInAddonsList(profileDir, a2.id));

            do_check_neq(a3, null);
            do_check_false(a3.isActive);
            do_check_true(a3.userDisabled);
            do_check_false(a3.appDisabled);
            do_check_eq(a3.pendingOperations, AddonManager.PENDING_NONE);
            do_check_false(isExtensionInAddonsList(profileDir, a3.id));

            do_check_eq(a4, null);

            do_check_neq(a5, null);
            do_check_eq(a5.version, "2.0");
            do_check_true(a5.isActive);
            do_check_false(a5.userDisabled);
            do_check_false(a5.appDisabled);
            do_check_eq(a5.pendingOperations, AddonManager.PENDING_NONE);
            do_check_true(isExtensionInAddonsList(profileDir, a5.id));

            do_check_neq(a6, null);
            do_check_true(a6.isActive);
            do_check_false(a6.userDisabled);
            do_check_false(a6.appDisabled);
            do_check_eq(a6.pendingOperations, AddonManager.PENDING_NONE);
            do_check_true(isExtensionInAddonsList(profileDir, a6.id));

            end_test();
          });
        });
      });
    });
  });
}

function end_test() {
  do_test_finished();
}
