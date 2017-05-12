/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update checks work

const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE = "general.useragent.locale";
const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
// This test requires lightweight themes update to be enabled even if the app
// doesn't support lightweight themes.
Services.prefs.setBoolPref("lightweightThemes.update.enabled", true);

Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm");

const PARAMS = "?%REQ_VERSION%/%ITEM_ID%/%ITEM_VERSION%/%ITEM_MAXAPPVERSION%/" +
               "%ITEM_STATUS%/%APP_ID%/%APP_VERSION%/%CURRENT_APP_VERSION%/" +
               "%APP_OS%/%APP_ABI%/%APP_LOCALE%/%UPDATE_TYPE%";

var gInstallDate;

var testserver = createHttpServer();
gPort = testserver.identity.primaryPort;
mapFile("/data/test_update.rdf", testserver);
mapFile("/data/test_update.json", testserver);
mapFile("/data/test_update.xml", testserver);
testserver.registerDirectory("/addons/", do_get_file("addons"));

const profileDir = gProfD.clone();
profileDir.append("extensions");

var originalSyncGUID;

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  Services.prefs.setBoolPref(PREF_MATCH_OS_LOCALE, false);
  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "fr-FR");

  run_next_test();
}

let testParams = [
  { updateFile: "test_update.rdf",
    appId: "xpcshell@tests.mozilla.org" },
  { updateFile: "test_update.json",
    appId: "toolkit@mozilla.org" },
];

