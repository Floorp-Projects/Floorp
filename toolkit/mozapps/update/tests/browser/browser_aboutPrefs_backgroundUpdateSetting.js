/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

/**
 * This file tests the background update UI in about:preferences.
 */

ChromeUtils.defineModuleGetter(
  this,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);

const ENABLE_UI_PREF = "app.update.background.scheduling.enabled";
const BACKGROUND_UPDATE_PREF = "app.update.background.enabled";

add_task(async function testBackgroundUpdateSettingUI() {
  if (!AppConstants.MOZ_UPDATE_AGENT) {
    // The element that we are testing in about:preferences is #ifdef'ed out of
    // the file if MOZ_UPDATE_AGENT isn't defined. So there is nothing to
    // test in that case.
    logTestInfo(
      `
===============================================================================
WARNING! This test involves background update, but background tasks are
         disabled. This test will unconditionally pass since the feature it
         wants to test isn't available.
===============================================================================
`
    );
    // Some of our testing environments do not consider a test to have passed if
    // it didn't make any assertions.
    ok(true, "Unconditionally passing test");
    return;
  }

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );

  const originalBackgroundUpdateVal = await UpdateUtils.readUpdateConfigSetting(
    BACKGROUND_UPDATE_PREF
  );
  const originalUpdateAutoVal = await UpdateUtils.getAppUpdateAutoEnabled();
  registerCleanupFunction(async () => {
    await BrowserTestUtils.removeTab(tab);
    Services.prefs.clearUserPref(ENABLE_UI_PREF);
    await UpdateUtils.writeUpdateConfigSetting(
      BACKGROUND_UPDATE_PREF,
      originalBackgroundUpdateVal
    );
    await UpdateUtils.setAppUpdateAutoEnabled(originalUpdateAutoVal);
  });

  // For a little while longer, we want to have tests that assert the
  // default value.
  let defaultValue =
    AppConstants.NIGHTLY_BUILD && AppConstants.platform == "win";
  is(
    Services.prefs.getBoolPref(ENABLE_UI_PREF, false),
    defaultValue,
    `${ENABLE_UI_PREF} should default to ${defaultValue}.`
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [defaultValue], defaultValue => {
    is(
      content.document.getElementById("backgroundUpdate").hidden,
      !defaultValue,
      `The background update UI should be ${
        defaultValue ? "shown" : "hidden"
      } when app.update.scheduling.enabled is ${defaultValue}.`
    );
  });

  Services.prefs.setBoolPref(ENABLE_UI_PREF, true);
  // app.update.background.scheduling.enabled does not dynamically update the
  // about:preferences page, so we need to reload it when we set this pref.
  await BrowserTestUtils.removeTab(tab);
  tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );

  // If auto update is disabled, the control for background update should be
  // disabled, since we cannot update in the background if we can't update
  // automatically.
  await UpdateUtils.setAppUpdateAutoEnabled(false);
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [UpdateUtils.PER_INSTALLATION_PREFS_SUPPORTED],
    async perInstallationPrefsSupported => {
      let backgroundUpdateCheckbox = content.document.getElementById(
        "backgroundUpdate"
      );
      is(
        backgroundUpdateCheckbox.hidden,
        !perInstallationPrefsSupported,
        `The background update UI should ${
          perInstallationPrefsSupported ? "not" : ""
        } be hidden when app.update.background.scheduling.enabled is true ` +
          `and perInstallationPrefsSupported is ${perInstallationPrefsSupported}`
      );
      if (perInstallationPrefsSupported) {
        is(
          backgroundUpdateCheckbox.disabled,
          true,
          `The background update UI should be disabled when auto update is ` +
            `disabled`
        );
      }
    }
  );

  if (!UpdateUtils.PER_INSTALLATION_PREFS_SUPPORTED) {
    // The remaining tests only make sense on platforms where per-installation
    // prefs are supported and the UI will ever actually be displayed
    return;
  }

  await UpdateUtils.setAppUpdateAutoEnabled(true);
  await UpdateUtils.writeUpdateConfigSetting(BACKGROUND_UPDATE_PREF, true);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    let backgroundUpdateCheckbox = content.document.getElementById(
      "backgroundUpdate"
    );
    is(
      backgroundUpdateCheckbox.disabled,
      false,
      `The background update UI should not be disabled when auto update is ` +
        `enabled`
    );

    is(
      backgroundUpdateCheckbox.checked,
      true,
      "After enabling background update, the checkbox should be checked"
    );

    // Note that this action results in asynchronous activity. Normally when
    // we change the update config, we await on the function to wait for the
    // value to be written to the disk. We can't easily await on the UI state
    // though. Luckily, we don't have to because reads/writes of the config file
    // are serialized. So when we verify the written value by awaiting on
    // readUpdateConfigSetting(), that will also wait for the value to be
    // written to disk and for this UI to react to that.
    backgroundUpdateCheckbox.click();
  });

  is(
    await UpdateUtils.readUpdateConfigSetting(BACKGROUND_UPDATE_PREF),
    false,
    "Toggling the checkbox should have changed the setting value to false"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    let backgroundUpdateCheckbox = content.document.getElementById(
      "backgroundUpdate"
    );
    is(
      backgroundUpdateCheckbox.checked,
      false,
      "After toggling the checked checkbox, it should be unchecked."
    );

    // Like the last call like this one, this initiates asynchronous behavior.
    backgroundUpdateCheckbox.click();
  });

  is(
    await UpdateUtils.readUpdateConfigSetting(BACKGROUND_UPDATE_PREF),
    true,
    "Toggling the checkbox should have changed the setting value to true"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    is(
      content.document.getElementById("backgroundUpdate").checked,
      true,
      "After toggling the unchecked checkbox, it should be checked"
    );
  });

  // Test that the UI reacts to observed setting changes properly.
  await UpdateUtils.writeUpdateConfigSetting(BACKGROUND_UPDATE_PREF, false);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    is(
      content.document.getElementById("backgroundUpdate").checked,
      false,
      "Externally disabling background update should uncheck the checkbox"
    );
  });

  await UpdateUtils.writeUpdateConfigSetting(BACKGROUND_UPDATE_PREF, true);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    is(
      content.document.getElementById("backgroundUpdate").checked,
      true,
      "Externally enabling background update should check the checkbox"
    );
  });
});
