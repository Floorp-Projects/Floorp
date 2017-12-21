/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that bootstrappable add-ons can be used without restarts.
Components.utils.import("resource://gre/modules/Services.jsm");

// Enable loading extensions from the user scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_USER);

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");
const userExtDir = gProfD.clone();
userExtDir.append("extensions2");
userExtDir.append(gAppInfo.ID);
registerDirectory("XREUSysExt", userExtDir.parent);

Components.utils.import("resource://testing-common/httpd.js");
// Create and configure the HTTP server.
var testserver = new HttpServer();
testserver.start(-1);
gPort = testserver.identity.primaryPort;

// register files with server
testserver.registerDirectory("/addons/", do_get_file("addons"));
mapFile("/data/test_dictionary.rdf", testserver);

/**
 * This object is both a factory and an mozISpellCheckingEngine implementation (so, it
 * is de-facto a service). It's also an interface requestor that gives out
 * itself when asked for mozISpellCheckingEngine.
 */
var HunspellEngine = {
  dictionaryDirs: [],
  listener: null,

  QueryInterface: function hunspell_qi(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIFactory) ||
        iid.equals(Components.interfaces.mozISpellCheckingEngine))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  createInstance: function hunspell_ci(outer, iid) {
    if (outer)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
  lockFactory: function hunspell_lockf(lock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  addDirectory: function hunspell_addDirectory(dir) {
    this.dictionaryDirs.push(dir);
    if (this.listener)
      this.listener("addDirectory");
  },

  removeDirectory: function hunspell_addDirectory(dir) {
    this.dictionaryDirs.splice(this.dictionaryDirs.indexOf(dir), 1);
    if (this.listener)
      this.listener("removeDirectory");
  },

  getInterface: function hunspell_gi(iid) {
    if (iid.equals(Components.interfaces.mozISpellCheckingEngine))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  contractID: "@mozilla.org/spellchecker/engine;1",
  classID: Components.ID("{6f3c63bc-a4fd-449b-9a58-a2d9bd972cce}"),

  activate: function hunspell_activate() {
    this.origClassID = Components.manager.nsIComponentRegistrar
      .contractIDToCID(this.contractID);
    this.origFactory = Components.manager
      .getClassObject(Components.classes[this.contractID],
                      Components.interfaces.nsIFactory);

    Components.manager.nsIComponentRegistrar
      .unregisterFactory(this.origClassID, this.origFactory);
    Components.manager.nsIComponentRegistrar.registerFactory(this.classID,
      "Test hunspell", this.contractID, this);
  },

  deactivate: function hunspell_deactivate() {
    Components.manager.nsIComponentRegistrar.unregisterFactory(this.classID, this);
    Components.manager.nsIComponentRegistrar.registerFactory(this.origClassID,
      "Hunspell", this.contractID, this.origFactory);
  },

  isDictionaryEnabled: function hunspell_isDictionaryEnabled(name) {
    return this.dictionaryDirs.some(function(dir) {
      var dic = dir.clone();
      dic.append(name);
      return dic.exists();
    });
  }
};

function run_test() {
  do_test_pending();

  startupManager();

  run_test_1();
}

// Tests that installing doesn't require a restart
function run_test_1() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  HunspellEngine.activate();

  AddonManager.getInstallForFile(do_get_addon("test_dictionary"), function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);
    Assert.equal(install.type, "dictionary");
    Assert.equal(install.version, "1.0");
    Assert.equal(install.name, "Test Dictionary");
    Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
    Assert.ok(install.addon.hasResource("install.rdf"));
    Assert.ok(!install.addon.hasResource("bootstrap.js"));
    Assert.equal(install.addon.operationsRequiringRestart &
                 AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
    do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    let addon = install.addon;
    prepare_test({
      "ab-CD@dictionaries.addons.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], function() {
      Assert.ok(addon.hasResource("install.rdf"));
      HunspellEngine.listener = function(aEvent) {
        HunspellEngine.listener = null;
        Assert.equal(aEvent, "addDirectory");
        executeSoon(check_test_1);
      };
    });
    install.install();
  });
}

