/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/NetUtil.jsm");

// using a dynamic port in the addon metadata
Components.utils.import("resource://testing-common/httpd.js");
let gServer = new HttpServer();
gServer.start(-1);
gPort = gServer.identity.primaryPort;

// This verifies that themes behave as expected

const PREF_GENERAL_SKINS_SELECTEDSKIN = "general.skins.selectedSkin";
const PREF_EXTENSIONS_DSS_ENABLED = "extensions.dss.enabled";

Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm");

const profileDir = gProfD.clone();
profileDir.append("extensions");

// Observer to ensure a "lightweight-theme-styling-update" notification is sent
// when expected
var gLWThemeChanged = false;
var LightweightThemeObserver = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic != "lightweight-theme-styling-update")
      return;

    gLWThemeChanged = true;
  }
};

AM_Cc["@mozilla.org/observer-service;1"]
     .getService(Components.interfaces.nsIObserverService)
     .addObserver(LightweightThemeObserver, "lightweight-theme-styling-update", false);


function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN, "theme1/1.0");
  Services.prefs.setBoolPref(PREF_EXTENSIONS_DSS_ENABLED, true);
  writeInstallRDFForExtension({
    id: "theme1@tests.mozilla.org",
    version: "1.0",
    name: "Test 1",
    type: 4,
    internalName: "theme1/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  }, profileDir);

  writeInstallRDFForExtension({
    id: "theme2@tests.mozilla.org",
    version: "1.0",
    name: "Test 1",
    internalName: "theme2/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  }, profileDir);

  // We need a default theme for some of these things to work but we have hidden
  // the one in the application directory.
  writeInstallRDFForExtension({
    id: "default@tests.mozilla.org",
    version: "1.0",
    name: "Default",
    internalName: "classic/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  }, profileDir);

  startupManager();
  // Make sure we only register once despite multiple calls
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);

  AddonManager.getAddonsByIDs(["theme1@tests.mozilla.org",
                               "theme2@tests.mozilla.org"], function([t1, t2]) {
    do_check_neq(t1, null);
    do_check_false(t1.userDisabled);
    do_check_false(t1.appDisabled);
    do_check_true(t1.isActive);
    do_check_eq(t1.screenshots, null);
    do_check_true(isThemeInAddonsList(profileDir, t1.id));
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_neq(t2, null);
    do_check_true(t2.userDisabled);
    do_check_false(t2.appDisabled);
    do_check_false(t2.isActive);
    do_check_eq(t2.screenshots, null);
    do_check_true(isThemeInAddonsList(profileDir, t2.id));
    do_check_false(hasFlag(t2.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(t2.permissions, AddonManager.PERM_CAN_ENABLE));

    do_execute_soon(run_test_1);
  });
}

function end_test() {
  do_test_finished();
}

// Checks enabling one theme disables the others
function run_test_1() {
  prepare_test({
    "theme1@tests.mozilla.org": [
      ["onDisabling", false],
      "onDisabled"
    ],
    "theme2@tests.mozilla.org": [
      ["onEnabling", false],
      "onEnabled"
    ]
  });
  AddonManager.getAddonsByIDs(["theme1@tests.mozilla.org",
                               "theme2@tests.mozilla.org"], function([t1, t2]) {
    t2.userDisabled = false;

    ensure_test_completed();
    do_check_false(hasFlag(t2.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(t2.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_true(t1.userDisabled);
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(t1.permissions, AddonManager.PERM_CAN_ENABLE));

    do_execute_soon(check_test_1);
  });
}

function check_test_1() {
  restartManager();
  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "theme2/1.0");

  AddonManager.getAddonsByIDs(["theme1@tests.mozilla.org",
                               "theme2@tests.mozilla.org"], function([t1, t2]) {
    do_check_neq(t1, null);
    do_check_true(t1.userDisabled);
    do_check_false(t1.appDisabled);
    do_check_false(t1.isActive);
    do_check_true(isThemeInAddonsList(profileDir, t1.id));
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(t1.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_neq(t2, null);
    do_check_false(t2.userDisabled);
    do_check_false(t2.appDisabled);
    do_check_true(t2.isActive);
    do_check_true(isThemeInAddonsList(profileDir, t2.id));
    do_check_false(hasFlag(t2.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(t2.permissions, AddonManager.PERM_CAN_ENABLE));
    do_check_false(gLWThemeChanged);

    do_execute_soon(run_test_2);
  });
}

// Removing the active theme should fall back to the default (not ideal in this
// case since we don't have the default theme installed)
function run_test_2() {
  var dest = profileDir.clone();
  dest.append(do_get_expected_addon_name("theme2@tests.mozilla.org"));
  dest.remove(true);

  restartManager();
  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  AddonManager.getAddonsByIDs(["theme1@tests.mozilla.org",
                               "theme2@tests.mozilla.org"], function([t1, t2]) {
    do_check_neq(t1, null);
    do_check_true(t1.userDisabled);
    do_check_false(t1.appDisabled);
    do_check_false(t1.isActive);
    do_check_true(isThemeInAddonsList(profileDir, t1.id));
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(t1.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_eq(t2, null);
    do_check_false(isThemeInAddonsList(profileDir, "theme2@tests.mozilla.org"));
    do_check_false(gLWThemeChanged);

    do_execute_soon(run_test_3);
  });
}

// Installing a lightweight theme should happen instantly and disable the default theme
function run_test_3() {
  writeInstallRDFForExtension({
    id: "theme2@tests.mozilla.org",
    version: "1.0",
    name: "Test 1",
    internalName: "theme2/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  }, profileDir);
  restartManager();

  prepare_test({
    "1@personas.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
      ["onEnabling", false],
      "onEnabled"
    ],
    "default@tests.mozilla.org": [
      ["onDisabling", false],
      "onDisabled",
    ]
  }, [
    "onExternalInstall"
  ]);

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
    iconURL: "http://localhost:" + gPort + "/data/icon.png"
  };

  ensure_test_completed();

  AddonManager.getAddonByID("1@personas.mozilla.org", function(p1) {
    do_check_neq(null, p1);
    do_check_eq(p1.name, "Test LW Theme");
    do_check_eq(p1.version, "1");
    do_check_eq(p1.type, "theme");
    do_check_eq(p1.description, "A test theme");
    do_check_eq(p1.creator, "Mozilla");
    do_check_eq(p1.homepageURL, "http://localhost:" + gPort + "/data/index.html");
    do_check_eq(p1.iconURL, "http://localhost:" + gPort + "/data/icon.png");
    do_check_eq(p1.screenshots.length, 1);
    do_check_eq(p1.screenshots[0], "http://localhost:" + gPort + "/data/preview.png");
    do_check_false(p1.appDisabled);
    do_check_false(p1.userDisabled);
    do_check_true(p1.isCompatible);
    do_check_true(p1.providesUpdatesSecurely);
    do_check_eq(p1.blocklistState, 0);
    do_check_true(p1.isActive);
    do_check_eq(p1.pendingOperations, 0);
    do_check_eq(p1.permissions, AddonManager.PERM_CAN_UNINSTALL | AddonManager.PERM_CAN_DISABLE);
    do_check_eq(p1.scope, AddonManager.SCOPE_PROFILE);
    do_check_true("isCompatibleWith" in p1);
    do_check_true("findUpdates" in p1);

    AddonManager.getAddonsByTypes(["theme"], function(addons) {
      let seen = false;
      addons.forEach(function(a) {
        if (a.id == "1@personas.mozilla.org") {
          seen = true;
        }
        else {
          dump("Checking theme " + a.id + "\n");
          do_check_false(a.isActive);
          do_check_true(a.userDisabled);
        }
      });
      do_check_true(seen);

      do_check_true(gLWThemeChanged);
      gLWThemeChanged = false;

      do_execute_soon(run_test_4);
    });
  });
}

// Installing a second lightweight theme should disable the first with no restart
function run_test_4() {
  prepare_test({
    "1@personas.mozilla.org": [
      ["onDisabling", false],
      "onDisabled",
    ],
    "2@personas.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
      ["onEnabling", false],
      "onEnabled"
    ]
  }, [
    "onExternalInstall"
  ]);

  LightweightThemeManager.currentTheme = {
    id: "2",
    version: "1",
    name: "Test LW Theme",
    description: "A second test theme",
    author: "Mozilla",
    homepageURL: "http://localhost:" + gPort + "/data/index.html",
    headerURL: "http://localhost:" + gPort + "/data/header.png",
    footerURL: "http://localhost:" + gPort + "/data/footer.png",
    previewURL: "http://localhost:" + gPort + "/data/preview.png",
    iconURL: "http://localhost:" + gPort + "/data/icon.png"
  };

  ensure_test_completed();

  AddonManager.getAddonsByIDs(["1@personas.mozilla.org",
                               "2@personas.mozilla.org"], function([p1, p2]) {
    do_check_neq(null, p2);
    do_check_false(p2.appDisabled);
    do_check_false(p2.userDisabled);
    do_check_true(p2.isActive);
    do_check_eq(p2.pendingOperations, 0);
    do_check_eq(p2.permissions, AddonManager.PERM_CAN_UNINSTALL | AddonManager.PERM_CAN_DISABLE);

    do_check_neq(null, p1);
    do_check_false(p1.appDisabled);
    do_check_true(p1.userDisabled);
    do_check_false(p1.isActive);
    do_check_eq(p1.pendingOperations, 0);
    do_check_eq(p1.permissions, AddonManager.PERM_CAN_UNINSTALL | AddonManager.PERM_CAN_ENABLE);

    AddonManager.getAddonsByTypes(["theme"], function(addons) {
      let seen = false;
      addons.forEach(function(a) {
        if (a.id == "2@personas.mozilla.org") {
          seen = true;
        }
        else {
          dump("Checking theme " + a.id + "\n");
          do_check_false(a.isActive);
          do_check_true(a.userDisabled);
        }
      });
      do_check_true(seen);

      do_check_true(gLWThemeChanged);
      gLWThemeChanged = false;

      do_execute_soon(run_test_5);
    });
  });
}

// Switching to a custom theme should disable the lightweight theme and require
// a restart. Cancelling that should also be possible.
function run_test_5() {
  prepare_test({
    "2@personas.mozilla.org": [
      ["onDisabling", false],
      "onDisabled"
    ],
    "theme2@tests.mozilla.org": [
      ["onEnabling", false],
      "onEnabled"
    ]
  });

  AddonManager.getAddonsByIDs(["2@personas.mozilla.org",
                               "theme2@tests.mozilla.org"], function([p2, t2]) {
    t2.userDisabled = false;

    ensure_test_completed();

    prepare_test({
      "2@personas.mozilla.org": [
        "onEnabling"
      ],
      "theme2@tests.mozilla.org": [
        ["onDisabling", false],
        "onDisabled"
      ]
    });

    p2.userDisabled = false;

    ensure_test_completed();

    prepare_test({
      "2@personas.mozilla.org": [
        ["onOperationCancelled", true]
      ],
      "theme2@tests.mozilla.org": [
        ["onEnabling", false],
        "onEnabled"
      ]
    });

    t2.userDisabled = false;

    ensure_test_completed();

    do_check_true(t2.isActive);
    do_check_false(t2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_ENABLE, t2.pendingOperations));
    do_check_false(p2.isActive);
    do_check_true(p2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_DISABLE, p2.pendingOperations));
    do_check_true(hasFlag(AddonManager.PERM_CAN_ENABLE, p2.permissions));
    do_check_true(gLWThemeChanged);

    do_execute_soon(check_test_5);
  });
}

function check_test_5() {
  restartManager();

  AddonManager.getAddonsByIDs(["2@personas.mozilla.org",
                               "theme2@tests.mozilla.org"], function([p2, t2]) {
    do_check_true(t2.isActive);
    do_check_false(t2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_ENABLE, t2.pendingOperations));
    do_check_false(p2.isActive);
    do_check_true(p2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_DISABLE, p2.pendingOperations));

    do_check_true(gLWThemeChanged);
    gLWThemeChanged = false;

    do_execute_soon(run_test_6);
  });
}

// Switching from a custom theme to a lightweight theme should require a restart
function run_test_6() {
  prepare_test({
    "2@personas.mozilla.org": [
      "onEnabling",
    ],
    "theme2@tests.mozilla.org": [
      ["onDisabling", false],
      "onDisabled"
    ]
  });

  AddonManager.getAddonsByIDs(["2@personas.mozilla.org",
                               "theme2@tests.mozilla.org"], function([p2, t2]) {
    p2.userDisabled = false;

    ensure_test_completed();

    prepare_test({
      "2@personas.mozilla.org": [
        "onOperationCancelled",
      ],
      "theme2@tests.mozilla.org": [
        ["onEnabling", false],
        "onEnabled"
      ]
    });

    t2.userDisabled = false;

    ensure_test_completed();

    prepare_test({
      "2@personas.mozilla.org": [
        "onEnabling",
      ],
      "theme2@tests.mozilla.org": [
        ["onDisabling", false],
        "onDisabled"
      ]
    });

    p2.userDisabled = false;

    ensure_test_completed();

    do_check_false(p2.isActive);
    do_check_false(p2.userDisabled);
    do_check_true(hasFlag(AddonManager.PENDING_ENABLE, p2.pendingOperations));
    do_check_false(t2.isActive);
    do_check_true(t2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_DISABLE, t2.pendingOperations));
    do_check_false(gLWThemeChanged);

    do_execute_soon(check_test_6);
  });
}

function check_test_6() {
  restartManager();

  AddonManager.getAddonsByIDs(["2@personas.mozilla.org",
                               "theme2@tests.mozilla.org"], function([p2, t2]) {
    do_check_true(p2.isActive);
    do_check_false(p2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_ENABLE, p2.pendingOperations));
    do_check_false(t2.isActive);
    do_check_true(t2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_DISABLE, t2.pendingOperations));

    do_check_true(gLWThemeChanged);
    gLWThemeChanged = false;

    do_execute_soon(run_test_7);
  });
}

// Uninstalling a lightweight theme should not require a restart
function run_test_7() {
  prepare_test({
    "1@personas.mozilla.org": [
      ["onUninstalling", false],
      "onUninstalled"
    ]
  });

  AddonManager.getAddonByID("1@personas.mozilla.org", function(p1) {
    p1.uninstall();

    ensure_test_completed();
    do_check_eq(LightweightThemeManager.usedThemes.length, 1);
    do_check_false(gLWThemeChanged);

    do_execute_soon(run_test_8);
  });
}

// Uninstalling a lightweight theme in use should not require a restart and it
// should reactivate the default theme
// Also, uninstalling a lightweight theme in use should send a
// "lightweight-theme-styling-update" notification through the observer service
function run_test_8() {
  prepare_test({
    "2@personas.mozilla.org": [
      ["onUninstalling", false],
      "onUninstalled"
    ],
    "default@tests.mozilla.org": [
      ["onEnabling", false],
      "onEnabled"
    ]
  });

  AddonManager.getAddonByID("2@personas.mozilla.org", function(p2) {
    p2.uninstall();

    ensure_test_completed();
    do_check_eq(LightweightThemeManager.usedThemes.length, 0);

    do_check_true(gLWThemeChanged);
    gLWThemeChanged = false;

    do_execute_soon(run_test_9);
  });
}

// Uninstalling a theme not in use should not require a restart
function run_test_9() {
  AddonManager.getAddonByID("theme1@tests.mozilla.org", function(t1) {
    prepare_test({
      "theme1@tests.mozilla.org": [
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });

    t1.uninstall();

    ensure_test_completed();

    AddonManager.getAddonByID("theme1@tests.mozilla.org", function(newt1) {
      do_check_eq(newt1, null);
      do_check_false(gLWThemeChanged);

      do_execute_soon(run_test_10);
    });
  });
}

// Uninstalling a custom theme in use should require a restart
function run_test_10() {
  AddonManager.getAddonByID("theme2@tests.mozilla.org", function(oldt2) {
    prepare_test({
      "theme2@tests.mozilla.org": [
        ["onEnabling", false],
        "onEnabled"
      ],
      "default@tests.mozilla.org": [
        ["onDisabling", false],
        "onDisabled"
      ]
    });

    oldt2.userDisabled = false;

    ensure_test_completed();

    restartManager();

    AddonManager.getAddonsByIDs(["default@tests.mozilla.org",
                                 "theme2@tests.mozilla.org"], function([d, t2]) {
      do_check_true(t2.isActive);
      do_check_false(t2.userDisabled);
      do_check_false(t2.appDisabled);
      do_check_false(d.isActive);
      do_check_true(d.userDisabled);
      do_check_false(d.appDisabled);

      prepare_test({
        "theme2@tests.mozilla.org": [
          ["onUninstalling", false],
          "onUninstalled"
        ],
        "default@tests.mozilla.org": [
          ["onEnabling", false],
          "onEnabled"
        ]
      });

      t2.uninstall();

      ensure_test_completed();
      do_check_false(gLWThemeChanged);

      restartManager();

      do_execute_soon(run_test_11);
    });
  });
}

// Installing a custom theme not in use should not require a restart
function run_test_11() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_theme"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.type, "theme");
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Test Theme 1");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);

    prepare_test({
      "theme1@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], check_test_11);
    install.install();
  });
}

function check_test_11() {
  restartManager();
  AddonManager.getAddonByID("theme1@tests.mozilla.org", function(t1) {
    do_check_neq(t1, null);
    var previewSpec = do_get_addon_root_uri(profileDir, "theme1@tests.mozilla.org") + "preview.png";
    do_check_eq(t1.screenshots.length, 1);
    do_check_eq(t1.screenshots[0], previewSpec);
    do_check_false(gLWThemeChanged);

    do_execute_soon(run_test_12);
  });
}

// Updating a custom theme not in use should not require a restart
function run_test_12() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_theme"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.type, "theme");
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Test Theme 1");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);

    prepare_test({
      "theme1@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], check_test_12);
    install.install();
  });
}

