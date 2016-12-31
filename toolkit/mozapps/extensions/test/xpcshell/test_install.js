/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-ons can be installed from XPI files
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

// install.rdf size, icon.png, icon64.png size
const ADDON1_SIZE = 705 + 16 + 16;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://testing-common/httpd.js");

var testserver;
var gInstallDate;

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager();
  // Make sure we only register once despite multiple calls
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);

  // Create and configure the HTTP server.
  testserver = new HttpServer();
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerPathHandler("/redirect", function(aRequest, aResponse) {
    aResponse.setStatusLine(null, 301, "Moved Permanently");
    let url = aRequest.host + ":" + aRequest.port + aRequest.queryString;
    aResponse.setHeader("Location", "http://" + url);
  });
  testserver.start(-1);
  gPort = testserver.identity.primaryPort;

  do_test_pending();
  run_test_1();
}

function end_test() {
  testserver.stop(do_test_finished);
}

// Checks that an install from a local file proceeds as expected
function run_test_1() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_install1"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.type, "extension");
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Test 1");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_true(install.addon.hasResource("install.rdf"));
    do_check_neq(install.addon.syncGUID, null);
    do_check_eq(install.addon.install, install);
    do_check_eq(install.addon.size, ADDON1_SIZE);
    do_check_true(hasFlag(install.addon.operationsRequiringRestart,
                          AddonManager.OP_NEEDS_RESTART_INSTALL));
    let file = do_get_addon("test_install1");
    let uri = Services.io.newFileURI(file).spec;
    do_check_eq(install.addon.getResourceURI("install.rdf").spec, "jar:" + uri + "!/install.rdf");
    do_check_eq(install.addon.iconURL, "jar:" + uri + "!/icon.png");
    do_check_eq(install.addon.icon64URL, "jar:" + uri + "!/icon64.png");
    do_check_eq(install.iconURL, null);

    do_check_eq(install.sourceURI.spec, uri);
    do_check_eq(install.addon.sourceURI.spec, uri);

    AddonManager.getAllInstalls(function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      AddonManager.getInstallsByTypes(["foo"], function(fooInstalls) {
        do_check_eq(fooInstalls.length, 0);

        AddonManager.getInstallsByTypes(["extension"], function(extensionInstalls) {
          do_check_eq(extensionInstalls.length, 1);
          do_check_eq(extensionInstalls[0], install);

          prepare_test({
            "addon1@tests.mozilla.org": [
              "onInstalling"
            ]
          }, [
            "onInstallStarted",
            "onInstallEnded",
          ], function() {
            check_test_1(install.addon.syncGUID);
          });
          install.install();
        });
      });
    });
  });
}