function check_test_1() {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    Assert.equal(installs.length, 0);

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
      Assert.notEqual(b1, null);
      Assert.equal(b1.version, "1.0");
      Assert.ok(!b1.appDisabled);
      Assert.ok(!b1.userDisabled);
      Assert.ok(b1.isActive);
      Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
      Assert.ok(b1.hasResource("install.rdf"));
      Assert.ok(!b1.hasResource("bootstrap.js"));
      do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

      let chromeReg = AM_Cc["@mozilla.org/chrome/chrome-registry;1"].
                      getService(AM_Ci.nsIChromeRegistry);
      try {
        chromeReg.convertChromeURL(NetUtil.newURI("chrome://dict/content/dict.xul"));
        do_throw("Chrome manifest should not have been registered");
      } catch (e) {
        // Expected the chrome url to not be registered
      }

      AddonManager.getAddonsWithOperationsByTypes(null, function(list) {
        Assert.equal(list.length, 0);

        run_test_2();
      });
    });
  });
}

// Tests that disabling doesn't require a restart
function run_test_2() {
  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    prepare_test({
      "ab-CD@dictionaries.addons.mozilla.org": [
        ["onDisabling", false],
        "onDisabled"
      ]
    });

    Assert.equal(b1.operationsRequiringRestart &
                 AddonManager.OP_NEEDS_RESTART_DISABLE, 0);
    b1.userDisabled = true;
    ensure_test_completed();

    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(b1.userDisabled);
    Assert.ok(!b1.isActive);
    Assert.ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
    do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(newb1) {
      Assert.notEqual(newb1, null);
      Assert.equal(newb1.version, "1.0");
      Assert.ok(!newb1.appDisabled);
      Assert.ok(newb1.userDisabled);
      Assert.ok(!newb1.isActive);

      executeSoon(run_test_3);
    });
  });
}

// Test that restarting doesn't accidentally re-enable
function run_test_3() {
  shutdownManager();
  Assert.ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  startupManager(false);
  Assert.ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(b1.userDisabled);
    Assert.ok(!b1.isActive);

    run_test_4();
  });
}

// Tests that enabling doesn't require a restart
function run_test_4() {
  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    prepare_test({
      "ab-CD@dictionaries.addons.mozilla.org": [
        ["onEnabling", false],
        "onEnabled"
      ]
    });

    Assert.equal(b1.operationsRequiringRestart &
                 AddonManager.OP_NEEDS_RESTART_ENABLE, 0);
    b1.userDisabled = false;
    ensure_test_completed();

    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
    do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(newb1) {
      Assert.notEqual(newb1, null);
      Assert.equal(newb1.version, "1.0");
      Assert.ok(!newb1.appDisabled);
      Assert.ok(!newb1.userDisabled);
      Assert.ok(newb1.isActive);

      executeSoon(run_test_5);
    });
  });
}

// Tests that a restart shuts down and restarts the add-on
function run_test_5() {
  shutdownManager();
  Assert.ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");
  startupManager(false);
  Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(!isExtensionInAddonsList(profileDir, b1.id));

    run_test_7();
  });
}

// Tests that uninstalling doesn't require a restart
function run_test_7() {
  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    prepare_test({
      "ab-CD@dictionaries.addons.mozilla.org": [
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });

    Assert.equal(b1.operationsRequiringRestart &
                 AddonManager.OP_NEEDS_RESTART_UNINSTALL, 0);
    b1.uninstall();

    check_test_7();
  });
}

function check_test_7() {
  ensure_test_completed();
  Assert.ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org",
   callback_soon(function(b1) {
    Assert.equal(b1, null);

    restartManager();

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(newb1) {
      Assert.equal(newb1, null);

      executeSoon(run_test_8);
    });
  }));
}

// Test that a bootstrapped extension dropped into the profile loads properly
// on startup and doesn't cause an EM restart
function run_test_8() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("ab-CD@dictionaries.addons.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_dictionary"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir.permissions |= FileUtils.PERMS_FILE;
  dir = dir.parent;
  dir.append("dictionaries");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  dir.append("ab-CD.dic");
  zip.extract("dictionaries/ab-CD.dic", dir);
  dir.permissions |= FileUtils.PERMS_FILE;
  zip.close();

  startupManager(false);

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
    do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    executeSoon(run_test_9);
  });
}

// Test that items detected as removed during startup get removed properly
function run_test_9() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("ab-CD@dictionaries.addons.mozilla.org");
  dir.remove(true);
  startupManager(false);

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    Assert.equal(b1, null);
    do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    executeSoon(run_test_12);
  });
}


// Tests that bootstrapped extensions are correctly loaded even if the app is
// upgraded at the same time
function run_test_12() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("ab-CD@dictionaries.addons.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_dictionary"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir = dir.parent;
  dir.append("dictionaries");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  dir.append("ab-CD.dic");
  zip.extract("dictionaries/ab-CD.dic", dir);
  zip.close();

  startupManager(true);

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(!b1.appDisabled);
    Assert.ok(!b1.userDisabled);
    Assert.ok(b1.isActive);
    Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
    do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    b1.uninstall();
    executeSoon(run_test_16);
  });
}


