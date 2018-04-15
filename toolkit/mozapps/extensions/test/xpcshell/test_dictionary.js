/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that bootstrappable add-ons can be used without restarts.
ChromeUtils.import("resource://gre/modules/Services.jsm");

// Enable loading extensions from the user scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_USER);

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const ID_DICT = "ab-CD@dictionaries.addons.mozilla.org";
const XPI_DICT = do_get_addon("test_dictionary");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");

const userExtDir = gProfD.clone();
userExtDir.append("extensions2");
userExtDir.append(gAppInfo.ID);

registerDirectory("XREUSysExt", userExtDir.parent);

// Create and configure the HTTP server.
var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});

// register files with server
testserver.registerDirectory("/addons/", do_get_file("addons"));
testserver.registerDirectory("/data/", do_get_file("data"));

/**
 * This object is both a factory and an mozISpellCheckingEngine implementation (so, it
 * is de-facto a service). It's also an interface requestor that gives out
 * itself when asked for mozISpellCheckingEngine.
 */
var HunspellEngine = {
  dictionaryDirs: [],
  listener: null,

  QueryInterface: function hunspell_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.mozISpellCheckingEngine))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  createInstance: function hunspell_ci(outer, iid) {
    if (outer)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
  lockFactory: function hunspell_lockf(lock) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
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
    if (iid.equals(Ci.mozISpellCheckingEngine))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  contractID: "@mozilla.org/spellchecker/engine;1",
  classID: Components.ID("{6f3c63bc-a4fd-449b-9a58-a2d9bd972cce}"),

  activate: function hunspell_activate() {
    this.origClassID = Components.manager.nsIComponentRegistrar
      .contractIDToCID(this.contractID);
    this.origFactory = Components.manager
      .getClassObject(Cc[this.contractID],
                      Ci.nsIFactory);

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

add_task(async function setup() {
  ok(AddonTestUtils.testUnpacked,
     "Dictionaries are only supported when installed unpacked.");

  await promiseStartupManager();
});

// Tests that installing doesn't require a restart
add_task(async function test_1() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  HunspellEngine.activate();

  let install = await AddonManager.getInstallForFile(
    do_get_addon("test_dictionary"));
  ensure_test_completed();

  notEqual(install, null);
  equal(install.type, "dictionary");
  equal(install.version, "1.0");
  equal(install.name, "Test Dictionary");
  equal(install.state, AddonManager.STATE_DOWNLOADED);
  ok(install.addon.hasResource("install.rdf"));
  ok(!install.addon.hasResource("bootstrap.js"));
  equal(install.addon.operationsRequiringRestart &
               AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
  do_check_not_in_crash_annotation(ID_DICT, "1.0");

  await new Promise(resolve => {
    let addon = install.addon;
    prepare_test({
      [ID_DICT]: [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], function() {
      ok(addon.hasResource("install.rdf"));
      HunspellEngine.listener = function(aEvent) {
        HunspellEngine.listener = null;
        equal(aEvent, "addDirectory");
        resolve();
      };
    });
    install.install();
  });

  let installs = await AddonManager.getAllInstalls();
  // There should be no active installs now since the install completed and
  // doesn't require a restart.
  equal(installs.length, 0);

  let addon = await AddonManager.getAddonByID(ID_DICT);
  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(!addon.appDisabled);
  ok(!addon.userDisabled);
  ok(addon.isActive);
  ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  ok(addon.hasResource("install.rdf"));
  ok(!addon.hasResource("bootstrap.js"));
  do_check_in_crash_annotation(ID_DICT, "1.0");

  let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                  getService(Ci.nsIChromeRegistry);
  try {
    chromeReg.convertChromeURL(NetUtil.newURI("chrome://dict/content/dict.xul"));
    do_throw("Chrome manifest should not have been registered");
  } catch (e) {
    // Expected the chrome url to not be registered
  }

  let list = await AddonManager.getAddonsWithOperationsByTypes(null);
  equal(list.length, 0);
});

// Tests that disabling doesn't require a restart
add_task(async function test_2() {
  let addon = await AddonManager.getAddonByID(ID_DICT);
  prepare_test({
    [ID_DICT]: [
      ["onDisabling", false],
      "onDisabled"
    ]
  });

  equal(addon.operationsRequiringRestart &
               AddonManager.OP_NEEDS_RESTART_DISABLE, 0);
  addon.userDisabled = true;
  ensure_test_completed();

  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(!addon.appDisabled);
  ok(addon.userDisabled);
  ok(!addon.isActive);
  ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_not_in_crash_annotation(ID_DICT, "1.0");

  addon = await AddonManager.getAddonByID(ID_DICT);
  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(!addon.appDisabled);
  ok(addon.userDisabled);
  ok(!addon.isActive);
});

// Test that restarting doesn't accidentally re-enable
add_task(async function test_3() {
  await promiseShutdownManager();
  ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  await promiseStartupManager(false);

  ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_not_in_crash_annotation(ID_DICT, "1.0");

  let addon = await AddonManager.getAddonByID(ID_DICT);
  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(!addon.appDisabled);
  ok(addon.userDisabled);
  ok(!addon.isActive);
});

// Tests that enabling doesn't require a restart
add_task(async function test_4() {
  let addon = await AddonManager.getAddonByID(ID_DICT);
  prepare_test({
    [ID_DICT]: [
      ["onEnabling", false],
      "onEnabled"
    ]
  });

  equal(addon.operationsRequiringRestart &
               AddonManager.OP_NEEDS_RESTART_ENABLE, 0);
  addon.userDisabled = false;
  ensure_test_completed();

  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(!addon.appDisabled);
  ok(!addon.userDisabled);
  ok(addon.isActive);
  ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_in_crash_annotation(ID_DICT, "1.0");

  addon = await AddonManager.getAddonByID(ID_DICT);
  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(!addon.appDisabled);
  ok(!addon.userDisabled);
  ok(addon.isActive);
});

// Tests that a restart shuts down and restarts the add-on
add_task(async function test_5() {
  await promiseShutdownManager();

  ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_not_in_crash_annotation(ID_DICT, "1.0");

  await promiseStartupManager(false);

  ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_in_crash_annotation(ID_DICT, "1.0");

  let addon = await AddonManager.getAddonByID(ID_DICT);
  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(!addon.appDisabled);
  ok(!addon.userDisabled);
  ok(addon.isActive);
  ok(!isExtensionInAddonsList(profileDir, addon.id));
});

// Tests that uninstalling doesn't require a restart
add_task(async function test_7() {
  let addon = await AddonManager.getAddonByID(ID_DICT);
  prepare_test({
    [ID_DICT]: [
      ["onUninstalling", false],
      "onUninstalled"
    ]
  });

  equal(addon.operationsRequiringRestart &
               AddonManager.OP_NEEDS_RESTART_UNINSTALL, 0);
  addon.uninstall();

  ensure_test_completed();

  ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_not_in_crash_annotation(ID_DICT, "1.0");

  addon = await AddonManager.getAddonByID(ID_DICT);
  equal(addon, null);

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID(ID_DICT);
  equal(addon, null);
});

// Test that a bootstrapped extension dropped into the profile loads properly
// on startup and doesn't cause an EM restart
add_task(async function test_8() {
  await promiseShutdownManager();
  await AddonTestUtils.manuallyInstall(XPI_DICT);
  await promiseStartupManager(false);

  let addon = await AddonManager.getAddonByID(ID_DICT);
  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(!addon.appDisabled);
  ok(!addon.userDisabled);
  ok(addon.isActive);
  ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_in_crash_annotation(ID_DICT, "1.0");
});

// Test that items detected as removed during startup get removed properly
add_task(async function test_9() {
  await promiseShutdownManager();
  await AddonTestUtils.manuallyUninstall(profileDir, ID_DICT);
  await promiseStartupManager(false);

  let addon = await AddonManager.getAddonByID(ID_DICT);
  equal(addon, null);
  do_check_not_in_crash_annotation(ID_DICT, "1.0");
});


// Tests that bootstrapped extensions are correctly loaded even if the app is
// upgraded at the same time
add_task(async function test_12() {
  await promiseShutdownManager();
  await AddonTestUtils.manuallyInstall(XPI_DICT);
  await promiseStartupManager(true);

  let addon = await AddonManager.getAddonByID(ID_DICT);
  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(!addon.appDisabled);
  ok(!addon.userDisabled);
  ok(addon.isActive);
  ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_in_crash_annotation(ID_DICT, "1.0");

  addon.uninstall();
});


// Tests that bootstrapped extensions don't get loaded when in safe mode
add_task(async function test_16() {
  await promiseRestartManager();
  await promiseInstallFile(do_get_addon("test_dictionary"));

  let addon = await AddonManager.getAddonByID(ID_DICT);
  // Should have installed and started
  ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

  await promiseShutdownManager();

  // Should have stopped
  ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

  gAppInfo.inSafeMode = true;
  await promiseStartupManager(false);

  addon = await AddonManager.getAddonByID(ID_DICT);
  // Should still be stopped
  ok(!HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  ok(!addon.isActive);

  await promiseShutdownManager();
  gAppInfo.inSafeMode = false;
  await promiseStartupManager(false);

  // Should have started
  ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

  addon = await AddonManager.getAddonByID(ID_DICT);
  addon.uninstall();
});

// Check that a bootstrapped extension in a non-profile location is loaded
add_task(async function test_17() {
  await promiseShutdownManager();
  await AddonTestUtils.manuallyInstall(XPI_DICT, userExtDir);
  await promiseStartupManager();

  let addon = await AddonManager.getAddonByID(ID_DICT);
  // Should have installed and started
  ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(addon.isActive);

  await AddonTestUtils.manuallyUninstall(userExtDir, ID_DICT);
  await promiseRestartManager();
});

// Tests that installing from a URL doesn't require a restart
add_task(async function test_23() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://example.com/addons/test_dictionary.xpi";
  let install = await AddonManager.getInstallForURL(url, "application/x-xpinstall");
  ensure_test_completed();

  notEqual(install, null);

  await new Promise(resolve => {
    prepare_test({ }, [
      "onDownloadStarted",
      "onDownloadEnded"
    ], function() {
      equal(install.type, "dictionary");
      equal(install.version, "1.0");
      equal(install.name, "Test Dictionary");
      equal(install.state, AddonManager.STATE_DOWNLOADED);
      ok(install.addon.hasResource("install.rdf"));
      ok(!install.addon.hasResource("bootstrap.js"));
      equal(install.addon.operationsRequiringRestart &
                   AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
      do_check_not_in_crash_annotation(ID_DICT, "1.0");

      let addon = install.addon;
      prepare_test({
        [ID_DICT]: [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], function() {
        ok(addon.hasResource("install.rdf"));
        // spin to let the addon startup finish
        resolve();
      });
    });
    install.install();
  });

  let installs = await AddonManager.getAllInstalls();
  // There should be no active installs now since the install completed and
  // doesn't require a restart.
  equal(installs.length, 0);

  let addon = await AddonManager.getAddonByID(ID_DICT);
  notEqual(addon, null);
  equal(addon.version, "1.0");
  ok(!addon.appDisabled);
  ok(!addon.userDisabled);
  ok(addon.isActive);
  ok(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  ok(addon.hasResource("install.rdf"));
  ok(!addon.hasResource("bootstrap.js"));
  do_check_in_crash_annotation(ID_DICT, "1.0");

  let list = await AddonManager.getAddonsWithOperationsByTypes(null);
  equal(list.length, 0);

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID(ID_DICT);
  addon.uninstall();
});

// Tests that an update check from a bootstrappable add-on to a bootstrappable add-on works
add_task(async function test_29() {
  await promiseRestartManager();

  await promiseWriteInstallRDFForExtension({
    id: "gh@dictionaries.addons.mozilla.org",
    version: "1.0",
    type: "64",
    updateURL: "http://example.com/data/test_dictionary.json",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Dictionary gh",
  }, profileDir);

  await promiseRestartManager();

  await new Promise(resolve => {
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
    ], resolve);

    AddonManagerPrivate.backgroundUpdateCheck();
  });

  let addon = await AddonManager.getAddonByID("gh@dictionaries.addons.mozilla.org");
  notEqual(addon, null);
  equal(addon.version, "2.0");
  equal(addon.type, "dictionary");

  await new Promise(resolve => {
    prepare_test({
      "gh@dictionaries.addons.mozilla.org": [
        ["onUninstalling", false],
        ["onUninstalled", false],
      ]
    }, [
    ], resolve);

    addon.uninstall();
  });
});