function check_test_1(installSyncGUID) {
  ensure_test_completed();
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(olda1) {
    do_check_eq(olda1, null);

    AddonManager.getAddonsWithOperationsByTypes(null, callback_soon(function(pendingAddons) {
      do_check_eq(pendingAddons.length, 1);
      do_check_eq(pendingAddons[0].id, "addon1@tests.mozilla.org");
      let uri = NetUtil.newURI(pendingAddons[0].iconURL);
      if (uri instanceof AM_Ci.nsIJARURI) {
        let jarURI = uri.QueryInterface(AM_Ci.nsIJARURI);
        let archiveURI = jarURI.JARFile;
        let archiveFile = archiveURI.QueryInterface(AM_Ci.nsIFileURL).file;
        let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                        createInstance(Ci.nsIZipReader);
        try {
          zipReader.open(archiveFile);
          do_check_true(zipReader.hasEntry(jarURI.JAREntry));
        } finally {
          zipReader.close();
        }
      } else {
        let iconFile = uri.QueryInterface(AM_Ci.nsIFileURL).file;
        do_check_true(iconFile.exists());
        // Make the iconFile predictably old.
        iconFile.lastModifiedTime = Date.now() - MAKE_FILE_OLD_DIFFERENCE;
      }

      // Make the pending install have a sensible date
      let updateDate = Date.now();
      let extURI = pendingAddons[0].getResourceURI("");
      let ext = extURI.QueryInterface(AM_Ci.nsIFileURL).file;
      setExtensionModifiedTime(ext, updateDate);

      // The pending add-on cannot be disabled or enabled.
      do_check_false(hasFlag(pendingAddons[0].permissions, AddonManager.PERM_CAN_ENABLE));
      do_check_false(hasFlag(pendingAddons[0].permissions, AddonManager.PERM_CAN_DISABLE));

      restartManager();

      AddonManager.getAllInstalls(function(activeInstalls) {
        do_check_eq(activeInstalls, 0);

        AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(function(a1) {
          do_check_neq(a1, null);
          do_check_neq(a1.syncGUID, null);
          do_check_true(a1.syncGUID.length >= 9);
          do_check_eq(a1.syncGUID, installSyncGUID);
          do_check_eq(a1.type, "extension");
          do_check_eq(a1.version, "1.0");
          do_check_eq(a1.name, "Test 1");
          do_check_true(isExtensionInAddonsList(profileDir, a1.id));
          do_check_true(do_get_addon("test_install1").exists());
          do_check_in_crash_annotation(a1.id, a1.version);
          do_check_eq(a1.size, ADDON1_SIZE);
          do_check_false(a1.foreignInstall);

          do_check_eq(a1.sourceURI.spec,
                      Services.io.newFileURI(do_get_addon("test_install1")).spec);
          let difference = a1.installDate.getTime() - updateDate;
          if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
            do_throw("Add-on install time was out by " + difference + "ms");

          difference = a1.updateDate.getTime() - updateDate;
          if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
            do_throw("Add-on update time was out by " + difference + "ms");

          do_check_true(a1.hasResource("install.rdf"));
          do_check_false(a1.hasResource("foo.bar"));

          let uri2 = do_get_addon_root_uri(profileDir, "addon1@tests.mozilla.org");
          do_check_eq(a1.getResourceURI("install.rdf").spec, uri2 + "install.rdf");
          do_check_eq(a1.iconURL, uri2 + "icon.png");
          do_check_eq(a1.icon64URL, uri2 + "icon64.png");

          // Ensure that extension bundle (or icon if unpacked) has updated
          // lastModifiedDate.
          let testURI = a1.getResourceURI(TEST_UNPACKED ? "icon.png" : "");
          let testFile = testURI.QueryInterface(Components.interfaces.nsIFileURL).file;
          do_check_true(testFile.exists());
          difference = testFile.lastModifiedTime - Date.now();
          do_check_true(Math.abs(difference) < MAX_TIME_DIFFERENCE);

          a1.uninstall();
          let { id, version } = a1;
          restartManager();
          do_check_not_in_crash_annotation(id, version);

          do_execute_soon(run_test_2);
        }));
      });
    }));
  });
}

// Tests that an install from a url downloads.
function run_test_2() {
  let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    do_check_neq(install, null);
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Test 2");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);
    do_check_eq(install.iconURL, null);
    do_check_eq(install.sourceURI.spec, url);

    AddonManager.getAllInstalls(function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      prepare_test({}, [
        "onDownloadStarted",
        "onDownloadEnded",
      ], check_test_2);

      install.addListener({
        onDownloadProgress() {
          do_execute_soon(function() {
            Components.utils.forceGC();
          });
        }
      });

      install.install();
    });
  }, "application/x-xpinstall", null, "Test 2", null, "1.0");
}

function check_test_2(install) {
  ensure_test_completed();
  do_check_eq(install.version, "2.0");
  do_check_eq(install.name, "Real Test 2");
  do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_eq(install.addon.install, install);
  do_check_true(hasFlag(install.addon.operationsRequiringRestart,
                        AddonManager.OP_NEEDS_RESTART_INSTALL));
  do_check_eq(install.iconURL, null);

  // Pause the install here and start it again in run_test_3
  do_execute_soon(function() { run_test_3(install); });
  return false;
}

