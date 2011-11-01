/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we rebuild something sensible from a corrupt database


do_load_httpd_js();
var testserver;

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);

// Will be enabled
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

// Will be disabled
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

// Will get a compatibility update and be enabled
var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "1.0",
  name: "Test 3",
  updateURL: "http://localhost:4444/data/test_corrupt.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Will get a compatibility update and be disabled
var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "1.0",
  name: "Test 4",
  updateURL: "http://localhost:4444/data/test_corrupt.rdf",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Stays incompatible
var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "1.0",
  name: "Test 5",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Enabled bootstrapped
var addon6 = {
  id: "addon6@tests.mozilla.org",
  version: "1.0",
  name: "Test 6",
  bootstrap: "true",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// Disabled bootstrapped
var addon7 = {
  id: "addon7@tests.mozilla.org",
  version: "1.0",
  name: "Test 7",
  bootstrap: "true",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// The default theme
var theme1 = {
  id: "theme1@tests.mozilla.org",
  version: "1.0",
  name: "Theme 1",
  internalName: "classic/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "2",
    maxVersion: "2"
  }]
};

// The selected theme
var theme2 = {
  id: "theme2@tests.mozilla.org",
  version: "1.0",
  name: "Theme 2",
  internalName: "test/1.0",
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
  writeInstallRDFForExtension(addon6, profileDir);
  writeInstallRDFForExtension(addon7, profileDir);
  writeInstallRDFForExtension(theme1, profileDir);
  writeInstallRDFForExtension(theme2, profileDir);

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.start(4444);

  // Startup the profile and setup the initial state
  startupManager();

  AddonManager.getAddonsByIDs(["addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon7@tests.mozilla.org",
                               "theme2@tests.mozilla.org"], function([a2, a3, a4,
                                                                      a7, t2]) {
    // Set up the initial state
    a2.userDisabled = true;
    a4.userDisabled = true;
    a7.userDisabled = true;
    t2.userDisabled = false;
    a3.findUpdates({
      onUpdateFinished: function() {
        a4.findUpdates({
          onUpdateFinished: function() {
            restartManager();

            run_test_1();
          }
        }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
      }
    }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
  });
}

function end_test() {
  testserver.stop(do_test_finished);
}

function run_test_1() {
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org",
                               "theme1@tests.mozilla.org",
                               "theme2@tests.mozilla.org"], function([a1, a2, a3,
                                                                      a4, a5, a6,
                                                                      a7, t1, t2]) {
    do_check_neq(a1, null);
    do_check_true(a1.isActive);
    do_check_false(a1.userDisabled);
    do_check_false(a1.appDisabled);
    do_check_eq(a1.pendingOperations, AddonManager.PENDING_NONE);

    do_check_neq(a2, null);
    do_check_false(a2.isActive);
    do_check_true(a2.userDisabled);
    do_check_false(a2.appDisabled);
    do_check_eq(a2.pendingOperations, AddonManager.PENDING_NONE);

    do_check_neq(a3, null);
    do_check_true(a3.isActive);
    do_check_false(a3.userDisabled);
    do_check_false(a3.appDisabled);
    do_check_eq(a3.pendingOperations, AddonManager.PENDING_NONE);

    do_check_neq(a4, null);
    do_check_false(a4.isActive);
    do_check_true(a4.userDisabled);
    do_check_false(a4.appDisabled);
    do_check_eq(a4.pendingOperations, AddonManager.PENDING_NONE);

    do_check_neq(a5, null);
    do_check_false(a5.isActive);
    do_check_false(a5.userDisabled);
    do_check_true(a5.appDisabled);
    do_check_eq(a5.pendingOperations, AddonManager.PENDING_NONE);

    do_check_neq(a6, null);
    do_check_true(a6.isActive);
    do_check_false(a6.userDisabled);
    do_check_false(a6.appDisabled);
    do_check_eq(a6.pendingOperations, AddonManager.PENDING_NONE);

    do_check_neq(a7, null);
    do_check_false(a7.isActive);
    do_check_true(a7.userDisabled);
    do_check_false(a7.appDisabled);
    do_check_eq(a7.pendingOperations, AddonManager.PENDING_NONE);

    do_check_neq(t1, null);
    do_check_false(t1.isActive);
    do_check_true(t1.userDisabled);
    do_check_false(t1.appDisabled);
    do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

    do_check_neq(t2, null);
    do_check_true(t2.isActive);
    do_check_false(t2.userDisabled);
    do_check_false(t2.appDisabled);
    do_check_eq(t2.pendingOperations, AddonManager.PENDING_NONE);

    // After restarting the database won't be open and so can be replaced with
    // a bad file
    restartManager();
    var dbfile = gProfD.clone();
    dbfile.append("extensions.sqlite");
    dbfile.remove(true);
    dbfile.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);

    // Accessing the add-ons should open and recover the database
    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org",
                                 "addon3@tests.mozilla.org",
                                 "addon4@tests.mozilla.org",
                                 "addon5@tests.mozilla.org",
                                 "addon6@tests.mozilla.org",
                                 "addon7@tests.mozilla.org",
                                 "theme1@tests.mozilla.org",
                                 "theme2@tests.mozilla.org"], function([a1, a2, a3,
                                                                        a4, a5, a6,
                                                                        a7, t1, t2]) {
      // Should be correctly recovered
      do_check_neq(a1, null);
      do_check_true(a1.isActive);
      do_check_false(a1.userDisabled);
      do_check_false(a1.appDisabled);
      do_check_eq(a1.pendingOperations, AddonManager.PENDING_NONE);

      // Should be correctly recovered
      do_check_neq(a2, null);
      do_check_false(a2.isActive);
      do_check_true(a2.userDisabled);
      do_check_false(a2.appDisabled);
      do_check_eq(a2.pendingOperations, AddonManager.PENDING_NONE);

      // The compatibility update won't be recovered but it should still be
      // active for this session
      do_check_neq(a3, null);
      do_check_true(a3.isActive);
      do_check_false(a3.userDisabled);
      do_check_true(a3.appDisabled);
      do_check_eq(a3.pendingOperations, AddonManager.PENDING_DISABLE);

      // The compatibility update won't be recovered and it will not have been
      // able to tell that it was previously userDisabled
      do_check_neq(a4, null);
      do_check_false(a4.isActive);
      do_check_false(a4.userDisabled);
      do_check_true(a4.appDisabled);
      do_check_eq(a4.pendingOperations, AddonManager.PENDING_NONE);

      do_check_neq(a5, null);
      do_check_false(a5.isActive);
      do_check_false(a5.userDisabled);
      do_check_true(a5.appDisabled);
      do_check_eq(a5.pendingOperations, AddonManager.PENDING_NONE);

      do_check_neq(a6, null);
      do_check_true(a6.isActive);
      do_check_false(a6.userDisabled);
      do_check_false(a6.appDisabled);
      do_check_eq(a6.pendingOperations, AddonManager.PENDING_NONE);

      do_check_neq(a7, null);
      do_check_false(a7.isActive);
      do_check_true(a7.userDisabled);
      do_check_false(a7.appDisabled);
      do_check_eq(a7.pendingOperations, AddonManager.PENDING_NONE);

      // Should be correctly recovered
      do_check_neq(t1, null);
      do_check_false(t1.isActive);
      do_check_true(t1.userDisabled);
      do_check_false(t1.appDisabled);
      do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

      // Should be correctly recovered
      do_check_neq(t2, null);
      do_check_true(t2.isActive);
      do_check_false(t2.userDisabled);
      do_check_false(t2.appDisabled);
      do_check_eq(t2.pendingOperations, AddonManager.PENDING_NONE);

      restartManager();

      AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                   "addon2@tests.mozilla.org",
                                   "addon3@tests.mozilla.org",
                                   "addon4@tests.mozilla.org",
                                   "addon5@tests.mozilla.org",
                                   "addon6@tests.mozilla.org",
                                   "addon7@tests.mozilla.org",
                                   "theme1@tests.mozilla.org",
                                   "theme2@tests.mozilla.org"], function([a1, a2, a3,
                                                                          a4, a5, a6,
                                                                          a7, t1, t2]) {
        do_check_neq(a1, null);
        do_check_true(a1.isActive);
        do_check_false(a1.userDisabled);
        do_check_false(a1.appDisabled);
        do_check_eq(a1.pendingOperations, AddonManager.PENDING_NONE);

        do_check_neq(a2, null);
        do_check_false(a2.isActive);
        do_check_true(a2.userDisabled);
        do_check_false(a2.appDisabled);
        do_check_eq(a2.pendingOperations, AddonManager.PENDING_NONE);

        do_check_neq(a3, null);
        do_check_false(a3.isActive);
        do_check_false(a3.userDisabled);
        do_check_true(a3.appDisabled);
        do_check_eq(a3.pendingOperations, AddonManager.PENDING_NONE);

        do_check_neq(a4, null);
        do_check_false(a4.isActive);
        do_check_false(a4.userDisabled);
        do_check_true(a4.appDisabled);
        do_check_eq(a4.pendingOperations, AddonManager.PENDING_NONE);

        do_check_neq(a5, null);
        do_check_false(a5.isActive);
        do_check_false(a5.userDisabled);
        do_check_true(a5.appDisabled);
        do_check_eq(a5.pendingOperations, AddonManager.PENDING_NONE);

        do_check_neq(a6, null);
        do_check_true(a6.isActive);
        do_check_false(a6.userDisabled);
        do_check_false(a6.appDisabled);
        do_check_eq(a6.pendingOperations, AddonManager.PENDING_NONE);

        do_check_neq(a7, null);
        do_check_false(a7.isActive);
        do_check_true(a7.userDisabled);
        do_check_false(a7.appDisabled);
        do_check_eq(a7.pendingOperations, AddonManager.PENDING_NONE);

        do_check_neq(t1, null);
        do_check_false(t1.isActive);
        do_check_true(t1.userDisabled);
        do_check_false(t1.appDisabled);
        do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

        do_check_neq(t2, null);
        do_check_true(t2.isActive);
        do_check_false(t2.userDisabled);
        do_check_false(t2.appDisabled);
        do_check_eq(t2.pendingOperations, AddonManager.PENDING_NONE);

        end_test();
      });
    });
  });
}