// Tests that bootstrapped extensions don't get loaded when in safe mode
function run_test_16() {
  restartManager();

  installAllFiles([do_get_addon("test_dictionary")], function() {
    // spin the event loop to let the addon finish starting
   executeSoon(function check_installed_dictionary() {
    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org",
     callback_soon(function(b1) {
      // Should have installed and started
      Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

      shutdownManager();

      // Should have stopped
      Assert.ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

      gAppInfo.inSafeMode = true;
      startupManager(false);

      AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org",
       callback_soon(function(b1_2) {
        // Should still be stopped
        Assert.ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
        Assert.ok(!b1_2.isActive);

        shutdownManager();
        gAppInfo.inSafeMode = false;
        startupManager(false);

        // Should have started
        Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

        AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1_3) {
          b1_3.uninstall();

          executeSoon(run_test_17);
        });
      }));
    }));
   });
  });
}

// Check that a bootstrapped extension in a non-profile location is loaded
function run_test_17() {
  shutdownManager();

  let dir = userExtDir.clone();
  dir.append("ab-CD@dictionaries.addons.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_dictionary"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir.permissions |= FileUtils.PERMS_FILE;
  dir = dir.parent;
  dir.append("dictionaries");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  dir.append("ab-CD.dic");
  zip.extract("dictionaries/ab-CD.dic", dir);
  dir.permissions |= FileUtils.PERMS_FILE;
  zip.close();

  startupManager();

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org",
   callback_soon(function(b1) {
    // Should have installed and started
    Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "1.0");
    Assert.ok(b1.isActive);

    // From run_test_21
    dir = userExtDir.clone();
    dir.append("ab-CD@dictionaries.addons.mozilla.org");
    dir.remove(true);

    restartManager();

    run_test_23();
  }));
}

// Tests that installing from a URL doesn't require a restart
function run_test_23() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:" + gPort + "/addons/test_dictionary.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    Assert.notEqual(install, null);

    prepare_test({ }, [
      "onDownloadStarted",
      "onDownloadEnded"
    ], function() {
      Assert.equal(install.type, "dictionary");
      Assert.equal(install.version, "1.0");
      Assert.equal(install.name, "Test Dictionary");
      Assert.equal(install.state, AddonManager.STATE_DOWNLOADED);
      Assert.ok(install.addon.hasResource("install.rdf"));
      Assert.ok(!install.addon.hasResource("bootstrap.js"));
      Assert.equal(install.addon.operationsRequiringRestart &
                   AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
      do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

      let addon = install.addon;
      prepare_test({
        "ab-CD@dictionaries.addons.mozilla.org": [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], function() {
        Assert.ok(addon.hasResource("install.rdf"));
        // spin to let the addon startup finish
        executeSoon(check_test_23);
      });
    });
    install.install();
  }, "application/x-xpinstall");
}

function check_test_23() {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    Assert.equal(installs.length, 0);

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
      Assert.notEqual(b1, null);
      Assert.equal(b1.version, "1.0");
      Assert.ok(!b1.appDisabled);
      Assert.ok(!b1.userDisabled);
      Assert.ok(b1.isActive);
      Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
      Assert.ok(b1.hasResource("install.rdf"));
      Assert.ok(!b1.hasResource("bootstrap.js"));
      do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

      AddonManager.getAddonsWithOperationsByTypes(null, callback_soon(function(list) {
        Assert.equal(list.length, 0);

        restartManager();
        AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1_2) {
          b1_2.uninstall();
          executeSoon(run_test_25);
        });
      }));
    });
  });
}

// Tests that updating from a bootstrappable add-on to a normal add-on calls
// the uninstall method
function run_test_25() {
  restartManager();

  HunspellEngine.listener = function(aEvent) {
    HunspellEngine.listener = null;
    Assert.equal(aEvent, "addDirectory");
    Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

    installAllFiles([do_get_addon("test_dictionary_2")], function test_25_installed2() {
      // Needs a restart to complete this so the old version stays running
      Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

      AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org",
       callback_soon(function(b1) {
        Assert.notEqual(b1, null);
        Assert.equal(b1.version, "1.0");
        Assert.ok(b1.isActive);
        Assert.ok(hasFlag(b1.pendingOperations, AddonManager.PENDING_UPGRADE));

        restartManager();

        Assert.ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

        AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1_2) {
          Assert.notEqual(b1_2, null);
          Assert.equal(b1_2.version, "2.0");
          Assert.ok(b1_2.isActive);
          Assert.equal(b1_2.pendingOperations, AddonManager.PENDING_NONE);

          executeSoon(run_test_26);
        });
      }));
    });
  };

  installAllFiles([do_get_addon("test_dictionary")], function test_25_installed() { });
}