// Tests that the downloaded XPI installs ok
function run_test_3(install) {
  prepare_test({
    "addon2@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ], check_test_3);
  install.install();
}

function check_test_3(aInstall) {
  // Make the pending install have a sensible date
  let updateDate = Date.now();
  let extURI = aInstall.addon.getResourceURI("");
  let ext = extURI.QueryInterface(AM_Ci.nsIFileURL).file;
  setExtensionModifiedTime(ext, updateDate);

  ensure_test_completed();
  AddonManager.getAddonByID("addon2@tests.mozilla.org", callback_soon(function(olda2) {
    do_check_eq(olda2, null);
    restartManager();

    AddonManager.getAllInstalls(function(installs) {
      do_check_eq(installs, 0);

      AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
        do_check_neq(a2, null);
        do_check_neq(a2.syncGUID, null);
        do_check_eq(a2.type, "extension");
        do_check_eq(a2.version, "2.0");
        do_check_eq(a2.name, "Real Test 2");
        do_check_true(isExtensionInAddonsList(profileDir, a2.id));
        do_check_true(do_get_addon("test_install2_1").exists());
        do_check_in_crash_annotation(a2.id, a2.version);
        do_check_eq(a2.sourceURI.spec,
                    "http://localhost:" + gPort + "/addons/test_install2_1.xpi");

        let difference = a2.installDate.getTime() - updateDate;
        if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
          do_throw("Add-on install time was out by " + difference + "ms");

        difference = a2.updateDate.getTime() - updateDate;
        if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
          do_throw("Add-on update time was out by " + difference + "ms");

        gInstallDate = a2.installDate.getTime();

        run_test_4();
      });
    });
  }));
}

// Tests that installing a new version of an existing add-on works
function run_test_4() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install2_2.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.version, "3.0");
    do_check_eq(install.name, "Test 3");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getAllInstalls(function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);
      do_check_eq(install.existingAddon, null);

      prepare_test({}, [
        "onDownloadStarted",
        "onDownloadEnded",
      ], check_test_4);
      install.install();
    });
  }, "application/x-xpinstall", null, "Test 3", null, "3.0");
}

function check_test_4(install) {
  ensure_test_completed();

  do_check_eq(install.version, "3.0");
  do_check_eq(install.name, "Real Test 3");
  do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_neq(install.existingAddon);
  do_check_eq(install.existingAddon.id, "addon2@tests.mozilla.org");
  do_check_eq(install.addon.install, install);
  do_check_true(hasFlag(install.addon.operationsRequiringRestart,
                        AddonManager.OP_NEEDS_RESTART_INSTALL));

  run_test_5();
  // Installation will continue when there is nothing returned.
}

// Continue installing the new version
function run_test_5() {
  prepare_test({
    "addon2@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ], check_test_5);
}

function check_test_5(install) {
  ensure_test_completed();

  do_check_eq(install.existingAddon.pendingUpgrade.install, install);

  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(olda2) {
    do_check_neq(olda2, null);
    do_check_true(hasFlag(olda2.pendingOperations, AddonManager.PENDING_UPGRADE));

    AddonManager.getInstallsByTypes(null, callback_soon(function(installs) {
      do_check_eq(installs.length, 1);
      do_check_eq(installs[0].addon, olda2.pendingUpgrade);
      restartManager();

      AddonManager.getInstallsByTypes(null, function(installs2) {
        do_check_eq(installs2.length, 0);

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          do_check_neq(a2, null);
          do_check_eq(a2.type, "extension");
          do_check_eq(a2.version, "3.0");
          do_check_eq(a2.name, "Real Test 3");
          do_check_true(a2.isActive);
          do_check_true(isExtensionInAddonsList(profileDir, a2.id));
          do_check_true(do_get_addon("test_install2_2").exists());
          do_check_in_crash_annotation(a2.id, a2.version);
          do_check_eq(a2.sourceURI.spec,
                      "http://localhost:" + gPort + "/addons/test_install2_2.xpi");
          do_check_false(a2.foreignInstall);

          do_check_eq(a2.installDate.getTime(), gInstallDate);
          // Update date should be later (or the same if this test is too fast)
          do_check_true(a2.installDate <= a2.updateDate);

          a2.uninstall();
          do_execute_soon(run_test_6);
        });
      });
    }));
  });
}

// Tests that an install that requires a compatibility update works
function run_test_6() {
  restartManager();

  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Real Test 4");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      prepare_test({}, [
        "onDownloadStarted",
        "onDownloadEnded",
      ], check_test_6);
      install.install();
    });
  }, "application/x-xpinstall", null, "Real Test 4", null, "1.0");
}

function check_test_6(install) {
  ensure_test_completed();
  do_check_eq(install.version, "1.0");
  do_check_eq(install.name, "Real Test 4");
  do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_eq(install.existingAddon, null);
  do_check_false(install.addon.appDisabled);
  run_test_7();
  return true;
}

