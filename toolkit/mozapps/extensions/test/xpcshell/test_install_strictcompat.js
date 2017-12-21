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
Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);


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
  testserver.start(4444);

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

    Assert.notEqual(install, null);
    Assert.equal(install.type, "extension");
    Assert.equal(install.version, "1.0");
    Assert.equal(install.name, "Test 1");
    Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
    Assert.ok(install.addon.hasResource("install.rdf"));
    Assert.equal(install.addon.install, install);
    Assert.equal(install.addon.size, ADDON1_SIZE);
    Assert.ok(hasFlag(install.addon.operationsRequiringRestart,
                      AddonManager.OP_NEEDS_RESTART_INSTALL));
    let file = do_get_addon("test_install1");
    let uri = Services.io.newFileURI(file).spec;
    Assert.equal(install.addon.getResourceURI("install.rdf").spec, "jar:" + uri + "!/install.rdf");
    Assert.equal(install.addon.iconURL, "jar:" + uri + "!/icon.png");
    Assert.equal(install.addon.icon64URL, "jar:" + uri + "!/icon64.png");
    Assert.equal(install.iconURL, null);

    Assert.equal(install.sourceURI.spec, uri);
    Assert.equal(install.addon.sourceURI.spec, uri);

    AddonManager.getAllInstalls(function(activeInstalls) {
      Assert.equal(activeInstalls.length, 1);
      Assert.equal(activeInstalls[0], install);

      AddonManager.getInstallsByTypes(["foo"], function(fooInstalls) {
        Assert.equal(fooInstalls.length, 0);

        AddonManager.getInstallsByTypes(["extension"], function(extensionInstalls) {
          Assert.equal(extensionInstalls.length, 1);
          Assert.equal(extensionInstalls[0], install);

          prepare_test({
            "addon1@tests.mozilla.org": [
              "onInstalling"
            ]
          }, [
            "onInstallStarted",
            "onInstallEnded",
          ], check_test_1);
          install.install();
        });
      });
    });
  });
}

function check_test_1() {
  ensure_test_completed();
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(olda1) {
    Assert.equal(olda1, null);

    AddonManager.getAddonsWithOperationsByTypes(null, callback_soon(async function(pendingAddons) {
      Assert.equal(pendingAddons.length, 1);
      Assert.equal(pendingAddons[0].id, "addon1@tests.mozilla.org");
      let uri = NetUtil.newURI(pendingAddons[0].iconURL);
      if (uri instanceof AM_Ci.nsIJARURI) {
        let jarURI = uri.QueryInterface(AM_Ci.nsIJARURI);
        let archiveURI = jarURI.JARFile;
        let archiveFile = archiveURI.QueryInterface(AM_Ci.nsIFileURL).file;
        let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                        createInstance(Ci.nsIZipReader);
        try {
          zipReader.open(archiveFile);
          Assert.ok(zipReader.hasEntry(jarURI.JAREntry));
        } finally {
          zipReader.close();
        }
      } else {
        let iconFile = uri.QueryInterface(AM_Ci.nsIFileURL).file;
        Assert.ok(iconFile.exists());
      }

      // Make the pending install have a sensible date
      let updateDate = Date.now();
      let extURI = pendingAddons[0].getResourceURI("");
      let ext = extURI.QueryInterface(AM_Ci.nsIFileURL).file;
      setExtensionModifiedTime(ext, updateDate);

      // The pending add-on cannot be disabled or enabled.
      Assert.ok(!hasFlag(pendingAddons[0].permissions, AddonManager.PERM_CAN_ENABLE));
      Assert.ok(!hasFlag(pendingAddons[0].permissions, AddonManager.PERM_CAN_DISABLE));

      await promiseRestartManager();

      AddonManager.getAllInstalls(function(activeInstalls) {
        Assert.equal(activeInstalls, 0);

        AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
          Assert.notEqual(a1, null);
          Assert.equal(a1.type, "extension");
          Assert.equal(a1.version, "1.0");
          Assert.equal(a1.name, "Test 1");
          Assert.ok(isExtensionInAddonsList(profileDir, a1.id));
          Assert.ok(do_get_addon("test_install1").exists());
          do_check_in_crash_annotation(a1.id, a1.version);
          Assert.equal(a1.size, ADDON1_SIZE);

          Assert.equal(a1.sourceURI.spec,
                       Services.io.newFileURI(do_get_addon("test_install1")).spec);
          let difference = a1.installDate.getTime() - updateDate;
          if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
            do_throw("Add-on install time was out by " + difference + "ms");

          difference = a1.updateDate.getTime() - updateDate;
          if (Math.abs(difference) > MAX_TIME_DIFFERENCE)
            do_throw("Add-on update time was out by " + difference + "ms");

          Assert.ok(a1.hasResource("install.rdf"));
          Assert.ok(!a1.hasResource("foo.bar"));

          let root_uri = do_get_addon_root_uri(profileDir, "addon1@tests.mozilla.org");
          Assert.equal(a1.getResourceURI("install.rdf").spec, root_uri + "install.rdf");
          Assert.equal(a1.iconURL, root_uri + "icon.png");
          Assert.equal(a1.icon64URL, root_uri + "icon64.png");

          a1.uninstall();
          do_execute_soon(function() { run_test_2(a1); });
        });
      });
    }));
  });
}

