/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Make sure we don't accidentally start a background update while the prefs
// are enabled.
disableBackgroundUpdateTimer();
registerCleanupFunction(() => {
  enableBackgroundUpdateTimer();
});

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm",
  {}
);

const PREF_UPDATE_ENABLED = "extensions.update.enabled";
const PREF_AUTOUPDATE_DEFAULT = "extensions.update.autoUpdateDefault";

add_task(async function testUpdateAutomaticallyButton() {
  Services.telemetry.clearEvents();
  SpecialPowers.pushPrefEnv({
    set: [
      [PREF_UPDATE_ENABLED, true],
      [PREF_AUTOUPDATE_DEFAULT, true],
    ],
  });

  let win = await loadInitialView("extension");

  let toggleAutomaticButton = win.document.querySelector(
    '#page-options [action="set-update-automatically"]'
  );

  info("Verify the checked state reflects the update state");
  ok(toggleAutomaticButton.checked, "Automatic updates button is checked");

  AddonManager.autoUpdateDefault = false;
  ok(!toggleAutomaticButton.checked, "Automatic updates button is unchecked");

  AddonManager.autoUpdateDefault = true;
  ok(toggleAutomaticButton.checked, "Automatic updates button is re-checked");

  info("Verify that clicking the button changes the update state");
  ok(AddonManager.autoUpdateDefault, "Auto updates are default");
  ok(AddonManager.updateEnabled, "Updates are enabled");

  toggleAutomaticButton.click();
  ok(!AddonManager.autoUpdateDefault, "Auto updates are disabled");
  ok(AddonManager.updateEnabled, "Updates are enabled");

  toggleAutomaticButton.click();
  ok(AddonManager.autoUpdateDefault, "Auto updates are enabled again");
  ok(AddonManager.updateEnabled, "Updates are enabled");

  assertAboutAddonsTelemetryEvents(
    [
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "enabled",
        { action: "setUpdatePolicy", view: "list" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        "default,enabled",
        { action: "setUpdatePolicy", view: "list" },
      ],
    ],
    { methods: ["action"] }
  );

  await closeView(win);
});

add_task(async function testResetUpdateStates() {
  let id = "update-state@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id } },
    },
    useAddonManager: "permanent",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let resetStateButton = win.document.querySelector(
    '#page-options [action="reset-update-states"]'
  );

  info("Changing add-on update state");
  let addon = await AddonManager.getAddonByID(id);

  let setAddonUpdateState = async updateState => {
    let changed = AddonTestUtils.promiseAddonEvent("onPropertyChanged");
    addon.applyBackgroundUpdates = updateState;
    await changed;
    let addonState = addon.applyBackgroundUpdates;
    is(addonState, updateState, `Add-on updates are ${updateState}`);
  };

  await setAddonUpdateState(AddonManager.AUTOUPDATE_DISABLE);

  let propertyChanged = AddonTestUtils.promiseAddonEvent("onPropertyChanged");
  resetStateButton.click();
  await propertyChanged;
  is(
    addon.applyBackgroundUpdates,
    AddonManager.AUTOUPDATE_DEFAULT,
    "Add-on is reset to default updates"
  );

  await setAddonUpdateState(AddonManager.AUTOUPDATE_ENABLE);

  propertyChanged = AddonTestUtils.promiseAddonEvent("onPropertyChanged");
  resetStateButton.click();
  await propertyChanged;
  is(
    addon.applyBackgroundUpdates,
    AddonManager.AUTOUPDATE_DEFAULT,
    "Add-on is reset to default updates again"
  );

  info("Check the label on the button as the global state changes");
  is(
    win.document.l10n.getAttributes(resetStateButton).id,
    "addon-updates-reset-updates-to-automatic",
    "The reset button label says it resets to automatic"
  );

  info("Disable auto updating globally");
  AddonManager.autoUpdateDefault = false;

  is(
    win.document.l10n.getAttributes(resetStateButton).id,
    "addon-updates-reset-updates-to-manual",
    "The reset button label says it resets to manual"
  );

  assertAboutAddonsTelemetryEvents(
    [
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        { action: "resetUpdatePolicy", view: "list" },
      ],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        { action: "resetUpdatePolicy", view: "list" },
      ],
    ],
    { methods: ["action"] }
  );

  await closeView(win);
  await extension.unload();
});