// Continue the install
function run_test_7() {
  prepare_test({
    "addon3@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onInstallStarted",
    "onInstallEnded",
  ], check_test_7);
}

function check_test_7() {
  ensure_test_completed();
  AddonManager.getAddonByID("addon3@tests.mozilla.org", callback_soon(function(olda3) {
    do_check_eq(olda3, null);
    restartManager();

    AddonManager.getAllInstalls(function(installs) {
      do_check_eq(installs, 0);

      AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
        do_check_neq(a3, null);
        do_check_neq(a3.syncGUID, null);
        do_check_eq(a3.type, "extension");
        do_check_eq(a3.version, "1.0");
        do_check_eq(a3.name, "Real Test 4");
        do_check_true(a3.isActive);
        do_check_false(a3.appDisabled);
        do_check_true(isExtensionInAddonsList(profileDir, a3.id));
        do_check_true(do_get_addon("test_install3").exists());
        a3.uninstall();
        do_execute_soon(run_test_8);
      });
    });
  }));
}

function run_test_8() {
  restartManager();

  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);

  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_install3"), function(install) {
    do_check_true(install.addon.isCompatible);

    prepare_test({
      "addon3@tests.mozilla.org": [
        "onInstalling"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], callback_soon(check_test_8));
    install.install();
  });
}

function check_test_8() {
  restartManager();

  AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
    do_check_neq(a3, null);
    do_check_neq(a3.syncGUID, null);
    do_check_eq(a3.type, "extension");
    do_check_eq(a3.version, "1.0");
    do_check_eq(a3.name, "Real Test 4");
    do_check_true(a3.isActive);
    do_check_false(a3.appDisabled);
    do_check_true(isExtensionInAddonsList(profileDir, a3.id));
    do_check_true(do_get_addon("test_install3").exists());
    a3.uninstall();
    do_execute_soon(run_test_9);
  });
}

// Test that after cancelling a download it is removed from the active installs
function run_test_9() {
  restartManager();

  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Real Test 4");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      prepare_test({}, [
        "onDownloadStarted",
        "onDownloadEnded",
      ], check_test_9);
      install.install();
    });
  }, "application/x-xpinstall", null, "Real Test 4", null, "1.0");
}

function check_test_9(install) {
  prepare_test({}, [
    "onDownloadCancelled"
  ], function() {
    let file = install.file;

    // Allow the file removal to complete
    do_execute_soon(function() {
      AddonManager.getAllInstalls(function(activeInstalls) {
        do_check_eq(activeInstalls.length, 0);
        do_check_false(file.exists());

        run_test_10();
      });
    });
  });

  install.cancel();
}

// Tests that after cancelling a pending install it is removed from the active
// installs
function run_test_10() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Real Test 4");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      prepare_test({
        "addon3@tests.mozilla.org": [
          "onInstalling"
        ]
      }, [
        "onDownloadStarted",
        "onDownloadEnded",
        "onInstallStarted",
        "onInstallEnded"
      ], check_test_10);
      install.install();
    });
  }, "application/x-xpinstall", null, "Real Test 4", null, "1.0");
}

function check_test_10(install) {
  prepare_test({
    "addon3@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  }, [
    "onInstallCancelled"
  ]);

  install.cancel();

  ensure_test_completed();

  AddonManager.getAllInstalls(callback_soon(function(activeInstalls) {
    do_check_eq(activeInstalls.length, 0);

    restartManager();

    // Check that the install did not complete
    AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
      do_check_eq(a3, null);

      do_execute_soon(run_test_11);
    });
  }));
}

function run_test_11() {
  // Tests 11 and 12 were removed, to avoid churn of renumbering,
  // just jump ahead to 13 here
  run_test_13();
}


// Tests that cancelling an upgrade leaves the original add-on's pendingOperations
// correct
function run_test_13() {
  restartManager();

  installAllFiles([do_get_addon("test_install2_1")], function() {
    restartManager();

    prepare_test({ }, [
      "onNewInstall"
    ]);

    let url = "http://localhost:" + gPort + "/addons/test_install2_2.xpi";
    AddonManager.getInstallForURL(url, function(install) {
      ensure_test_completed();

      do_check_neq(install, null);
      do_check_eq(install.version, "3.0");
      do_check_eq(install.name, "Test 3");
      do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

      AddonManager.getAllInstalls(function(activeInstalls) {
        do_check_eq(activeInstalls.length, 1);
        do_check_eq(activeInstalls[0], install);
        do_check_eq(install.existingAddon, null);

        prepare_test({
          "addon2@tests.mozilla.org": [
            "onInstalling"
          ]
        }, [
          "onDownloadStarted",
          "onDownloadEnded",
          "onInstallStarted",
          "onInstallEnded",
        ], check_test_13);
        install.install();
      });
    }, "application/x-xpinstall", null, "Test 3", null, "3.0");
  });
}