function check_test_12() {
  AddonManager.getAddonByID("theme1@tests.mozilla.org", function(t1) {
    do_check_neq(t1, null);
    do_check_false(gLWThemeChanged);

    do_execute_soon(run_test_13);
  });
}

// Updating a custom theme in use should require a restart
function run_test_13() {
  AddonManager.getAddonByID("theme1@tests.mozilla.org", function(t1) {
    prepare_test({
      "theme1@tests.mozilla.org": [
        ["onEnabling", false],
        "onEnabled"
      ],
      "default@tests.mozilla.org": [
        ["onDisabling", false],
        "onDisabled"
      ]
    });

    t1.userDisabled = false;
    ensure_test_completed();
    restartManager();

    prepare_test({ }, [
      "onNewInstall"
    ]);

    AddonManager.getInstallForFile(do_get_addon("test_theme"), function(install) {
      ensure_test_completed();

      do_check_neq(install, null);
      do_check_eq(install.type, "theme");
      do_check_eq(install.version, "1.0");
      do_check_eq(install.name, "Test Theme 1");
      do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);

      prepare_test({
        "theme1@tests.mozilla.org": [
          "onInstalling",
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], check_test_13);
      install.install();
    });
  });
}

function check_test_13() {
  restartManager();

  AddonManager.getAddonByID("theme1@tests.mozilla.org", function(t1) {
    do_check_neq(t1, null);
    do_check_true(t1.isActive);
    do_check_false(gLWThemeChanged);
    t1.uninstall();
    restartManager();

    do_execute_soon(run_test_14);
  });
}

// Switching from a lightweight theme to the default theme should not require
// a restart
function run_test_14() {
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
    iconURL: "http://localhost:" + gPort + "/data/icon.png"
  };

  AddonManager.getAddonByID("default@tests.mozilla.org", function(d) {
    do_check_true(d.userDisabled);
    do_check_false(d.isActive);

    prepare_test({
      "1@personas.mozilla.org": [
        ["onDisabling", false],
        "onDisabled"
      ],
      "default@tests.mozilla.org": [
        ["onEnabling", false],
        "onEnabled"
      ]
    });

    d.userDisabled = false;
    ensure_test_completed();

    do_check_false(d.userDisabled);
    do_check_true(d.isActive);

    do_check_true(gLWThemeChanged);
    gLWThemeChanged = false;

    end_test();
  });
}