// Tests that an install from a url downloads.
function run_test_2(aAddon) {
  let { id, version } = aAddon;
  restartManager();
  do_check_not_in_crash_annotation(id, version);

  let url = "http://localhost:4444/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    Assert.notEqual(install, null);
    Assert.equal(install.version, "1.0");
    Assert.equal(install.name, "Test 2");
    Assert.equal(install.state, AddonManager.STATE_AVAILABLE);
    Assert.equal(install.iconURL, null);
    Assert.equal(install.sourceURI.spec, url);

    AddonManager.getAllInstalls(function(activeInstalls) {
      Assert.equal(activeInstalls.length, 1);
      Assert.equal(activeInstalls[0], install);

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
  Assert.equal(install.version, "2.0");
  Assert.equal(install.name, "Real Test 2");
  Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
  Assert.equal(install.addon.install, install);
  Assert.ok(hasFlag(install.addon.operationsRequiringRestart,
                    AddonManager.OP_NEEDS_RESTART_INSTALL));
  Assert.equal(install.iconURL, null);

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
  AddonManager.getAddonByID("addon2@tests.mozilla.org", callback_soon(async function(olda2) {
    Assert.equal(olda2, null);
    await promiseRestartManager();

    AddonManager.getAllInstalls(function(installs) {
      Assert.equal(installs, 0);

      AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
        Assert.notEqual(a2, null);
        Assert.equal(a2.type, "extension");
        Assert.equal(a2.version, "2.0");
        Assert.equal(a2.name, "Real Test 2");
        Assert.ok(isExtensionInAddonsList(profileDir, a2.id));
        Assert.ok(do_get_addon("test_install2_1").exists());
        do_check_in_crash_annotation(a2.id, a2.version);
        Assert.equal(a2.sourceURI.spec,
                     "http://localhost:4444/addons/test_install2_1.xpi");

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

  let url = "http://localhost:4444/addons/test_install2_2.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);
    Assert.equal(install.version, "3.0");
    Assert.equal(install.name, "Test 3");
    Assert.equal(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getAllInstalls(function(activeInstalls) {
      Assert.equal(activeInstalls.length, 1);
      Assert.equal(activeInstalls[0], install);
      Assert.equal(install.existingAddon, null);

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

  Assert.equal(install.version, "3.0");
  Assert.equal(install.name, "Real Test 3");
  Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
  do_check_neq(install.existingAddon);
  Assert.equal(install.existingAddon.id, "addon2@tests.mozilla.org");
  Assert.equal(install.addon.install, install);
  Assert.ok(hasFlag(install.addon.operationsRequiringRestart,
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

  Assert.equal(install.existingAddon.pendingUpgrade.install, install);

  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(olda2) {
    Assert.notEqual(olda2, null);
    Assert.ok(hasFlag(olda2.pendingOperations, AddonManager.PENDING_UPGRADE));

    AddonManager.getInstallsByTypes(null, callback_soon(async function(installs) {
      Assert.equal(installs.length, 1);
      Assert.equal(installs[0].addon, olda2.pendingUpgrade);
      await promiseRestartManager();

      AddonManager.getInstallsByTypes(null, function(installs2) {
        Assert.equal(installs2.length, 0);

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          Assert.notEqual(a2, null);
          Assert.equal(a2.type, "extension");
          Assert.equal(a2.version, "3.0");
          Assert.equal(a2.name, "Real Test 3");
          Assert.ok(a2.isActive);
          Assert.ok(isExtensionInAddonsList(profileDir, a2.id));
          Assert.ok(do_get_addon("test_install2_2").exists());
          do_check_in_crash_annotation(a2.id, a2.version);
          Assert.equal(a2.sourceURI.spec,
                       "http://localhost:4444/addons/test_install2_2.xpi");

          Assert.equal(a2.installDate.getTime(), gInstallDate);
          // Update date should be later (or the same if this test is too fast)
          Assert.ok(a2.installDate <= a2.updateDate);

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

  let url = "http://localhost:4444/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);
    Assert.equal(install.version, "1.0");
    Assert.equal(install.name, "Real Test 4");
    Assert.equal(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      Assert.equal(activeInstalls.length, 1);
      Assert.equal(activeInstalls[0], install);

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
  Assert.equal(install.version, "1.0");
  Assert.equal(install.name, "Real Test 4");
  Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
  Assert.equal(install.existingAddon, null);
  Assert.ok(!install.addon.appDisabled);
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
  AddonManager.getAddonByID("addon3@tests.mozilla.org", callback_soon(async function(olda3) {
    Assert.equal(olda3, null);
    await promiseRestartManager();

    AddonManager.getAllInstalls(function(installs) {
      Assert.equal(installs, 0);

      AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
        Assert.notEqual(a3, null);
        Assert.equal(a3.type, "extension");
        Assert.equal(a3.version, "1.0");
        Assert.equal(a3.name, "Real Test 4");
        Assert.ok(a3.isActive);
        Assert.ok(!a3.appDisabled);
        Assert.ok(isExtensionInAddonsList(profileDir, a3.id));
        Assert.ok(do_get_addon("test_install3").exists());
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
    Assert.ok(install.addon.isCompatible);

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

async function check_test_8() {
  await promiseRestartManager();

  AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
    Assert.notEqual(a3, null);
    Assert.equal(a3.type, "extension");
    Assert.equal(a3.version, "1.0");
    Assert.equal(a3.name, "Real Test 4");
    Assert.ok(a3.isActive);
    Assert.ok(!a3.appDisabled);
    Assert.ok(isExtensionInAddonsList(profileDir, a3.id));
    Assert.ok(do_get_addon("test_install3").exists());
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

  let url = "http://localhost:4444/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);
    Assert.equal(install.version, "1.0");
    Assert.equal(install.name, "Real Test 4");
    Assert.equal(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      Assert.equal(activeInstalls.length, 1);
      Assert.equal(activeInstalls[0], install);

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
        Assert.equal(activeInstalls.length, 0);
        Assert.ok(!file.exists());

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

  let url = "http://localhost:4444/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);
    Assert.equal(install.version, "1.0");
    Assert.equal(install.name, "Real Test 4");
    Assert.equal(install.state, AddonManager.STATE_AVAILABLE);

    AddonManager.getInstallsByTypes(null, function(activeInstalls) {
      Assert.equal(activeInstalls.length, 1);
      Assert.equal(activeInstalls[0], install);

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
    Assert.equal(activeInstalls.length, 0);

    restartManager();

    // Check that the install did not complete
    AddonManager.getAddonByID("addon3@tests.mozilla.org", function(a3) {
      Assert.equal(a3, null);

      run_test_11();
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

    let url = "http://localhost:4444/addons/test_install2_2.xpi";
    AddonManager.getInstallForURL(url, function(install) {
      ensure_test_completed();

      Assert.notEqual(install, null);
      Assert.equal(install.version, "3.0");
      Assert.equal(install.name, "Test 3");
      Assert.equal(install.state, AddonManager.STATE_AVAILABLE);

      AddonManager.getAllInstalls(function(activeInstalls) {
        Assert.equal(activeInstalls.length, 1);
        Assert.equal(activeInstalls[0], install);
        Assert.equal(install.existingAddon, null);

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

  Assert.equal(install.version, "3.0");
  Assert.equal(install.name, "Real Test 3");
  Assert.equal(install.state, AddonManager.STATE_INSTALLED);
  Assert.notEqual(install.existingAddon, null);
  Assert.equal(install.existingAddon.id, "addon2@tests.mozilla.org");
  Assert.equal(install.addon.install, install);

  AddonManager.getAddonByID("addon2@tests.mozilla.org", callback_soon(function(olda2) {
    Assert.notEqual(olda2, null);
    Assert.ok(hasFlag(olda2.pendingOperations, AddonManager.PENDING_UPGRADE));
    Assert.equal(olda2.pendingUpgrade, install.addon);

    Assert.ok(hasFlag(install.addon.pendingOperations,
                      AddonManager.PENDING_INSTALL));

    prepare_test({
      "addon2@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    }, [
      "onInstallCancelled",
    ]);

    install.cancel();

    Assert.ok(!hasFlag(install.addon.pendingOperations, AddonManager.PENDING_INSTALL));

    Assert.ok(!hasFlag(olda2.pendingOperations, AddonManager.PENDING_UPGRADE));
    Assert.equal(olda2.pendingUpgrade, null);

    restartManager();

    // Check that the upgrade did not complete
    AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
      Assert.equal(a2.version, "2.0");

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

  let url = "http://localhost:4444/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    Assert.equal(install.file, null);

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
      Assert.ok(!file.exists());

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

  let url = "http://localhost:4444/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    Assert.equal(install.file, null);

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

  let url = "http://localhost:4444/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallStarted() {
        Assert.ok(!aInstall.addon.userDisabled);
        aInstall.addon.userDisabled = true;
      },

      onInstallEnded() {
       do_execute_soon(function test16_install1() {
        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          Assert.ok(a2.userDisabled);
          Assert.ok(!a2.isActive);

          let url_2 = "http://localhost:4444/addons/test_install2_2.xpi";
          AddonManager.getInstallForURL(url_2, function(aInstall_2) {
            aInstall_2.addListener({
              onInstallEnded() {
               do_execute_soon(function test16_install2() {
                Assert.ok(aInstall_2.addon.userDisabled);

                restartManager();

                AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2_2) {
                  Assert.ok(a2_2.userDisabled);
                  Assert.ok(!a2_2.isActive);

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

  let url = "http://localhost:4444/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallEnded() {
       do_execute_soon(function() {
        Assert.ok(!aInstall.addon.userDisabled);

        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          Assert.ok(!a2.userDisabled);
          Assert.ok(a2.isActive);

          let url_2 = "http://localhost:4444/addons/test_install2_2.xpi";
          AddonManager.getInstallForURL(url_2, function(aInstall_2) {
            aInstall_2.addListener({
              onInstallStarted() {
                Assert.ok(!aInstall_2.addon.userDisabled);
                aInstall_2.addon.userDisabled = true;
              },

              onInstallEnded() {
               do_execute_soon(function() {
                restartManager();

                AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2_2) {
                  Assert.ok(a2_2.userDisabled);
                  Assert.ok(!a2_2.isActive);

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

  let url = "http://localhost:4444/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallStarted() {
        Assert.ok(!aInstall.addon.userDisabled);
        aInstall.addon.userDisabled = true;
      },

      onInstallEnded() {
       do_execute_soon(function test18_install1() {
        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          Assert.ok(a2.userDisabled);
          Assert.ok(!a2.isActive);

          let url_2 = "http://localhost:4444/addons/test_install2_2.xpi";
          AddonManager.getInstallForURL(url_2, function(aInstall_2) {
            aInstall_2.addListener({
              onInstallStarted() {
                Assert.ok(aInstall_2.addon.userDisabled);
                aInstall_2.addon.userDisabled = false;
              },

              onInstallEnded() {
               do_execute_soon(function test18_install2() {
                restartManager();

                AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2_2) {
                  Assert.ok(!a2_2.userDisabled);
                  Assert.ok(a2_2.isActive);

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
                             "http://localhost:4444/data/test_install.xml");

  Services.prefs.setBoolPref("extensions.addon2@tests.mozilla.org.getAddons.cache.enabled", false);

  let url = "http://localhost:4444/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallEnded(unused, aAddon) {
       do_execute_soon(function test18_install() {
        Assert.notEqual(aAddon.fullDescription, "Repository description");

        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          Assert.notEqual(a2.fullDescription, "Repository description");

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

  let url = "http://localhost:4444/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallEnded(unused, aAddon) {
       do_execute_soon(function test19_install() {
        Assert.equal(aAddon.fullDescription, "Repository description");

        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          Assert.equal(a2.fullDescription, "Repository description");

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

  let url = "http://localhost:4444/addons/test_install2_1.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    aInstall.addListener({
      onInstallEnded(unused, aAddon) {
       do_execute_soon(function test20_install() {
        Assert.equal(aAddon.fullDescription, "Repository description");

        restartManager();

        AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
          Assert.equal(a2.fullDescription, "Repository description");

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
      Assert.equal(aInstalls.length, 1);

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

      let url = "http://localhost:4444/addons/test_install2_1.xpi";
      AddonManager.getInstallForURL(url, function(aInstall) {
        aInstall.install();
      }, "application/x-xpinstall");
    });
  });
}

function check_test_21(aInstall) {
  AddonManager.getAllInstalls(callback_soon(function(aInstalls) {
    Assert.equal(aInstalls.length, 1);
    Assert.equal(aInstalls[0], aInstall);

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
      Assert.equal(a2, null);

      run_test_22();
    });
  }));
}

// Tests that an install can be restarted after being cancelled
function run_test_22() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:4444/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    ensure_test_completed();

    Assert.notEqual(aInstall, null);
    Assert.equal(aInstall.state, AddonManager.STATE_AVAILABLE);

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

  let url = "http://localhost:4444/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    ensure_test_completed();

    Assert.notEqual(aInstall, null);
    Assert.equal(aInstall.state, AddonManager.STATE_AVAILABLE);

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

  let url = "http://localhost:4444/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    ensure_test_completed();

    Assert.notEqual(aInstall, null);
    Assert.equal(aInstall.state, AddonManager.STATE_AVAILABLE);

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

    Assert.notEqual(aInstall, null);
    Assert.equal(aInstall.state, AddonManager.STATE_DOWNLOADED);
    Assert.equal(aInstall.error, 0);

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
      if (aChannel.URI.spec != "http://localhost:4444/addons/test_install1.xpi" ||
          aType != AM_Ci.nsIHttpActivityObserver.ACTIVITY_TYPE_HTTP_TRANSACTION ||
          aSubtype != AM_Ci.nsIHttpActivityObserver.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE)
        return;

      // Request should have been cancelled
      Assert.equal(aChannel.status, Components.results.NS_BINDING_ABORTED);

      observerService.removeObserver(this);

      run_test_27();
    }
  });

  let url = "http://localhost:4444/redirect?/addons/test_install1.xpi";
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

  let url = "http://localhost:4444/addons/test_install3.xpi";
  AddonManager.getInstallForURL(url, function(aInstall) {
    ensure_test_completed();

    Assert.notEqual(aInstall, null);
    Assert.equal(aInstall.state, AddonManager.STATE_AVAILABLE);

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
  Assert.notEqual(file.path, aInstall.file.path);
  Assert.ok(!file.exists());
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

  run_test_30();
}

// Tests that a multi-package XPI with no add-ons inside shows up as a
// corrupt file
function run_test_30() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_install7"), function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);
    Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
    Assert.equal(install.error, AddonManager.ERROR_CORRUPT_FILE);

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

    Assert.notEqual(install, null);
    Assert.equal(install.state, AddonManager.STATE_DOWNLOAD_FAILED);
    Assert.equal(install.error, AddonManager.ERROR_CORRUPT_FILE);

    end_test();
  });
}