function check_test_13(install) {
  ensure_test_completed();

  do_check_eq(install.version, "3.0");
  do_check_eq(install.name, "Real Test 3");
  do_check_eq(install.state, AddonManager.STATE_INSTALLED);
  do_check_neq(install.existingAddon, null);
  do_check_eq(install.existingAddon.id, "addon2@tests.mozilla.org");
  do_check_eq(install.addon.install, install);

  AddonManager.getAddonByID("addon2@tests.mozilla.org", callback_soon(function(olda2) {
    do_check_neq(olda2, null);
    do_check_true(hasFlag(olda2.pendingOperations, AddonManager.PENDING_UPGRADE));
    do_check_eq(olda2.pendingUpgrade, install.addon);

    do_check_true(hasFlag(install.addon.pendingOperations,
                          AddonManager.PENDING_INSTALL));

    prepare_test({
      "addon2@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    }, [
      "onInstallCancelled",
    ]);

    install.cancel();

    do_check_false(hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));

    do_check_false(hasFlag(olda2.pendingOperations, AddonManager.PENDING_UPGRADE));
    do_check_eq(olda2.pendingUpgrade, null);

    restartManager();

    // Check that the upgrade did not complete
    AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
      do_check_eq(a2.version, "2.0");

      a2.uninstall();

      do_execute_soon(run_test_14);
    });
  }));
}

// Check that cancelling the install from onDownloadStarted actually cancels it
function run_test_14() {
  restartManager();

  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_eq(install.file, null);

    prepare_test({ }, [
      "onDownloadStarted"
    ], check_test_14);
    install.install();
  }, "application/x-xpinstall");
}

function check_test_14(install) {
  prepare_test({ }, [
    "onDownloadCancelled"
  ], function() {
    let file = install.file;

    install.addListener({
      onDownloadProgress() {
        do_throw("Download should not have continued");
      },
      onDownloadEnded() {
        do_throw("Download should not have continued");
      }
    });

    // Allow the listener to return to see if it continues downloading. The
    // The listener only really tests if we give it time to see progress, the
    // file check isn't ideal either
    do_execute_soon(function() {
      do_check_false(file.exists());

      run_test_15();
    });
  });

  // Wait for the channel to be ready to cancel
  do_execute_soon(function() {
    install.cancel();
  });
}

// Checks that cancelling the install from onDownloadEnded actually cancels it
function run_test_15() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_eq(install.file, null);

    prepare_test({ }, [
      "onDownloadStarted",
      "onDownloadEnded"
    ], check_test_15);
    install.install();
  }, "application/x-xpinstall");
}

function check_test_15(install) {
  prepare_test({ }, [
    "onDownloadCancelled"
  ]);

  install.cancel();

  ensure_test_completed();

  install.addListener({
    onInstallStarted() {
      do_throw("Install should not have continued");
    }
  });

  // Allow the listener to return to see if it starts installing
  do_execute_soon(run_test_16);
}

