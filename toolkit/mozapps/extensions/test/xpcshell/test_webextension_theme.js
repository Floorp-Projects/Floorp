"use strict";

/**
 * This file contains test for 'theme' type WebExtension addons. Tests focus mostly
 * on interoperability between the different theme formats (XUL and LWT) and
 * Addon Manager integration.
 *
 * Coverage may overlap with other tests in this folder.
 */

ChromeUtils.defineModuleGetter(this, "LightweightThemeManager",
                               "resource://gre/modules/LightweightThemeManager.jsm");
const THEME_IDS = [
  "theme3@tests.mozilla.org",
  "theme2@personas.mozilla.org",
  "default-theme@mozilla.org",
];
const DEFAULT_THEME = THEME_IDS[2];

const profileDir = gProfD.clone();
profileDir.append("extensions");

// We remember the last/ currently active theme for tracking events.
var gActiveTheme = null;

add_task(async function setup_to_default_browserish_state() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  await promiseWriteWebManifestForExtension({
    author: "Some author",
    manifest_version: 2,
    name: "Web Extension Name",
    version: "1.0",
    theme: { images: { headerURL: "example.png" } },
    applications: {
      gecko: {
        id: THEME_IDS[0]
      }
    }
  }, profileDir);

  await promiseStartupManager();

  // We can add an LWT only after the Addon Manager was started.
  LightweightThemeManager.currentTheme = {
    id: THEME_IDS[1].substr(0, THEME_IDS[1].indexOf("@")),
    version: "1",
    name: "Bling",
    description: "SO MUCH BLING!",
    author: "Pixel Pusher",
    homepageURL: "http://localhost:8888/data/index.html",
    headerURL: "http://localhost:8888/data/header.png",
    previewURL: "http://localhost:8888/data/preview.png",
    iconURL: "http://localhost:8888/data/icon.png",
    textcolor: Math.random().toString(),
    accentcolor: Math.random().toString()
  };

  let [ t1, t2, d ] = await promiseAddonsByIDs(THEME_IDS);
  Assert.ok(t1, "Theme addon should exist");
  Assert.ok(t2, "Theme addon should exist");
  Assert.ok(d, "Theme addon should exist");

  t1.userDisabled = t2.userDisabled = true;
  Assert.ok(!t1.isActive, "Theme should be disabled");
  Assert.ok(!t2.isActive, "Theme should be disabled");
  Assert.ok(d.isActive, "Default theme should be active");

  await promiseRestartManager();

  [ t1, t2, d ] = await promiseAddonsByIDs(THEME_IDS);
  Assert.ok(!t1.isActive, "Theme should still be disabled");
  Assert.ok(!t2.isActive, "Theme should still be disabled");
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
  if (disabled)
    Assert.equal(which, gActiveTheme, "Only the active theme can be disabled");

  let themeToDisable = disabled ? which : gActiveTheme;
  let themeToEnable = disabled ? DEFAULT_THEME : which;

  let expectedStates = {
    [themeToDisable]: true,
    [themeToEnable]: false
  };
  let expectedEvents = {
    [themeToDisable]: [
      [ "onDisabling", false ],
      [ "onDisabled", false ],
    ],
    [themeToEnable]: [
      [ "onEnabling", false ],
      [ "onEnabled", false ],
    ]
  };

  // Set the state of the theme to change.
  let theme = await promiseAddonByID(which);
  prepare_test(expectedEvents);
  theme.userDisabled = disabled;

  let isDisabled;
  for (theme of await promiseAddonsByIDs(THEME_IDS)) {
    isDisabled = (theme.id in expectedStates) ? expectedStates[theme.id] : true;
    Assert.equal(theme.userDisabled, isDisabled,
      `Theme '${theme.id}' should be ${isDisabled ? "dis" : "en"}abled`);
    Assert.equal(theme.pendingOperations, AddonManager.PENDING_NONE,
      "There should be no pending operations when no restart is expected");
    Assert.equal(theme.isActive, !isDisabled,
      `Theme '${theme.id} should be ${isDisabled ? "in" : ""}active`);
  }

  await promiseRestartManager();

  // All should still be good after a restart of the Addon Manager.
  for (theme of await promiseAddonsByIDs(THEME_IDS)) {
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

add_task(async function test_WebExtension_themes() {
  // Enable the WebExtension theme.
  await setDisabledStateAndCheck(THEME_IDS[0]);

  // Disabling WebExtension should revert to the default theme.
  await setDisabledStateAndCheck(THEME_IDS[0], true);

  // Enable it again.
  await setDisabledStateAndCheck(THEME_IDS[0]);

  // Enabling an LWT should disable the active theme.
  await setDisabledStateAndCheck(THEME_IDS[1]);

  // Switching back should disable the LWT.
  await setDisabledStateAndCheck(THEME_IDS[0]);
});

add_task(async function test_LWTs() {
  // Start with enabling an LWT.
  await setDisabledStateAndCheck(THEME_IDS[1]);

  // Disabling LWT should revert to the default theme.
  await setDisabledStateAndCheck(THEME_IDS[1], true);

  // Enable it again.
  await setDisabledStateAndCheck(THEME_IDS[1]);

  // Enabling a WebExtension theme should disable the active theme.
  await setDisabledStateAndCheck(THEME_IDS[0]);

  // Switching back should disable the LWT.
  await setDisabledStateAndCheck(THEME_IDS[1]);
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
  const ID = THEME_IDS[0];
  let theme = await promiseAddonByID(ID);

  Assert.ok(theme, "Webextension theme is present");
  Assert.ok(!theme.isActive, "Webextension theme is not active");

  function promiseAddonEvent(event, id) {
    return new Promise(resolve => {
      let listener = {
        // eslint-disable-next-line object-shorthand
        [event]: function(addon) {
          if (id) {
            Assert.equal(addon.id, id, "Got event for expected addon");
          }
          AddonManager.removeAddonListener(listener);
          resolve();
        }
      };
      AddonManager.addAddonListener(listener);
    });
  }

  let uninstallingPromise = promiseAddonEvent("onUninstalling", ID);
  theme.uninstall(true);
  await uninstallingPromise;

  Assert.ok(hasFlag(theme.pendingOperations, AddonManager.PENDING_UNINSTALL),
            "Theme being uninstalled has PENDING_UNINSTALL flag");

  let cancelPromise = promiseAddonEvent("onOperationCancelled", ID);
  theme.cancelUninstall();
  await cancelPromise;

  Assert.equal(theme.pendingOperations, AddonManager.PENDING_NONE,
               "PENDING_UNINSTALL flag is cleared when uninstall is canceled");

  theme.uninstall();
  await promiseRestartManager();
});

// Test that default_locale works with WE themes
add_task(async function default_locale_themes() {
  let addon = await promiseInstallWebExtension({
    manifest: {
      applications: {
        gecko: {
          id: "locale-theme@tests.mozilla.org",
        }
      },
      default_locale: "en",
      name: "__MSG_name__",
      description: "__MSG_description__",
      theme: {
        "colors": {
          "accentcolor": "black",
          "textcolor": "white",
        }
      }
    },
    files: {
      "_locales/en/messages.json": `{
        "name": {
          "message": "the name"
        },
        "description": {
          "message": "the description"
        }
      }`
    }
  });

  addon = await promiseAddonByID(addon.id);
  equal(addon.name, "the name");
  equal(addon.description, "the description");
  equal(addon.type, "theme");
  addon.uninstall();
});
