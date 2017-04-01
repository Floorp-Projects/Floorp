"use strict";

/**
 * This file contains test for 'theme' type WebExtension addons. Tests focus mostly
 * on interoperability between the different theme formats (XUL and LWT) and
 * Addon Manager integration.
 *
 * Coverage may overlap with other tests in this folder.
 */

const {LightweightThemeManager} = AM_Cu.import("resource://gre/modules/LightweightThemeManager.jsm", {});
const THEME_IDS = ["theme1@tests.mozilla.org", "theme3@tests.mozilla.org",
  "theme2@personas.mozilla.org", "default@tests.mozilla.org"];
const REQUIRE_RESTART = { [THEME_IDS[0]]: 1 };
const DEFAULT_THEME = THEME_IDS[3];

const profileDir = gProfD.clone();
profileDir.append("extensions");

// We remember the last/ currently active theme for tracking events.
var gActiveTheme = null;

add_task(function* setup_to_default_browserish_state() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  writeInstallRDFForExtension({
    id: THEME_IDS[0],
    version: "1.0",
    name: "Test 1",
    type: 4,
    skinnable: true,
    internalName: "theme1/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  }, profileDir);

  yield promiseWriteWebManifestForExtension({
    author: "Some author",
    manifest_version: 2,
    name: "Web Extension Name",
    version: "1.0",
    theme: { images: { headerURL: "example.png" } },
    applications: {
      gecko: {
        id: THEME_IDS[1]
      }
    }
  }, profileDir);

  // We need a default theme for some of these things to work but we have hidden
  // the one in the application directory.
  writeInstallRDFForExtension({
    id: DEFAULT_THEME,
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

  // We can add an LWT only after the Addon Manager was started.
  LightweightThemeManager.currentTheme = {
    id: THEME_IDS[2].substr(0, THEME_IDS[2].indexOf("@")),
    version: "1",
    name: "Bling",
    description: "SO MUCH BLING!",
    author: "Pixel Pusher",
    homepageURL: "http://mochi.test:8888/data/index.html",
    headerURL: "http://mochi.test:8888/data/header.png",
    previewURL: "http://mochi.test:8888/data/preview.png",
    iconURL: "http://mochi.test:8888/data/icon.png",
    textcolor: Math.random().toString(),
    accentcolor: Math.random().toString()
  };

  let [ t1, t2, t3, d ] = yield promiseAddonsByIDs(THEME_IDS);
  Assert.ok(t1, "Theme addon should exist");
  Assert.ok(t2, "Theme addon should exist");
  Assert.ok(t3, "Theme addon should exist");
  Assert.ok(d, "Theme addon should exist");

  t1.userDisabled = t2.userDisabled = t3.userDisabled = true;
  Assert.ok(!t1.isActive, "Theme should be disabled");
  Assert.ok(!t2.isActive, "Theme should be disabled");
  Assert.ok(!t3.isActive, "Theme should be disabled");
  Assert.ok(d.isActive, "Default theme should be active");

  yield promiseRestartManager();

  [ t1, t2, t3, d ] = yield promiseAddonsByIDs(THEME_IDS);
  Assert.ok(!t1.isActive, "Theme should still be disabled");
  Assert.ok(!t2.isActive, "Theme should still be disabled");
  Assert.ok(!t3.isActive, "Theme should still be disabled");
  Assert.ok(d.isActive, "Default theme should still be active");

  gActiveTheme = d.id;
});

/**
 * Set the `userDisabled` property of one specific theme and check if the theme
 * switching works as expected by checking the state of all installed themes.
 *
 * @param {String}  which    ID of the addon to set the `userDisabled` property on
 * @param {Boolean} disabled Flag value to switch to
 */
function* setDisabledStateAndCheck(which, disabled = false) {
  if (disabled)
    Assert.equal(which, gActiveTheme, "Only the active theme can be disabled");

  let themeToDisable = disabled ? which : gActiveTheme;
  let themeToEnable = disabled ? DEFAULT_THEME : which;
  let expectRestart = !!(REQUIRE_RESTART[themeToDisable] || REQUIRE_RESTART[themeToEnable]);

  let expectedStates = {
    [themeToDisable]: true,
    [themeToEnable]: false
  };
  let expectedEvents = {
    [themeToDisable]: [ [ "onDisabling", expectRestart ] ],
    [themeToEnable]: [ [ "onEnabling", expectRestart ] ]
  };
  if (!expectRestart) {
    expectedEvents[themeToDisable].push([ "onDisabled", false ]);
    expectedEvents[themeToEnable].push([ "onEnabled", false ]);
  }

  // Set the state of the theme to change.
  let theme = yield promiseAddonByID(which);
  prepare_test(expectedEvents);
  theme.userDisabled = disabled;

  let isDisabled;
  for (theme of yield promiseAddonsByIDs(THEME_IDS)) {
    isDisabled = (theme.id in expectedStates) ? expectedStates[theme.id] : true;
    Assert.equal(theme.userDisabled, isDisabled,
      `Theme '${theme.id}' should be ${isDisabled ? "dis" : "en"}abled`);
    // Some themes need a restart to get their act together.
    if (expectRestart && (theme.id == themeToEnable || theme.id == themeToDisable)) {
      let expectedFlag = theme.id == themeToEnable ? AddonManager.PENDING_ENABLE : AddonManager.PENDING_DISABLE;
      Assert.ok(hasFlag(theme.pendingOperations, expectedFlag),
        "When expecting a restart, the pending operation flags should match");
    } else {
      Assert.equal(theme.pendingOperations, AddonManager.PENDING_NONE,
        "There should be no pending operations when no restart is expected");
      Assert.equal(theme.isActive, !isDisabled,
        `Theme '${theme.id} should be ${isDisabled ? "in" : ""}active`);
    }
  }

  yield promiseRestartManager();

  // All should still be good after a restart of the Addon Manager.
  for (theme of yield promiseAddonsByIDs(THEME_IDS)) {
    isDisabled = (theme.id in expectedStates) ? expectedStates[theme.id] : true;
    Assert.equal(theme.userDisabled, isDisabled,
      `Theme '${theme.id}' should be ${isDisabled ? "dis" : "en"}abled`);
    Assert.equal(theme.isActive, !isDisabled,
      `Theme '${theme.id}' should be ${isDisabled ? "in" : ""}active`);
    Assert.equal(theme.pendingOperations, AddonManager.PENDING_NONE,
      "There should be no pending operations left");
    if (!isDisabled)
      gActiveTheme = theme.id;
  }

  ensure_test_completed();
}

add_task(function* test_complete_themes() {
  // Enable the complete theme.
  yield* setDisabledStateAndCheck(THEME_IDS[0]);

  // Disabling the complete theme should revert to the default theme.
  yield* setDisabledStateAndCheck(THEME_IDS[0], true);

  // Enable it again.
  yield* setDisabledStateAndCheck(THEME_IDS[0]);

  // Enabling a WebExtension theme should disable the active theme.
  yield* setDisabledStateAndCheck(THEME_IDS[1]);

  // Switching back should disable the WebExtension theme.
  yield* setDisabledStateAndCheck(THEME_IDS[0]);
});

add_task(function* test_WebExtension_themes() {
  // Enable the WebExtension theme.
  yield* setDisabledStateAndCheck(THEME_IDS[1]);

  // Disabling WebExtension should revert to the default theme.
  yield* setDisabledStateAndCheck(THEME_IDS[1], true);

  // Enable it again.
  yield* setDisabledStateAndCheck(THEME_IDS[1]);

  // Enabling an LWT should disable the active theme.
  yield* setDisabledStateAndCheck(THEME_IDS[2]);

  // Switching back should disable the LWT.
  yield* setDisabledStateAndCheck(THEME_IDS[1]);
});

add_task(function* test_LWTs() {
  // Start with enabling an LWT.
  yield* setDisabledStateAndCheck(THEME_IDS[2]);

  // Disabling LWT should revert to the default theme.
  yield* setDisabledStateAndCheck(THEME_IDS[2], true);

  // Enable it again.
  yield* setDisabledStateAndCheck(THEME_IDS[2]);

  // Enabling a WebExtension theme should disable the active theme.
  yield* setDisabledStateAndCheck(THEME_IDS[1]);

  // Switching back should disable the LWT.
  yield* setDisabledStateAndCheck(THEME_IDS[2]);
});

add_task(function* test_default_theme() {
  // Explicitly enable the default theme.
  yield* setDisabledStateAndCheck(DEFAULT_THEME);

  // Swith to the WebExtension theme.
  yield* setDisabledStateAndCheck(THEME_IDS[1]);

  // Enable it again.
  yield* setDisabledStateAndCheck(DEFAULT_THEME);
});