// Verify that the userDisabled value carries over to the upgrade by default
function run_test_16() {
  restartManager();

  let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallStarted() {
        do_check_false(aInstall.addon.userDisabled);
        aInstall.addon.userDisabled = true;
      },

      onInstallEnded() {
       do_execute_soon(function install2_1_ended() {
        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          do_check_true(a2.userDisabled);
          do_check_false(a2.isActive);

          let url_2 = "http://localhost:" + gPort + "/addons/test_install2_2.xpi";
          AddonManager.getInstallForURL(url_2, function(aInstall_2) {
            aInstall_2.addListener({
              onInstallEnded() {
               do_execute_soon(function install2_2_ended() {
                do_check_true(aInstall_2.addon.userDisabled);

                restartManager();

                AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2_2) {
                  do_check_true(a2_2.userDisabled);
                  do_check_false(a2_2.isActive);

                  a2_2.uninstall();
                  do_execute_soon(run_test_17);
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
}

// Verify that changing the userDisabled value before onInstallEnded works
function run_test_17() {
  restartManager();

  let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallEnded() {
       do_execute_soon(function install2_1_ended2() {
        do_check_false(aInstall.addon.userDisabled);

        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          do_check_false(a2.userDisabled);
          do_check_true(a2.isActive);

          let url_2 = "http://localhost:" + gPort + "/addons/test_install2_2.xpi";
          AddonManager.getInstallForURL(url_2, function(aInstall_2) {
            aInstall_2.addListener({
              onInstallStarted() {
                do_check_false(aInstall_2.addon.userDisabled);
                aInstall_2.addon.userDisabled = true;
              },

              onInstallEnded() {
               do_execute_soon(function install2_2_ended2() {
                restartManager();

                AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2_2) {
                  do_check_true(a2_2.userDisabled);
                  do_check_false(a2_2.isActive);

                  a2_2.uninstall();
                  do_execute_soon(run_test_18);
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
}

// Verify that changing the userDisabled value before onInstallEnded works
function run_test_18() {
  restartManager();

  let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallStarted() {
        do_check_false(aInstall.addon.userDisabled);
        aInstall.addon.userDisabled = true;
      },

      onInstallEnded() {
       do_execute_soon(function install_2_1_ended3() {
        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          do_check_true(a2.userDisabled);
          do_check_false(a2.isActive);

          let url_2 = "http://localhost:" + gPort + "/addons/test_install2_2.xpi";
          AddonManager.getInstallForURL(url_2, function(aInstall_2) {
            aInstall_2.addListener({
              onInstallStarted() {
                do_check_true(aInstall_2.addon.userDisabled);
                aInstall_2.addon.userDisabled = false;
              },

              onInstallEnded() {
               do_execute_soon(function install_2_2_ended3() {
                restartManager();

                AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2_2) {
                  do_check_false(a2_2.userDisabled);
                  do_check_true(a2_2.isActive);

                  a2_2.uninstall();
                  do_execute_soon(run_test_18_1);
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
}


// Checks that metadata is not stored if the pref is set to false
function run_test_18_1() {
  restartManager();

  Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", true);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS,
                             "http://localhost:" + gPort + "/data/test_install.xml");

  Services.prefs.setBoolPref("extensions.addon2@tests.mozilla.org.getAddons.cache.enabled", false);

  let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallEnded(unused, aAddon) {
       do_execute_soon(function test18_1_install_ended() {
        do_check_neq(aAddon.fullDescription, "Repository description");

        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          do_check_neq(a2.fullDescription, "Repository description");

          a2.uninstall();
          do_execute_soon(run_test_19);
        });
       });
      }
    });
    aInstall.install();
  }, "application/x-xpinstall");
}

// Checks that metadata is downloaded for new installs and is visible before and
// after restart
function run_test_19() {
  restartManager();
  Services.prefs.setBoolPref("extensions.addon2@tests.mozilla.org.getAddons.cache.enabled", true);

  let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallEnded(unused, aAddon) {
       do_execute_soon(function test19_install_ended() {
        do_check_eq(aAddon.fullDescription, "Repository description");

        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          do_check_eq(a2.fullDescription, "Repository description");

          a2.uninstall();
          do_execute_soon(run_test_20);
        });
       });
      }
    });
    aInstall.install();
  }, "application/x-xpinstall");
}

// Do the same again to make sure it works when the data is already in the cache
function run_test_20() {
  restartManager();

  let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallEnded(unused, aAddon) {
       do_execute_soon(function test20_install_ended() {
        do_check_eq(aAddon.fullDescription, "Repository description");

        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          do_check_eq(a2.fullDescription, "Repository description");

          a2.uninstall();
          do_execute_soon(run_test_21);
        });
       });
      }
    });
    aInstall.install();
  }, "application/x-xpinstall");
}

// Verify that installing an add-on that is already pending install cancels the
// first install
function run_test_21() {
  restartManager();
  Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", false);

  installAllFiles([do_get_addon("test_install2_1")], function() {
    AddonManager.getAllInstalls(function(aInstalls) {
      do_check_eq(aInstalls.length, 1);

      prepare_test({
        "addon2@tests.mozilla.org": [
          "onOperationCancelled",
          "onInstalling"
        ]
      }, [
        "onNewInstall",
        "onDownloadStarted",
        "onDownloadEnded",
        "onInstallStarted",
        "onInstallCancelled",
        "onInstallEnded",
      ], check_test_21);

      let url = "http://localhost:" + gPort + "/addons/test_install2_1.xpi";
      AddonManager.getInstallForURL(url, function(aInstall) {
        aInstall.install();
      }, "application/x-xpinstall");
    });
  });
}

function check_test_21(aInstall) {
  AddonManager.getAllInstalls(callback_soon(function(aInstalls) {
    do_check_eq(aInstalls.length, 1);
    do_check_eq(aInstalls[0], aInstall);

    prepare_test({
      "addon2@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    }, [
      "onInstallCancelled",
    ]);

    aInstall.cancel();

    ensure_test_completed();

    restartManager();

    AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
      do_check_eq(a2, null);

      run_test_22();
    });
  }));
}

// Tests that an install can be restarted after being cancelled
function run_test_22() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    ensure_test_completed();

    do_check_neq(aInstall, null);
    do_check_eq(aInstall.state, AddonManager.STATE_AVAILABLE);

    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], check_test_22);
    aInstall.install();
  }, "application/x-xpinstall");
}

function check_test_22(aInstall) {
  prepare_test({}, [
    "onDownloadCancelled"
  ]);

  aInstall.cancel();

  ensure_test_completed();

  prepare_test({
    "addon3@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded"
  ], finish_test_22);

  aInstall.install();
}

function finish_test_22(aInstall) {
  prepare_test({
    "addon3@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  }, [
    "onInstallCancelled"
  ]);

  aInstall.cancel();

  ensure_test_completed();

  run_test_23();
}

// Tests that an install can be restarted after being cancelled when a hash
// was provided
function run_test_23() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    ensure_test_completed();

    do_check_neq(aInstall, null);
    do_check_eq(aInstall.state, AddonManager.STATE_AVAILABLE);

    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadEnded",
    ], check_test_23);
    aInstall.install();
  }, "application/x-xpinstall", do_get_addon_hash("test_install3"));
}

