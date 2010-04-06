/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that themes behave as expected

// TODO This test relies on the default theme being present

const PREF_GENERAL_SKINS_SELECTEDSKIN = "general.skins.selectedSkin";

Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm");

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN, "theme1/1.0");
  var dest = profileDir.clone();
  dest.append("theme1@tests.mozilla.org");
  writeInstallRDFToDir({
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
  }, dest);
  dest = profileDir.clone();
  dest.append("theme2@tests.mozilla.org");
  writeInstallRDFToDir({
    id: "theme2@tests.mozilla.org",
    version: "1.0",
    name: "Test 1",
    internalName: "theme2/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  }, dest);

  startupManager(1);
  // Make sure we only register once despite multiple calls
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);
  AddonManager.addAddonListener(AddonListener);

  AddonManager.getAddons(["theme1@tests.mozilla.org", "theme2@tests.mozilla.org"], function([t1, t2]) {
    do_check_neq(t1, null);
    do_check_false(t1.userDisabled);
    do_check_false(t1.appDisabled);
    do_check_true(t1.isActive);
    do_check_true(isThemeInAddonsList(profileDir, t1.id));
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_neq(t2, null);
    do_check_true(t2.userDisabled);
    do_check_false(t2.appDisabled);
    do_check_false(t2.isActive);
    do_check_false(isThemeInAddonsList(profileDir, t2.id));
    do_check_false(hasFlag(t2.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(t2.permissions, AddonManager.PERM_CAN_ENABLE));

    run_test_1();
  });
}

function end_test() {
  do_test_finished();
}

// Checks enabling one theme disables the others
function run_test_1() {
  prepare_test({
    "theme1@tests.mozilla.org": [
      "onDisabling"
    ],
    "theme2@tests.mozilla.org": [
      "onEnabling"
    ]
  });
  AddonManager.getAddons(["theme1@tests.mozilla.org", "theme2@tests.mozilla.org"], function([t1, t2]) {
    t2.userDisabled = false;

    ensure_test_completed();
    do_check_false(hasFlag(t2.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(t2.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_true(t1.userDisabled);
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(t1.permissions, AddonManager.PERM_CAN_ENABLE));

    check_test_1();
  });
}

function check_test_1() {
  restartManager(0);
  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "theme2/1.0");

  AddonManager.getAddons(["theme1@tests.mozilla.org", "theme2@tests.mozilla.org"], function([t1, t2]) {
    do_check_neq(t1, null);
    do_check_true(t1.userDisabled);
    do_check_false(t1.appDisabled);
    do_check_false(t1.isActive);
    do_check_false(isThemeInAddonsList(profileDir, t1.id));
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(t1.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_neq(t2, null);
    do_check_false(t2.userDisabled);
    do_check_false(t2.appDisabled);
    do_check_true(t2.isActive);
    do_check_true(isThemeInAddonsList(profileDir, t2.id));
    do_check_false(hasFlag(t2.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(t2.permissions, AddonManager.PERM_CAN_ENABLE));

    run_test_2();
  });
}

// Removing the active theme should fall back to the default (not ideal in this
// case since we don't have the default theme installed)
function run_test_2() {
  var dest = profileDir.clone();
  dest.append("theme2@tests.mozilla.org");
  dest.remove(true);

  restartManager(1);
  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  AddonManager.getAddons(["theme1@tests.mozilla.org", "theme2@tests.mozilla.org"], function([t1, t2]) {
    do_check_neq(t1, null);
    do_check_true(t1.userDisabled);
    do_check_false(t1.appDisabled);
    do_check_false(t1.isActive);
    do_check_false(isThemeInAddonsList(profileDir, t1.id));
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(t1.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_eq(t2, null);
    do_check_false(isThemeInAddonsList(profileDir, "theme2@tests.mozilla.org"));

    run_test_3();
  });
}

// Installing a lightweight theme should happen instantly and disable the default theme
function run_test_3() {
  var dest = profileDir.clone();
  dest.append("theme2@tests.mozilla.org");
  writeInstallRDFToDir({
    id: "theme2@tests.mozilla.org",
    version: "1.0",
    name: "Test 1",
    internalName: "theme2/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  }, dest);
  restartManager(1);

  prepare_test({
    "1@personas.mozilla.org": [
      ["onInstalling", false],
      "onInstalled",
      ["onEnabling", false],
      "onEnabled"
    ],
    "{972ce4c6-7e08-4474-a285-3208198ce6fd}": [
      ["onDisabling", false],
      "onDisabled",
    ]
  });

  LightweightThemeManager.currentTheme = {
    id: "1",
    version: "1",
    name: "Test LW Theme",
    description: "A test theme",
    author: "Mozilla",
    homepageURL: "http://localhost:4444/data/index.html",
    headerURL: "http://localhost:4444/data/header.png",
    footerURL: "http://localhost:4444/data/footer.png",
    previewURL: "http://localhost:4444/data/preview.png",
    iconURL: "http://localhost:4444/data/icon.png"
  };

  ensure_test_completed();

  AddonManager.getAddon("1@personas.mozilla.org", function(p1) {
    do_check_neq(null, p1);
    do_check_eq(p1.name, "Test LW Theme");
    do_check_eq(p1.version, "1");
    do_check_eq(p1.description, "A test theme");
    do_check_eq(p1.creator, "Mozilla");
    do_check_eq(p1.homepageURL, "http://localhost:4444/data/index.html");
    do_check_eq(p1.iconURL, "http://localhost:4444/data/icon.png");
    do_check_eq(p1.screenshots.length, 1);
    do_check_eq(p1.screenshots[0], "http://localhost:4444/data/preview.png");
    do_check_false(p1.appDisabled);
    do_check_false(p1.userDisabled);
    do_check_true(p1.isCompatible);
    do_check_true(p1.providesUpdatesSecurely);
    do_check_eq(p1.blocklistState, 0);
    do_check_true(p1.isActive);
    do_check_eq(p1.pendingOperations, 0);
    do_check_eq(p1.permissions, AddonManager.PERM_CAN_UNINSTALL);

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

      run_test_4();
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
  });

  LightweightThemeManager.currentTheme = {
    id: "2",
    version: "1",
    name: "Test LW Theme",
    description: "A second test theme",
    author: "Mozilla",
    homepageURL: "http://localhost:4444/data/index.html",
    headerURL: "http://localhost:4444/data/header.png",
    footerURL: "http://localhost:4444/data/footer.png",
    previewURL: "http://localhost:4444/data/preview.png",
    iconURL: "http://localhost:4444/data/icon.png"
  };

  ensure_test_completed();

  AddonManager.getAddons(["1@personas.mozilla.org",
                          "2@personas.mozilla.org"], function([p1, p2]) {
    do_check_neq(null, p2);
    do_check_false(p2.appDisabled);
    do_check_false(p2.userDisabled);
    do_check_true(p2.isActive);
    do_check_eq(p2.pendingOperations, 0);
    do_check_eq(p2.permissions, AddonManager.PERM_CAN_UNINSTALL);

    do_check_neq(null, p1);
    do_check_false(p1.appDisabled);
    do_check_true(p1.userDisabled);
    do_check_false(p1.isActive);
    do_check_eq(p1.pendingOperations, 0);
    do_check_eq(p1.permissions, AddonManager.PERM_CAN_UNINSTALL + AddonManager.PERM_CAN_ENABLE);

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

      run_test_5();
    });
  });
}

// Switching to a custom theme should disable the lightweight theme and require
// a restart. Cancelling that should also be possible.
function run_test_5() {
  prepare_test({
    "2@personas.mozilla.org": [
      "onDisabling",
    ],
    "theme2@tests.mozilla.org": [
      "onEnabling"
    ]
  });

  AddonManager.getAddons(["2@personas.mozilla.org",
                          "theme2@tests.mozilla.org"], function([p2, t2]) {
    t2.userDisabled = false;

    ensure_test_completed();

    prepare_test({
      "2@personas.mozilla.org": [
        "onOperationCancelled",
      ],
      "theme2@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    });

    p2.userDisabled = false;

    ensure_test_completed();

    prepare_test({
      "2@personas.mozilla.org": [
        "onDisabling",
      ],
      "theme2@tests.mozilla.org": [
        "onEnabling"
      ]
    });

    t2.userDisabled = false;

    ensure_test_completed();

    do_check_false(t2.isActive);
    do_check_false(t2.userDisabled);
    do_check_true(hasFlag(AddonManager.PENDING_ENABLE, t2.pendingOperations));
    do_check_true(p2.isActive);
    do_check_true(p2.userDisabled);
    do_check_true(hasFlag(AddonManager.PENDING_DISABLE, p2.pendingOperations));
    do_check_true(hasFlag(AddonManager.PERM_CAN_ENABLE, p2.permissions));

    check_test_5();
  });
}

function check_test_5() {
  restartManager(0);

  AddonManager.getAddons(["2@personas.mozilla.org",
                          "theme2@tests.mozilla.org"], function([p2, t2]) {
    do_check_true(t2.isActive);
    do_check_false(t2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_ENABLE, t2.pendingOperations));
    do_check_false(p2.isActive);
    do_check_true(p2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_DISABLE, p2.pendingOperations));

    run_test_6();
  });
}

// Switching from a custom theme to a lightweight theme should require a restart
function run_test_6() {
  prepare_test({
    "2@personas.mozilla.org": [
      "onEnabling",
    ],
    "theme2@tests.mozilla.org": [
      "onDisabling"
    ]
  });

  AddonManager.getAddons(["2@personas.mozilla.org",
                          "theme2@tests.mozilla.org"], function([p2, t2]) {
    p2.userDisabled = false;

    ensure_test_completed();

    prepare_test({
      "2@personas.mozilla.org": [
        "onOperationCancelled",
      ],
      "theme2@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    });

    t2.userDisabled = false;

    ensure_test_completed();

    prepare_test({
      "2@personas.mozilla.org": [
        "onEnabling",
      ],
      "theme2@tests.mozilla.org": [
        "onDisabling"
      ]
    });

    p2.userDisabled = false;

    ensure_test_completed();

    do_check_false(p2.isActive);
    do_check_false(p2.userDisabled);
    do_check_true(hasFlag(AddonManager.PENDING_ENABLE, p2.pendingOperations));
    do_check_true(t2.isActive);
    do_check_true(t2.userDisabled);
    do_check_true(hasFlag(AddonManager.PENDING_DISABLE, t2.pendingOperations));

    check_test_6();
  });
}

function check_test_6() {
  restartManager(0);

  AddonManager.getAddons(["2@personas.mozilla.org",
                          "theme2@tests.mozilla.org"], function([p2, t2]) {
    do_check_true(p2.isActive);
    do_check_false(p2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_ENABLE, p2.pendingOperations));
    do_check_false(t2.isActive);
    do_check_true(t2.userDisabled);
    do_check_false(hasFlag(AddonManager.PENDING_DISABLE, t2.pendingOperations));

    run_test_7();
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

  AddonManager.getAddon("1@personas.mozilla.org", function(p1) {
    p1.uninstall();

    ensure_test_completed();
    do_check_eq(LightweightThemeManager.usedThemes.length, 1);

    run_test_8();
  });
}

// Uninstalling a lightweight theme in use should not require a restart and it
// should reactivate the default theme
function run_test_8() {
  // TODO stop this depending on the default theme being present
  prepare_test({
    "2@personas.mozilla.org": [
      ["onUninstalling", false],
      "onUninstalled"
    ],
    "{972ce4c6-7e08-4474-a285-3208198ce6fd}": [
      ["onEnabling", false],
      "onEnabled"
    ]
  });

  AddonManager.getAddon("2@personas.mozilla.org", function(p2) {
    p2.uninstall();

    ensure_test_completed();
    do_check_eq(LightweightThemeManager.usedThemes.length, 0);

    run_test_9();
  });
}

// Uninstalling a theme not in use should not require a restart
function run_test_9() {
  AddonManager.getAddon("theme1@tests.mozilla.org", function(t1) {
    prepare_test({
      "theme1@tests.mozilla.org": [
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });

    t1.uninstall();

    ensure_test_completed();

    AddonManager.getAddon("theme1@tests.mozilla.org", function(newt1) {
      do_check_eq(newt1, null);

      run_test_10();
    });
  });
}

// Uninstalling a custom theme in use should require a restart
function run_test_10() {
  AddonManager.getAddon("theme2@tests.mozilla.org", function(oldt2) {
    prepare_test({
      "theme2@tests.mozilla.org": [
        "onEnabling",
      ],
      "{972ce4c6-7e08-4474-a285-3208198ce6fd}": [
        "onDisabling"
      ]
    });

    oldt2.userDisabled = false;

    ensure_test_completed();

    restartManager(0);

    AddonManager.getAddons(["{972ce4c6-7e08-4474-a285-3208198ce6fd}",
                            "theme2@tests.mozilla.org"], function([d, t2]) {
      do_check_true(t2.isActive);
      do_check_false(t2.userDisabled);
      do_check_false(t2.appDisabled);
      do_check_false(d.isActive);
      do_check_true(d.userDisabled);
      do_check_false(d.appDisabled);

      prepare_test({
        "theme2@tests.mozilla.org": [
          "onUninstalling",
        ],
        "{972ce4c6-7e08-4474-a285-3208198ce6fd}": [
          "onEnabling"
        ]
      });

      t2.uninstall();

      ensure_test_completed();

      restartManager(0);

      run_test_11();
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
  AddonManager.getAddon("theme1@tests.mozilla.org", function(t1) {
    do_check_neq(t1, null);

    run_test_12();
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
  AddonManager.getAddon("theme1@tests.mozilla.org", function(t1) {
    do_check_neq(t1, null);

    run_test_13();
  });
}

// Updating a custom theme in use should require a restart
function run_test_13() {
  AddonManager.getAddon("theme1@tests.mozilla.org", function(t1) {
    prepare_test({
      "theme1@tests.mozilla.org": [
        "onEnabling",
      ],
      "{972ce4c6-7e08-4474-a285-3208198ce6fd}": [
        "onDisabling"
      ]
    });

    t1.userDisabled = false;
    ensure_test_completed();
    restartManager(0);

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
  restartManager(1);

  AddonManager.getAddon("theme1@tests.mozilla.org", function(t1) {
    do_check_neq(t1, null);
    do_check_true(t1.isActive);

    end_test();
  });
}