for (let test of testParams) {
  let { updateFile, appId } = test;

  add_test(function() {
    writeInstallRDFForExtension({
      id: "addon1@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 1",
    }, profileDir);

    writeInstallRDFForExtension({
      id: "addon2@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "0",
        maxVersion: "0"
      }],
      name: "Test Addon 2",
    }, profileDir);

    writeInstallRDFForExtension({
      id: "addon3@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "5",
        maxVersion: "5"
      }],
      name: "Test Addon 3",
    }, profileDir);

    startupManager();

    run_next_test();
  });

  // Verify that an update is available and can be installed.
  let check_test_1;
  add_test(function run_test_1() {
    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      do_check_neq(a1, null);
      do_check_eq(a1.version, "1.0");
      do_check_eq(a1.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DEFAULT);
      do_check_eq(a1.releaseNotesURI, null);
      do_check_true(a1.foreignInstall);
      do_check_neq(a1.syncGUID, null);

      originalSyncGUID = a1.syncGUID;
      a1.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DEFAULT;

      prepare_test({
        "addon1@tests.mozilla.org": [
          ["onPropertyChanged", ["applyBackgroundUpdates"]]
        ]
      });
      a1.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;
      check_test_completed();

      a1.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;

      prepare_test({}, [
        "onNewInstall",
      ]);

      a1.findUpdates({
        onNoCompatibilityUpdateAvailable(addon) {
          ok(false, "Should not have seen onNoCompatibilityUpdateAvailable notification");
        },

        onUpdateAvailable(addon, install) {
          ensure_test_completed();

          AddonManager.getAllInstalls(function(aInstalls) {
            do_check_eq(aInstalls.length, 1);
            do_check_eq(aInstalls[0], install);

            do_check_eq(addon, a1);
            do_check_eq(install.name, addon.name);
            do_check_eq(install.version, "2.0");
            do_check_eq(install.state, AddonManager.STATE_AVAILABLE);
            do_check_eq(install.existingAddon, addon);
            do_check_eq(install.releaseNotesURI.spec, "http://example.com/updateInfo.xhtml");

            // Verify that another update check returns the same AddonInstall
            a1.findUpdates({
              onNoCompatibilityUpdateAvailable() {
                ok(false, "Should not have seen onNoCompatibilityUpdateAvailable notification");
              },

              onUpdateAvailable(newAddon, newInstall) {
                AddonManager.getAllInstalls(function(aInstalls2) {
                  do_check_eq(aInstalls2.length, 1);
                  do_check_eq(aInstalls2[0], install);
                  do_check_eq(newAddon, addon);
                  do_check_eq(newInstall, install);

                  prepare_test({}, [
                    "onDownloadStarted",
                    "onDownloadEnded",
                  ], check_test_1);
                  install.install();
                });
              },

              onNoUpdateAvailable() {
                ok(false, "Should not have seen onNoUpdateAvailable notification");
              }
            }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
          });
        },

        onNoUpdateAvailable(addon) {
          ok(false, "Should not have seen onNoUpdateAvailable notification");
        }
      }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
    });
  });

  let run_test_2;
  check_test_1 = (install) => {
    ensure_test_completed();
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    run_test_2(install);
    return false;
  };

  // Continue installing the update.
  let check_test_2;
  run_test_2 = (install) => {
    // Verify that another update check returns no new update
    install.existingAddon.findUpdates({
      onNoCompatibilityUpdateAvailable(addon) {
        ok(false, "Should not have seen onNoCompatibilityUpdateAvailable notification");
      },

      onUpdateAvailable() {
        ok(false, "Should find no available update when one is already downloading");
      },

      onNoUpdateAvailable(addon) {
        AddonManager.getAllInstalls(function(aInstalls) {
          do_check_eq(aInstalls.length, 1);
          do_check_eq(aInstalls[0], install);

          prepare_test({
            "addon1@tests.mozilla.org": [
              "onInstalling"
            ]
          }, [
            "onInstallStarted",
            "onInstallEnded",
          ], check_test_2);
          install.install();
        });
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  };

  check_test_2 = () => {
    ensure_test_completed();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(async function(olda1) {
      await AddonTestUtils.loadAddonsList(true);

      do_check_neq(olda1, null);
      do_check_eq(olda1.version, "1.0");
      do_check_true(isExtensionInAddonsList(profileDir, olda1.id));

      shutdownManager();

      await promiseStartupManager();

      do_check_true(isExtensionInAddonsList(profileDir, "addon1@tests.mozilla.org"));

      AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
        do_check_neq(a1, null);
        do_check_eq(a1.version, "2.0");
        do_check_true(isExtensionInAddonsList(profileDir, a1.id));
        do_check_eq(a1.applyBackgroundUpdates, AddonManager.AUTOUPDATE_DISABLE);
        do_check_eq(a1.releaseNotesURI.spec, "http://example.com/updateInfo.xhtml");
        do_check_true(a1.foreignInstall);
        do_check_neq(a1.syncGUID, null);
        do_check_eq(originalSyncGUID, a1.syncGUID);

        // Make sure that the extension lastModifiedTime was updated.
        let testURI = a1.getResourceURI(TEST_UNPACKED ? "install.rdf" : "");
        let testFile = testURI.QueryInterface(Components.interfaces.nsIFileURL).file;
        let difference = testFile.lastModifiedTime - Date.now();
        do_check_true(Math.abs(difference) < MAX_TIME_DIFFERENCE);

        a1.uninstall();
        run_next_test();
      });
    }));
  };

  // Check that an update check finds compatibility updates and applies them
  let check_test_3;
  add_test(function run_test_3() {
    restartManager();

    AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
      do_check_neq(a2, null);
      do_check_true(a2.isActive);
      do_check_true(a2.isCompatible);
      do_check_false(a2.appDisabled);
      do_check_true(a2.isCompatibleWith("0", "0"));

      a2.findUpdates({
        onCompatibilityUpdateAvailable(addon) {
          do_check_true(a2.isCompatible);
          do_check_false(a2.appDisabled);
          do_check_true(a2.isActive);
        },

        onUpdateAvailable(addon, install) {
          ok(false, "Should not have seen an available update");
        },

        onNoUpdateAvailable(addon) {
          do_check_eq(addon, a2);
          do_execute_soon(check_test_3);
        }
      }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
    });
  });

  check_test_3 = () => {
    restartManager();
    AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
      do_check_neq(a2, null);
      do_check_true(a2.isActive);
      do_check_true(a2.isCompatible);
      do_check_false(a2.appDisabled);
      a2.uninstall();

      run_next_test();
    });
  }

  // Checks that we see no compatibility information when there is none.
  add_test(function run_test_4() {
    AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
      do_check_neq(a3, null);
      do_check_false(a3.isActive);
      do_check_false(a3.isCompatible);
      do_check_true(a3.appDisabled);
      do_check_true(a3.isCompatibleWith("5", "5"));
      do_check_false(a3.isCompatibleWith("2", "2"));

      a3.findUpdates({
        sawUpdate: false,
        onCompatibilityUpdateAvailable(addon) {
          ok(false, "Should not have seen compatibility information");
        },

        onNoCompatibilityUpdateAvailable(addon) {
          this.sawUpdate = true;
        },

        onUpdateAvailable(addon, install) {
          ok(false, "Should not have seen an available update");
        },

        onNoUpdateAvailable(addon) {
          do_check_true(this.sawUpdate);
          run_next_test();
        }
      }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
    });
  });

  // Checks that compatibility info for future apps are detected but don't make
  // the item compatibile.
  let check_test_5;
  add_test(function run_test_5() {
    AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
      do_check_neq(a3, null);
      do_check_false(a3.isActive);
      do_check_false(a3.isCompatible);
      do_check_true(a3.appDisabled);
      do_check_true(a3.isCompatibleWith("5", "5"));
      do_check_false(a3.isCompatibleWith("2", "2"));

      a3.findUpdates({
        sawUpdate: false,
        onCompatibilityUpdateAvailable(addon) {
          do_check_false(a3.isCompatible);
          do_check_true(a3.appDisabled);
          do_check_false(a3.isActive);
          this.sawUpdate = true;
        },

        onNoCompatibilityUpdateAvailable(addon) {
          ok(false, "Should have seen some compatibility information");
        },

        onUpdateAvailable(addon, install) {
          ok(false, "Should not have seen an available update");
        },

        onNoUpdateAvailable(addon) {
          do_check_true(this.sawUpdate);
          do_execute_soon(check_test_5);
        }
      }, AddonManager.UPDATE_WHEN_USER_REQUESTED, "3.0", "3.0");
    });
  });

  check_test_5 = () => {
    restartManager();
    AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
      do_check_neq(a3, null);
      do_check_false(a3.isActive);
      do_check_false(a3.isCompatible);
      do_check_true(a3.appDisabled);

      a3.uninstall();
      run_next_test();
    });
  }

  // Test that background update checks work
  let continue_test_6;
  add_test(function run_test_6() {
    restartManager();

    writeInstallRDFForExtension({
      id: "addon1@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 1",
    }, profileDir);
    restartManager();

    prepare_test({}, [
      "onNewInstall",
      "onDownloadStarted",
      "onDownloadEnded"
    ], continue_test_6);

    AddonManagerInternal.backgroundUpdateCheck();
  });

  let check_test_6;
  continue_test_6 = (install) => {
    do_check_neq(install.existingAddon, null);
    do_check_eq(install.existingAddon.id, "addon1@tests.mozilla.org");

    prepare_test({
      "addon1@tests.mozilla.org": [
        "onInstalling"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], callback_soon(check_test_6));
  }

  check_test_6 = (install) => {
    do_check_eq(install.existingAddon.pendingUpgrade.install, install);

    restartManager();
    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      do_check_neq(a1, null);
      do_check_eq(a1.version, "2.0");
      do_check_eq(a1.releaseNotesURI.spec, "http://example.com/updateInfo.xhtml");
      a1.uninstall();
      run_next_test();
    });
  }

  // Verify the parameter escaping in update urls.
  add_test(function run_test_8() {
    restartManager();

    writeInstallRDFForExtension({
      id: "addon1@tests.mozilla.org",
      version: "5.0",
      updateURL: "http://localhost:" + gPort + "/data/param_test.rdf" + PARAMS,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "2"
      }],
      name: "Test Addon 1",
    }, profileDir);

    writeInstallRDFForExtension({
      id: "addon2@tests.mozilla.org",
      version: "67.0.5b1",
      updateURL: "http://localhost:" + gPort + "/data/param_test.rdf" + PARAMS,
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "0",
        maxVersion: "3"
      }],
      name: "Test Addon 2",
    }, profileDir);

    writeInstallRDFForExtension({
      id: "addon3@tests.mozilla.org",
      version: "1.3+",
      updateURL: "http://localhost:" + gPort + "/data/param_test.rdf" + PARAMS,
      targetApplications: [{
        id: appId,
        minVersion: "0",
        maxVersion: "0"
      }, {
        id: "toolkit@mozilla.org",
        minVersion: "0",
        maxVersion: "3"
      }],
      name: "Test Addon 3",
    }, profileDir);

    writeInstallRDFForExtension({
      id: "addon4@tests.mozilla.org",
      version: "0.5ab6",
      updateURL: "http://localhost:" + gPort + "/data/param_test.rdf" + PARAMS,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "5"
      }],
      name: "Test Addon 4",
    }, profileDir);

    writeInstallRDFForExtension({
      id: "addon5@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/param_test.rdf" + PARAMS,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 5",
    }, profileDir);

    writeInstallRDFForExtension({
      id: "addon6@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/param_test.rdf" + PARAMS,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 6",
    }, profileDir);

    restartManager();

    AddonManager.getAddonByID("addon2@tests.mozilla.org", callback_soon(function(a2) {
      a2.userDisabled = true;
      restartManager();

      testserver.registerPathHandler("/data/param_test.rdf", function(request, response) {
        do_check_neq(request.queryString, "");
        let [req_version, item_id, item_version,
             item_maxappversion, item_status,
             app_id, app_version, current_app_version,
             app_os, app_abi, app_locale, update_type] =
             request.queryString.split("/").map(a => decodeURIComponent(a));

        do_check_eq(req_version, "2");

        switch (item_id) {
        case "addon1@tests.mozilla.org":
          do_check_eq(item_version, "5.0");
          do_check_eq(item_maxappversion, "2");
          do_check_eq(item_status, "userEnabled");
          do_check_eq(app_version, "1");
          do_check_eq(update_type, "97");
          break;
        case "addon2@tests.mozilla.org":
          do_check_eq(item_version, "67.0.5b1");
          do_check_eq(item_maxappversion, "3");
          do_check_eq(item_status, "userDisabled");
          do_check_eq(app_version, "1");
          do_check_eq(update_type, "49");
          break;
        case "addon3@tests.mozilla.org":
          do_check_eq(item_version, "1.3+");
          do_check_eq(item_maxappversion, "0");
          do_check_eq(item_status, "userEnabled");
          do_check_eq(app_version, "1");
          do_check_eq(update_type, "112");
          break;
        case "addon4@tests.mozilla.org":
          do_check_eq(item_version, "0.5ab6");
          do_check_eq(item_maxappversion, "5");
          do_check_eq(item_status, "userEnabled");
          do_check_eq(app_version, "2");
          do_check_eq(update_type, "98");
          break;
        case "addon5@tests.mozilla.org":
          do_check_eq(item_version, "1.0");
          do_check_eq(item_maxappversion, "1");
          do_check_eq(item_status, "userEnabled");
          do_check_eq(app_version, "1");
          do_check_eq(update_type, "35");
          break;
        case "addon6@tests.mozilla.org":
          do_check_eq(item_version, "1.0");
          do_check_eq(item_maxappversion, "1");
          do_check_eq(item_status, "userEnabled");
          do_check_eq(app_version, "1");
          do_check_eq(update_type, "99");
          break;
        default:
          ok(false, "Update request for unexpected add-on " + item_id);
        }

        do_check_eq(app_id, "xpcshell@tests.mozilla.org");
        do_check_eq(current_app_version, "1");
        do_check_eq(app_os, "XPCShell");
        do_check_eq(app_abi, "noarch-spidermonkey");
        do_check_eq(app_locale, "fr-FR");

        request.setStatusLine(null, 500, "Server Error");
      });

      AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                   "addon2@tests.mozilla.org",
                                   "addon3@tests.mozilla.org",
                                   "addon4@tests.mozilla.org",
                                   "addon5@tests.mozilla.org",
                                   "addon6@tests.mozilla.org"],
                                   function([a1_2, a2_2, a3_2, a4_2, a5_2, a6_2]) {
        let count = 6;

        function next_test() {
          a1_2.uninstall();
          a2_2.uninstall();
          a3_2.uninstall();
          a4_2.uninstall();
          a5_2.uninstall();
          a6_2.uninstall();

          restartManager();
          run_next_test();
        }

        let compatListener = {
          onUpdateFinished(addon, error) {
            if (--count == 0)
              do_execute_soon(next_test);
          }
        };

        let updateListener = {
          onUpdateAvailable(addon, update) {
            // Dummy so the update checker knows we care about new versions
          },

          onUpdateFinished(addon, error) {
            if (--count == 0)
              do_execute_soon(next_test);
          }
        };

        a1_2.findUpdates(updateListener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
        a2_2.findUpdates(compatListener, AddonManager.UPDATE_WHEN_ADDON_INSTALLED);
        a3_2.findUpdates(updateListener, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
        a4_2.findUpdates(updateListener, AddonManager.UPDATE_WHEN_NEW_APP_DETECTED, "2");
        a5_2.findUpdates(compatListener, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
        a6_2.findUpdates(updateListener, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
      });
    }));
  });

  // Tests that if an install.rdf claims compatibility then the add-on will be
  // seen as compatible regardless of what the update.rdf says.
  add_test(function run_test_9() {
    writeInstallRDFForExtension({
      id: "addon4@tests.mozilla.org",
      version: "5.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "0",
        maxVersion: "1"
      }],
      name: "Test Addon 1",
    }, profileDir);

    restartManager();

    AddonManager.getAddonByID("addon4@tests.mozilla.org", function(a4) {
      do_check_true(a4.isActive, "addon4 is active");
      do_check_true(a4.isCompatible, "addon4 is compatible");

      run_next_test();
    });
  });

  // Tests that a normal update check won't decrease a targetApplication's
  // maxVersion.
  add_test(function run_test_10() {
    AddonManager.getAddonByID("addon4@tests.mozilla.org", function(a4) {
      a4.findUpdates({
        onUpdateFinished(addon) {
          do_check_true(addon.isCompatible, "addon4 is compatible");

          run_next_test();
        }
      }, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
    });
  });

  // Tests that an update check for a new application will decrease a
  // targetApplication's maxVersion.
  add_test(function run_test_11() {
    AddonManager.getAddonByID("addon4@tests.mozilla.org", function(a4) {
      a4.findUpdates({
        onUpdateFinished(addon) {
          do_check_true(addon.isCompatible, "addon4 is not compatible");

          run_next_test();
        }
      }, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
    });
  });

  // Check that the decreased maxVersion applied and disables the add-on
  add_test(function run_test_12() {
    restartManager();

    AddonManager.getAddonByID("addon4@tests.mozilla.org", function(a4) {
      do_check_true(a4.isActive);
      do_check_true(a4.isCompatible);

      a4.uninstall();
      run_next_test();
    });
  });

  // Tests that a compatibility update is passed to the listener when there is
  // compatibility info for the current version of the app but not for the
  // version of the app that the caller requested an update check for, when
  // strict compatibility checking is disabled.
  let check_test_13;
  add_test(function run_test_13() {
    restartManager();

    // Not initially compatible but the update check will make it compatible
    writeInstallRDFForExtension({
      id: "addon7@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "0",
        maxVersion: "0"
      }],
      name: "Test Addon 7",
    }, profileDir);
    restartManager();

    AddonManager.getAddonByID("addon7@tests.mozilla.org", function(a7) {
      do_check_neq(a7, null);
      do_check_true(a7.isActive);
      do_check_true(a7.isCompatible);
      do_check_false(a7.appDisabled);
      do_check_true(a7.isCompatibleWith("0", "0"));

      a7.findUpdates({
        sawUpdate: false,
        onNoCompatibilityUpdateAvailable(addon) {
          ok(false, "Should have seen compatibility information");
        },

        onUpdateAvailable(addon, install) {
          ok(false, "Should not have seen an available update");
        },

        onUpdateFinished(addon) {
          do_check_true(addon.isCompatible);
          do_execute_soon(check_test_13);
        }
      }, AddonManager.UPDATE_WHEN_NEW_APP_DETECTED, "3.0", "3.0");
    });
  });

  check_test_13 = () => {
    restartManager();
    AddonManager.getAddonByID("addon7@tests.mozilla.org", function(a7) {
      do_check_neq(a7, null);
      do_check_true(a7.isActive);
      do_check_true(a7.isCompatible);
      do_check_false(a7.appDisabled);

      a7.uninstall();
      run_next_test();
    });
  }

  // Test that background update checks doesn't update an add-on that isn't
  // allowed to update automatically.
  let check_test_14;
  add_test(function run_test_14() {
    restartManager();

    // Have an add-on there that will be updated so we see some events from it
    writeInstallRDFForExtension({
      id: "addon1@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 1",
    }, profileDir);

    writeInstallRDFForExtension({
      id: "addon8@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 8",
    }, profileDir);
    restartManager();

    AddonManager.getAddonByID("addon8@tests.mozilla.org", function(a8) {
      a8.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;

      // The background update check will find updates for both add-ons but only
      // proceed to install one of them.
      AddonManager.addInstallListener({
        onNewInstall(aInstall) {
          let id = aInstall.existingAddon.id;
          ok((id == "addon1@tests.mozilla.org" || id == "addon8@tests.mozilla.org"),
             "Saw unexpected onNewInstall for " + id);
        },

        onDownloadStarted(aInstall) {
          do_check_eq(aInstall.existingAddon.id, "addon1@tests.mozilla.org");
        },

        onDownloadEnded(aInstall) {
          do_check_eq(aInstall.existingAddon.id, "addon1@tests.mozilla.org");
        },

        onDownloadFailed(aInstall) {
          ok(false, "Should not have seen onDownloadFailed event");
        },

        onDownloadCancelled(aInstall) {
          ok(false, "Should not have seen onDownloadCancelled event");
        },

        onInstallStarted(aInstall) {
          do_check_eq(aInstall.existingAddon.id, "addon1@tests.mozilla.org");
        },

        onInstallEnded(aInstall) {
          do_check_eq(aInstall.existingAddon.id, "addon1@tests.mozilla.org");
          do_check_eq(aInstall.existingAddon.pendingUpgrade.install, aInstall);

          do_execute_soon(check_test_14);
        },

        onInstallFailed(aInstall) {
          ok(false, "Should not have seen onInstallFailed event");
        },

        onInstallCancelled(aInstall) {
          ok(false, "Should not have seen onInstallCancelled event");
        },
      });

      AddonManagerInternal.backgroundUpdateCheck();
    });
  });

  check_test_14 = () => {
    restartManager();
    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon8@tests.mozilla.org"], function([a1, a8]) {
      do_check_neq(a1, null);
      do_check_eq(a1.version, "2.0");
      a1.uninstall();

      do_check_neq(a8, null);
      do_check_eq(a8.version, "1.0");
      a8.uninstall();

      run_next_test();
    });
  }

  // Test that background update checks doesn't update an add-on that is
  // pending uninstall
  let check_test_15;
  add_test(function run_test_15() {
    restartManager();

    // Have an add-on there that will be updated so we see some events from it
    writeInstallRDFForExtension({
      id: "addon1@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 1",
    }, profileDir);

    writeInstallRDFForExtension({
      id: "addon8@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 8",
    }, profileDir);
    restartManager();

    AddonManager.getAddonByID("addon8@tests.mozilla.org", function(a8) {
      a8.uninstall();
      do_check_false(hasFlag(a8.permissions, AddonManager.PERM_CAN_UPGRADE));

      // The background update check will find updates for both add-ons but only
      // proceed to install one of them.
      AddonManager.addInstallListener({
        onNewInstall(aInstall) {
          let id = aInstall.existingAddon.id;
          ok((id == "addon1@tests.mozilla.org" || id == "addon8@tests.mozilla.org"),
             "Saw unexpected onNewInstall for " + id);
        },

        onDownloadStarted(aInstall) {
          do_check_eq(aInstall.existingAddon.id, "addon1@tests.mozilla.org");
        },

        onDownloadEnded(aInstall) {
          do_check_eq(aInstall.existingAddon.id, "addon1@tests.mozilla.org");
        },

        onDownloadFailed(aInstall) {
          ok(false, "Should not have seen onDownloadFailed event");
        },

        onDownloadCancelled(aInstall) {
          ok(false, "Should not have seen onDownloadCancelled event");
        },

        onInstallStarted(aInstall) {
          do_check_eq(aInstall.existingAddon.id, "addon1@tests.mozilla.org");
        },

        onInstallEnded(aInstall) {
          do_check_eq(aInstall.existingAddon.id, "addon1@tests.mozilla.org");
          do_execute_soon(check_test_15);
        },

        onInstallFailed(aInstall) {
          ok(false, "Should not have seen onInstallFailed event");
        },

        onInstallCancelled(aInstall) {
          ok(false, "Should not have seen onInstallCancelled event");
        },
      });

      AddonManagerInternal.backgroundUpdateCheck();
    });
  });

  check_test_15 = () => {
    restartManager();
    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon8@tests.mozilla.org"], function([a1, a8]) {
      do_check_neq(a1, null);
      do_check_eq(a1.version, "2.0");
      a1.uninstall();

      do_check_eq(a8, null);

      run_next_test();
    });
  }

  add_test(function run_test_16() {
    restartManager();

    restartManager();

    let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
    AddonManager.getInstallForURL(url, function(aInstall) {
      aInstall.addListener({
        onInstallEnded() {
         do_execute_soon(function install_2_1_ended() {
          restartManager();

          AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a1) {
            do_check_neq(a1.syncGUID, null);
            let oldGUID = a1.syncGUID;

            let url_2 = "http://localhost:" + gPort + "/addons/test_install2_2.xpi";
            AddonManager.getInstallForURL(url_2, function(aInstall_2) {
              aInstall_2.addListener({
                onInstallEnded() {
                 do_execute_soon(function install_2_2_ended() {
                  restartManager();

                  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
                    do_check_neq(a2.syncGUID, null);
                    do_check_eq(oldGUID, a2.syncGUID);

                    a2.uninstall();
                    run_next_test();
                  });
                 });
                }
              });
              aInstall_2.install();
            }, "application/x-xpinstall");
          });
         });
        }
      });
      aInstall.install();
    }, "application/x-xpinstall");
  });

  // Test that the update check correctly observes the
  // extensions.strictCompatibility pref and compatibility overrides.
  add_test(function run_test_17() {
    restartManager();

    writeInstallRDFForExtension({
      id: "addon9@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "0.1",
        maxVersion: "0.2"
      }],
      name: "Test Addon 9",
    }, profileDir);
    restartManager();

    AddonManager.addInstallListener({
      onNewInstall(aInstall) {
        equal(aInstall.existingAddon.id, "addon9@tests.mozilla.org",
              "Saw unexpected onNewInstall for " + aInstall.existingAddon.id);
        do_check_eq(aInstall.version, "3.0");
      },
      onDownloadFailed(aInstall) {
        AddonManager.getAddonByID("addon9@tests.mozilla.org", function(a9) {
          a9.uninstall();
          run_next_test();
        });
      }
    });

    Services.prefs.setCharPref(PREF_GETADDONS_BYIDS,
                               "http://localhost:" + gPort + "/data/test_update.xml");
    Services.prefs.setCharPref(PREF_GETADDONS_BYIDS_PERFORMANCE,
                               "http://localhost:" + gPort + "/data/test_update.xml");
    Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

    AddonManagerInternal.backgroundUpdateCheck();
  });

  // Tests that compatibility updates are applied to addons when the updated
  // compatibility data wouldn't match with strict compatibility enabled.
  add_test(function run_test_18() {
    restartManager();
    writeInstallRDFForExtension({
      id: "addon10@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "0.1",
        maxVersion: "0.2"
      }],
      name: "Test Addon 10",
    }, profileDir);
    restartManager();

    AddonManager.getAddonByID("addon10@tests.mozilla.org", function(a10) {
      do_check_neq(a10, null);

      a10.findUpdates({
        onNoCompatibilityUpdateAvailable() {
          ok(false, "Should have seen compatibility information");
        },

        onUpdateAvailable() {
          ok(false, "Should not have seen an available update");
        },

        onUpdateFinished() {
          a10.uninstall();
          run_next_test();
        }
      }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
    });
  });

  // Test that the update check correctly observes when an addon opts-in to
  // strict compatibility checking.
  add_test(function run_test_19() {
    restartManager();
    writeInstallRDFForExtension({
      id: "addon11@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "0.1",
        maxVersion: "0.2"
      }],
      name: "Test Addon 11",
    }, profileDir);
    restartManager();

    AddonManager.getAddonByID("addon11@tests.mozilla.org", function(a11) {
      do_check_neq(a11, null);

      a11.findUpdates({
        onCompatibilityUpdateAvailable() {
          ok(false, "Should have not have seen compatibility information");
        },

        onUpdateAvailable() {
          ok(false, "Should not have seen an available update");
        },

        onUpdateFinished() {
          a11.uninstall();
          run_next_test();
        }
     }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
    });
  });

  // Test that the update succeeds when the update.rdf URN contains a type prefix
  // different from the add-on type
  let continue_test_20;
  add_test(function run_test_20() {
    restartManager();
    writeInstallRDFForExtension({
      id: "addon12@tests.mozilla.org",
      version: "1.0",
      updateURL: "http://localhost:" + gPort + "/data/" + updateFile,
      targetApplications: [{
        id: appId,
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 12",
    }, profileDir);
    restartManager();

    prepare_test({}, [
      "onNewInstall",
      "onDownloadStarted",
      "onDownloadEnded"
    ], continue_test_20);

    AddonManagerPrivate.backgroundUpdateCheck();
  });

  let check_test_20;
  continue_test_20 = (install) => {
    do_check_neq(install.existingAddon, null);
    do_check_eq(install.existingAddon.id, "addon12@tests.mozilla.org");

    prepare_test({
      "addon12@tests.mozilla.org": [
        "onInstalling"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], callback_soon(check_test_20));
  }

  check_test_20 = (install) => {
    do_check_eq(install.existingAddon.pendingUpgrade.install, install);

    restartManager();
    AddonManager.getAddonByID("addon12@tests.mozilla.org", function(a12) {
      do_check_neq(a12, null);
      do_check_eq(a12.version, "2.0");
      do_check_eq(a12.type, "extension");
      a12.uninstall();

      do_execute_soon(() => {
        restartManager();
        run_next_test()
      });
    });
  }

  add_task(async function cleanup() {
    let addons = await AddonManager.getAddonsByTypes(["extension"]);

    for (let addon of addons)
      addon.uninstall();

    await promiseRestartManager();

    shutdownManager();

    await new Promise(do_execute_soon);
  });
}

// Test that background update checks work for lightweight themes
add_test(function run_test_7() {
  startupManager();

  LightweightThemeManager.currentTheme = {
    id: "1",
    version: "1",
    name: "Test LW Theme",
    description: "A test theme",
    author: "Mozilla",
    homepageURL: "http://localhost:" + gPort + "/data/index.html",
    headerURL: "http://localhost:" + gPort + "/data/header.png",
    footerURL: "http://localhost:" + gPort + "/data/footer.png",
    previewURL: "http://localhost:" + gPort + "/data/preview.png",
    iconURL: "http://localhost:" + gPort + "/data/icon.png",
    updateURL: "http://localhost:" + gPort + "/data/lwtheme.js"
  };

  // XXX The lightweight theme manager strips non-https updateURLs so hack it
  // back in.
  let themes = JSON.parse(Services.prefs.getCharPref("lightweightThemes.usedThemes"));
  do_check_eq(themes.length, 1);
  themes[0].updateURL = "http://localhost:" + gPort + "/data/lwtheme.js";
  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(themes));

  testserver.registerPathHandler("/data/lwtheme.js", function(request, response) {
    // Server will specify an expiry in one year.
    let expiry = new Date();
    expiry.setFullYear(expiry.getFullYear() + 1);
    response.setHeader("Expires", expiry.toUTCString(), false);
    response.write(JSON.stringify({
      id: "1",
      version: "2",
      name: "Updated Theme",
      description: "A test theme",
      author: "Mozilla",
      homepageURL: "http://localhost:" + gPort + "/data/index2.html",
      headerURL: "http://localhost:" + gPort + "/data/header.png",
      footerURL: "http://localhost:" + gPort + "/data/footer.png",
      previewURL: "http://localhost:" + gPort + "/data/preview.png",
      iconURL: "http://localhost:" + gPort + "/data/icon2.png",
      updateURL: "http://localhost:" + gPort + "/data/lwtheme.js"
    }));
  });

  AddonManager.getAddonByID("1@personas.mozilla.org", function(p1) {
    do_check_neq(p1, null);
    do_check_eq(p1.version, "1");
    do_check_eq(p1.name, "Test LW Theme");
    do_check_true(p1.isActive);
    do_check_eq(p1.installDate.getTime(), p1.updateDate.getTime());

    // 5 seconds leeway seems like a lot, but tests can run slow and really if
    // this is within 5 seconds it is fine. If it is going to be wrong then it
    // is likely to be hours out at least
    do_check_true((Date.now() - p1.installDate.getTime()) < 5000);

    gInstallDate = p1.installDate.getTime();

    prepare_test({
      "1@personas.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onExternalInstall"
    ], check_test_7);

    AddonManagerInternal.backgroundUpdateCheck();
  });
});

function check_test_7() {
  AddonManager.getAddonByID("1@personas.mozilla.org", function(p1) {
    do_check_neq(p1, null);
    do_check_eq(p1.version, "2");
    do_check_eq(p1.name, "Updated Theme");
    do_check_eq(p1.installDate.getTime(), gInstallDate);
    do_check_true(p1.installDate.getTime() < p1.updateDate.getTime());

    // 5 seconds leeway seems like a lot, but tests can run slow and really if
    // this is within 5 seconds it is fine. If it is going to be wrong then it
    // is likely to be hours out at least
    do_check_true((Date.now() - p1.updateDate.getTime()) < 5000);

    gInstallDate = p1.installDate.getTime();

    run_next_test();
  });
}

// Test that background update checks for lightweight themes do not use the cache
// The update body from test 7 shouldn't be used since the cache should be bypassed.
add_test(function() {
  // XXX The lightweight theme manager strips non-https updateURLs so hack it
  // back in.
  let themes = JSON.parse(Services.prefs.getCharPref("lightweightThemes.usedThemes"));
  do_check_eq(themes.length, 1);
  themes[0].updateURL = "http://localhost:" + gPort + "/data/lwtheme.js";
  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(themes));

  testserver.registerPathHandler("/data/lwtheme.js", function(request, response) {
    response.write(JSON.stringify({
      id: "1",
      version: "3",
      name: "Updated Theme v.3",
      description: "A test theme v.3",
      author: "John Smith",
      homepageURL: "http://localhost:" + gPort + "/data/index3.html?v=3",
      headerURL: "http://localhost:" + gPort + "/data/header.png?v=3",
      footerURL: "http://localhost:" + gPort + "/data/footer.png?v=3",
      previewURL: "http://localhost:" + gPort + "/data/preview.png?v=3",
      iconURL: "http://localhost:" + gPort + "/data/icon2.png?v=3",
      updateURL: "https://localhost:" + gPort + "/data/lwtheme.js?v=3"
    }));
  });

  AddonManager.getAddonByID("1@personas.mozilla.org", function(p1) {
    do_check_neq(p1, null);
    do_check_eq(p1.version, "2");
    do_check_eq(p1.name, "Updated Theme");
    do_check_true(p1.isActive);
    do_check_eq(p1.installDate.getTime(), gInstallDate);
    do_check_true(p1.installDate.getTime() < p1.updateDate.getTime());

    prepare_test({
      "1@personas.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onExternalInstall"
    ], check_test_7_cache);

    AddonManagerInternal.backgroundUpdateCheck();
  });
});