function check_test_23(aInstall) {
  prepare_test({}, [
    "onDownloadCancelled"
  ]);

  aInstall.cancel();

  ensure_test_completed();

  prepare_test({
    "addon3@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded"
  ], finish_test_23);

  aInstall.install();
}

function finish_test_23(aInstall) {
  prepare_test({
    "addon3@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  }, [
    "onInstallCancelled"
  ]);

  aInstall.cancel();

  ensure_test_completed();

  run_test_24();
}

// Tests that an install with a bad hash can be restarted after it fails, though
// it will only fail again
function run_test_24() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    ensure_test_completed();

    do_check_neq(aInstall, null);
    do_check_eq(aInstall.state, AddonManager.STATE_AVAILABLE);

    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadFailed",
    ], check_test_24);
    aInstall.install();
  }, "application/x-xpinstall", "sha1:foo");
}

function check_test_24(aInstall) {
  prepare_test({ }, [
    "onDownloadStarted",
    "onDownloadFailed"
  ], run_test_25);

  aInstall.install();
}

// Tests that installs with a hash for a local file work
function run_test_25() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = Services.io.newFileURI(do_get_addon("test_install3")).spec;
  AddonManager.getInstallForURL(url, function(aInstall) {
    ensure_test_completed();

    do_check_neq(aInstall, null);
    do_check_eq(aInstall.state, AddonManager.STATE_DOWNLOADED);
    do_check_eq(aInstall.error, 0);

    prepare_test({ }, [
      "onDownloadCancelled"
    ]);

    aInstall.cancel();

    ensure_test_completed();

    run_test_26();
  }, "application/x-xpinstall", do_get_addon_hash("test_install3"));
}