// Tests that updating from a normal add-on to a bootstrappable add-on calls
// the install method
function run_test_26() {
  installAllFiles([do_get_addon("test_dictionary")], function test_26_install() {
    // Needs a restart to complete this
    Assert.ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org",
     callback_soon(function(b1) {
      Assert.notEqual(b1, null);
      Assert.equal(b1.version, "2.0");
      Assert.ok(b1.isActive);
      Assert.ok(hasFlag(b1.pendingOperations, AddonManager.PENDING_UPGRADE));

      restartManager();

      Assert.ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

      AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1_2) {
        Assert.notEqual(b1_2, null);
        Assert.equal(b1_2.version, "1.0");
        Assert.ok(b1_2.isActive);
        Assert.equal(b1_2.pendingOperations, AddonManager.PENDING_NONE);

        HunspellEngine.deactivate();
        b1_2.uninstall();
        executeSoon(run_test_27);
      });
    }));
  });
}

// Tests that an update check from a normal add-on to a bootstrappable add-on works
function run_test_27() {
  restartManager();
  writeInstallRDFForExtension({
    id: "ab-CD@dictionaries.addons.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:" + gPort + "/data/test_dictionary.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Dictionary",
  }, profileDir);
  restartManager();

  prepare_test({
    "ab-CD@dictionaries.addons.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded"
  ], callback_soon(check_test_27));

  AddonManagerPrivate.backgroundUpdateCheck();
}

function check_test_27(install) {
  Assert.equal(install.existingAddon.pendingUpgrade.install, install);

  restartManager();
  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    Assert.notEqual(b1, null);
    Assert.equal(b1.version, "2.0");
    Assert.equal(b1.type, "dictionary");
    b1.uninstall();
    executeSoon(run_test_28);
  });
}

// Tests that an update check from a bootstrappable add-on to a normal add-on works
function run_test_28() {
  restartManager();

  writeInstallRDFForExtension({
    id: "ef@dictionaries.addons.mozilla.org",
    version: "1.0",
    type: "64",
    updateURL: "http://localhost:" + gPort + "/data/test_dictionary.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Dictionary ef",
  }, profileDir);
  restartManager();

  prepare_test({
    "ef@dictionaries.addons.mozilla.org": [
      "onInstalling"
    ]
  }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded"
  ], callback_soon(check_test_28));

  AddonManagerPrivate.backgroundUpdateCheck();
}

function check_test_28(install) {
  Assert.equal(install.existingAddon.pendingUpgrade.install, install);

  restartManager();
  AddonManager.getAddonByID("ef@dictionaries.addons.mozilla.org", function(b2) {
    Assert.notEqual(b2, null);
    Assert.equal(b2.version, "2.0");
    Assert.equal(b2.type, "extension");
    b2.uninstall();
    executeSoon(run_test_29);
  });
}

// Tests that an update check from a bootstrappable add-on to a bootstrappable add-on works
function run_test_29() {
  restartManager();

  writeInstallRDFForExtension({
    id: "gh@dictionaries.addons.mozilla.org",
    version: "1.0",
    type: "64",
    updateURL: "http://localhost:" + gPort + "/data/test_dictionary.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Dictionary gh",
  }, profileDir);
  restartManager();

  prepare_test({
    "gh@dictionaries.addons.mozilla.org": [
      ["onInstalling", false /* = no restart */],
      ["onInstalled", false]
    ]
  }, [
    "onNewInstall",
    "onDownloadStarted",
    "onDownloadEnded",
    "onInstallStarted",
    "onInstallEnded"
  ], check_test_29);

  AddonManagerPrivate.backgroundUpdateCheck();
}

function check_test_29(install) {
  AddonManager.getAddonByID("gh@dictionaries.addons.mozilla.org", function(b2) {
    Assert.notEqual(b2, null);
    Assert.equal(b2.version, "2.0");
    Assert.equal(b2.type, "dictionary");

    prepare_test({
      "gh@dictionaries.addons.mozilla.org": [
        ["onUninstalling", false],
        ["onUninstalled", false],
      ]
    }, [
    ], callback_soon(finish_test_29));

    b2.uninstall();
  });
}

function finish_test_29() {
  testserver.stop(do_test_finished);
}