function check_test_7_cache() {
  AddonManager.getAddonByID("1@personas.mozilla.org", function(p1) {
    let currentTheme = LightweightThemeManager.currentTheme;
    do_check_neq(p1, null);
    do_check_eq(p1.version, "3");
    do_check_eq(p1.name, "Updated Theme v.3");
    do_check_eq(p1.description, "A test theme v.3");
    do_print(JSON.stringify(p1));
    do_check_eq(p1.creator.name, "John Smith");
    do_check_eq(p1.homepageURL, "http://localhost:" + gPort + "/data/index3.html?v=3");
    do_check_eq(p1.screenshots[0].url, "http://localhost:" + gPort + "/data/preview.png?v=3");
    do_check_eq(p1.iconURL, "http://localhost:" + gPort + "/data/icon2.png?v=3");
    do_check_eq(currentTheme.headerURL, "http://localhost:" + gPort + "/data/header.png?v=3");
    do_check_eq(currentTheme.footerURL, "http://localhost:" + gPort + "/data/footer.png?v=3");
    do_check_eq(currentTheme.updateURL, "https://localhost:" + gPort + "/data/lwtheme.js?v=3");

    do_check_eq(p1.installDate.getTime(), gInstallDate);
    do_check_true(p1.installDate.getTime() < p1.updateDate.getTime());

    run_next_test();
  });
}

// Test that the update check returns nothing for addons in locked install
// locations.
add_test(function run_test_locked_install() {
  const lockedDir = gProfD.clone();
  lockedDir.append("locked_extensions");
  registerDirectory("XREAppFeat", lockedDir);
  restartManager();
  writeInstallRDFForExtension({
    id: "addon13@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:" + gPort + "/data/test_update.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "0.1",
      maxVersion: "0.2"
    }],
    name: "Test Addon 13",
  }, lockedDir);
  restartManager();

  AddonManager.getAddonByID("addon13@tests.mozilla.org", function(a13) {
    do_check_neq(a13, null);

    a13.findUpdates({
      onCompatibilityUpdateAvailable() {
        ok(false, "Should have not have seen compatibility information");
      },

      onUpdateAvailable() {
        ok(false, "Should not have seen an available update");
      },

      onUpdateFinished() {
        ok(true, "Should have seen an onUpdateFinished");
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });

  AddonManager.getAllInstalls(aInstalls => {
    do_check_eq(aInstalls.length, 0);
  });
  run_next_test();
});