function run_test_26() {
  prepare_test({ }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadCancelled"
  ]);

  let observerService = AM_Cc["@mozilla.org/network/http-activity-distributor;1"].
                        getService(AM_Ci.nsIHttpActivityDistributor);
  observerService.addObserver({
    observeActivity(aChannel, aType, aSubtype, aTimestamp, aSizeData,
                              aStringData) {
      aChannel.QueryInterface(AM_Ci.nsIChannel);
      // Wait for the final event for the redirected URL
      if (aChannel.URI.spec != "http://localhost:" + gPort + "/addons/test_install1.xpi" ||
          aType != AM_Ci.nsIHttpActivityObserver.ACTIVITY_TYPE_HTTP_TRANSACTION ||
          aSubtype != AM_Ci.nsIHttpActivityObserver.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE)
        return;

      // Request should have been cancelled
      do_check_eq(aChannel.status, Components.results.NS_BINDING_ABORTED);

      observerService.removeObserver(this);

      run_test_27();
    }
  });

  let url = "http://localhost:" + gPort + "/redirect?/addons/test_install1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onDownloadProgress(aDownloadProgressInstall) {
        aDownloadProgressInstall.cancel();
      }
    });

    aInstall.install();
  }, "application/x-xpinstall");
}


// Tests that an install can be restarted during onDownloadCancelled after being
// cancelled in mid-download
function run_test_27() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    ensure_test_completed();

    do_check_neq(aInstall, null);
    do_check_eq(aInstall.state, AddonManager.STATE_AVAILABLE);

    aInstall.addListener({
      onDownloadProgress() {
        aInstall.removeListener(this);
        aInstall.cancel();
      }
    });

    prepare_test({}, [
      "onDownloadStarted",
      "onDownloadCancelled",
    ], check_test_27);
    aInstall.install();
  }, "application/x-xpinstall");
}

function check_test_27(aInstall) {
  prepare_test({
    "addon3@tests.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded"
  ], finish_test_27);

  let file = aInstall.file;
  aInstall.install();
  do_check_neq(file.path, aInstall.file.path);
  do_check_false(file.exists());
}

function finish_test_27(aInstall) {
  prepare_test({
    "addon3@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  }, [
    "onInstallCancelled"
  ]);

  aInstall.cancel();

  ensure_test_completed();

  run_test_28();
}

// Tests that an install that isn't strictly compatible and has
// binary components correctly has appDisabled set (see bug 702868).
function run_test_28() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install5.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Real Test 5");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      prepare_test({}, [
        "onDownloadStarted",
        "onDownloadEnded",
        "onInstallStarted"
      ], check_test_28);
      install.install();
    });
  }, "application/x-xpinstall", null, "Real Test 5", null, "1.0");
}

function check_test_28(install) {
  ensure_test_completed();
  do_check_eq(install.version, "1.0");
  do_check_eq(install.name, "Real Test 5");
  do_check_eq(install.state, AddonManager.STATE_INSTALLING);
  do_check_eq(install.existingAddon, null);
  do_check_false(install.addon.isCompatible);
  do_check_true(install.addon.appDisabled);

  prepare_test({}, [
    "onInstallCancelled"
  ], finish_test_28);
  return false;
}

function finish_test_28(install) {
  prepare_test({}, [
    "onDownloadCancelled"
  ], run_test_29);

  install.cancel();
}

// Tests that an install with a matching compatibility override has appDisabled
// set correctly.
function run_test_29() {
  Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", true);

  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_install6.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Addon Test 6");
    do_check_eq(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      do_check_eq(activeInstalls.length, 1);
      do_check_eq(activeInstalls[0], install);

      prepare_test({}, [
        "onDownloadStarted",
        "onDownloadEnded"
      ], check_test_29);
      install.install();
    });
  }, "application/x-xpinstall", null, "Addon Test 6", null, "1.0");
}

function check_test_29(install) {
  // ensure_test_completed();
  do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_neq(install.addon, null);
  do_check_false(install.addon.isCompatible);
  do_check_true(install.addon.appDisabled);

  prepare_test({}, [
    "onDownloadCancelled"
  ], run_test_30);
  install.cancel();
  return false;
}

// Tests that a multi-package XPI with no add-ons inside shows up as a
// corrupt file
function run_test_30() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_install7"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
    do_check_eq(install.error, AddonManager.ERROR_CORRUPT_FILE);

    run_test_31();
  });
}

// Tests that a multi-package XPI with no valid add-ons inside shows up as a
// corrupt file
function run_test_31() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_install8"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
    do_check_eq(install.error, AddonManager.ERROR_CORRUPT_FILE);

    end_test();
  });
}
