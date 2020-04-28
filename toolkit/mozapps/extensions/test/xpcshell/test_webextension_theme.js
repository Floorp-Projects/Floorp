"use strict";

/**
 * This file contains test for 'theme' type WebExtension addons. Tests focus mostly
 * on interoperability between the different theme formats (XUL and LWT) and
 * Addon Manager integration.
 *
 * Coverage may overlap with other tests in this folder.
 */

const THEME_IDS = [
  "theme3@tests.mozilla.org",
  "theme2@personas.mozilla.org", // Unused. Legacy. Evil.
  "default-theme@mozilla.org",
];
const REAL_THEME_IDS = [THEME_IDS[0], THEME_IDS[2]];
const DEFAULT_THEME = THEME_IDS[2];

const profileDir = gProfD.clone();
profileDir.append("extensions");

Services.prefs.setIntPref(
  "extensions.enabledScopes",
  AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION
);

// We remember the last/ currently active theme for tracking events.
var gActiveTheme = null;

add_task(async function setup_to_default_browserish_state() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  await promiseWriteWebManifestForExtension(
    {
      author: "Some author",
      manifest_version: 2,
      name: "Web Extension Name",
      version: "1.0",
      theme: { images: { theme_frame: "example.png" } },
      applications: {
        gecko: {
          id: THEME_IDS[0],
        },
      },
    },
    profileDir
  );

  await promiseStartupManager();

  if (AppConstants.MOZ_DEV_EDITION) {
    // Developer Edition selects the wrong theme by default.
    let defaultTheme = await AddonManager.getAddonByID(DEFAULT_THEME);
    await defaultTheme.enable();
  }

  let [t1, t2, d] = await promiseAddonsByIDs(THEME_IDS);
  Assert.ok(t1, "Theme addon should exist");
  Assert.equal(t2, null, "Theme addon is not a thing anymore");
  Assert.ok(d, "Theme addon should exist");

  await t1.disable();
  await new Promise(executeSoon);
  Assert.ok(!t1.isActive, "Theme should be disabled");
  Assert.ok(d.isActive, "Default theme should be active");

  await promiseRestartManager();

  [t1, t2, d] = await promiseAddonsByIDs(THEME_IDS);
  Assert.ok(!t1.isActive, "Theme should still be disabled");
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
async function setDisabledStateAndCheck(which, disabled = false) {
  if (disabled) {
    Assert.equal(which, gActiveTheme, "Only the active theme can be disabled");
  }

  let themeToDisable = disabled ? which : gActiveTheme;
  let themeToEnable = disabled ? DEFAULT_THEME : which;

  let expectedStates = {
    [themeToDisable]: true,
    [themeToEnable]: false,
  };
  let addonEvents = {
    [themeToDisable]: [{ event: "onDisabling" }, { event: "onDisabled" }],
    [themeToEnable]: [{ event: "onEnabling" }, { event: "onEnabled" }],
  };

  // Set the state of the theme to change.
  let theme = await promiseAddonByID(which);
  await expectEvents({ addonEvents }, () => {
    if (disabled) {
      theme.disable();
    } else {
      theme.enable();
    }
  });

  let isDisabled;
  for (theme of await promiseAddonsByIDs(REAL_THEME_IDS)) {
    isDisabled = theme.id in expectedStates ? expectedStates[theme.id] : true;
    Assert.equal(
      theme.userDisabled,
      isDisabled,
      `Theme '${theme.id}' should be ${isDisabled ? "dis" : "en"}abled`
    );
    Assert.equal(
      theme.pendingOperations,
      AddonManager.PENDING_NONE,
      "There should be no pending operations when no restart is expected"
    );
    Assert.equal(
      theme.isActive,
      !isDisabled,
      `Theme '${theme.id} should be ${isDisabled ? "in" : ""}active`
    );
  }

  await promiseRestartManager();

  // All should still be good after a restart of the Addon Manager.
  for (theme of await promiseAddonsByIDs(REAL_THEME_IDS)) {
    isDisabled = theme.id in expectedStates ? expectedStates[theme.id] : true;
    Assert.equal(
      theme.userDisabled,
      isDisabled,
      `Theme '${theme.id}' should be ${isDisabled ? "dis" : "en"}abled`
    );
    Assert.equal(
      theme.isActive,
      !isDisabled,
      `Theme '${theme.id}' should be ${isDisabled ? "in" : ""}active`
    );
    Assert.equal(
      theme.pendingOperations,
      AddonManager.PENDING_NONE,
      "There should be no pending operations left"
    );
    if (!isDisabled) {
      gActiveTheme = theme.id;
    }
  }
}

add_task(async function test_WebExtension_themes() {
  // Enable the WebExtension theme.
  await setDisabledStateAndCheck(THEME_IDS[0]);

  // Disabling WebExtension should revert to the default theme.
  await setDisabledStateAndCheck(THEME_IDS[0], true);

  // Enable it again.
  await setDisabledStateAndCheck(THEME_IDS[0]);
});

add_task(async function test_default_theme() {
  // Explicitly enable the default theme.
  await setDisabledStateAndCheck(DEFAULT_THEME);

  // Swith to the WebExtension theme.
  await setDisabledStateAndCheck(THEME_IDS[0]);

  // Enable it again.
  await setDisabledStateAndCheck(DEFAULT_THEME);
});

add_task(async function uninstall_offers_undo() {
  let defaultTheme = await AddonManager.getAddonByID(DEFAULT_THEME);
  const ID = THEME_IDS[0];
  let theme = await promiseAddonByID(ID);

  Assert.ok(theme, "Webextension theme is present");

  async function promiseAddonEvent(event, id) {
    let [addon] = await AddonTestUtils.promiseAddonEvent(event);
    if (id) {
      Assert.equal(addon.id, id, `Got event for expected addon (${event})`);
    }
  }

  async function uninstallTheme() {
    let uninstallingPromise = promiseAddonEvent("onUninstalling", ID);
    await theme.uninstall(true);
    await uninstallingPromise;

    Assert.ok(
      hasFlag(theme.pendingOperations, AddonManager.PENDING_UNINSTALL),
      "Theme being uninstalled has PENDING_UNINSTALL flag"
    );
  }

  async function cancelUninstallTheme() {
    let cancelPromise = promiseAddonEvent("onOperationCancelled", ID);
    theme.cancelUninstall();
    await cancelPromise;

    Assert.equal(
      theme.pendingOperations,
      AddonManager.PENDING_NONE,
      "PENDING_UNINSTALL flag is cleared when uninstall is canceled"
    );
  }

  // A theme should still be disabled if the uninstallation of a disabled theme
  // is undone.
  Assert.ok(!theme.isActive, "Webextension theme is not active");
  Assert.ok(defaultTheme.isActive, "Default theme is active");
  await uninstallTheme();
  await cancelUninstallTheme();
  Assert.ok(!theme.isActive, "Webextension theme is still not active");
  Assert.ok(defaultTheme.isActive, "Default theme is still active");

  // Enable theme, the previously active theme should be disabled.
  await Promise.all([
    promiseAddonEvent("onDisabled", DEFAULT_THEME),
    promiseAddonEvent("onEnabled", ID),
    theme.enable(),
  ]);
  Assert.ok(theme.isActive, "Webextension theme is active after enabling");
  Assert.ok(!defaultTheme.isActive, "Default theme is not active any more");

  // Uninstall active theme, default theme should become active.
  await Promise.all([
    // Note: no listener for onDisabled & ID because the uninstall is pending.
    promiseAddonEvent("onEnabled", DEFAULT_THEME),
    uninstallTheme(),
  ]);
  Assert.ok(!theme.isActive, "Webextension theme is not active upon uninstall");
  Assert.ok(defaultTheme.isActive, "Default theme is active again");

  // Undo uninstall, default theme should be deactivated.
  await Promise.all([
    // Note: no listener for onEnabled & ID because the uninstall was pending.
    promiseAddonEvent("onDisabled", DEFAULT_THEME),
    cancelUninstallTheme(),
  ]);
  Assert.ok(theme.isActive, "Webextension theme is active upon undo uninstall");
  Assert.ok(!defaultTheme.isActive, "Default theme is not active again");

  // Immediately remove the theme. Default theme should be activated.
  await Promise.all([
    promiseAddonEvent("onEnabled", DEFAULT_THEME),
    theme.uninstall(),
  ]);

  await promiseRestartManager();
});

// Test that default_locale works with WE themes
add_task(async function default_locale_themes() {
  let addon = await promiseInstallWebExtension({
    manifest: {
      default_locale: "en",
      name: "__MSG_name__",
      description: "__MSG_description__",
      theme: {
        colors: {
          frame: "black",
          tab_background_text: "white",
        },
      },
    },
    files: {
      "_locales/en/messages.json": `{
        "name": {
          "message": "the name"
        },
        "description": {
          "message": "the description"
        }
      }`,
    },
  });

  addon = await promiseAddonByID(addon.id);
  equal(addon.name, "the name");
  equal(addon.description, "the description");
  equal(addon.type, "theme");
  await addon.uninstall();
});

add_task(async function test_theme_update() {
  let addon = await AddonManager.getAddonByID(DEFAULT_THEME);
  ok(!addon.userDisabled, "default theme is enabled");

  await AddonTestUtils.promiseRestartManager(2);

  addon = await AddonManager.getAddonByID(DEFAULT_THEME);
  ok(!addon.userDisabled, "default theme is enabled after upgrade");
});
