/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

ChromeUtils.defineESModuleGetters(this, {
  BackgroundUpdate: "resource://gre/modules/BackgroundUpdate.sys.mjs",
});

const transitionPerformedPref = "app.update.background.rolledout";
const backgroundUpdateEnabledPref = "app.update.background.enabled";
const defaultPrefValue =
  UpdateUtils.PER_INSTALLATION_PREFS[backgroundUpdateEnabledPref].defaultValue;

async function testTransition(options) {
  Services.prefs.clearUserPref(transitionPerformedPref);
  await UpdateUtils.writeUpdateConfigSetting(
    backgroundUpdateEnabledPref,
    options.initialDefaultValue,
    { setDefaultOnly: true }
  );
  await UpdateUtils.writeUpdateConfigSetting(
    backgroundUpdateEnabledPref,
    options.initialUserValue
  );
  BackgroundUpdate.ensureExperimentToRolloutTransitionPerformed();
  Assert.equal(
    await UpdateUtils.readUpdateConfigSetting(backgroundUpdateEnabledPref),
    options.expectedPostTransitionValue,
    "Post transition option value does not match the expected value"
  );

  // Make sure that we only do the transition once.

  // If we change the default value, then change the user value to the same
  // thing, we will end up with only a default value and no saved user value.
  // This allows us to ensure that we read the default value back out, if it is
  // changed.
  await UpdateUtils.writeUpdateConfigSetting(
    backgroundUpdateEnabledPref,
    !defaultPrefValue,
    { setDefaultOnly: true }
  );
  await UpdateUtils.writeUpdateConfigSetting(
    backgroundUpdateEnabledPref,
    !defaultPrefValue
  );
  BackgroundUpdate.ensureExperimentToRolloutTransitionPerformed();
  Assert.equal(
    await UpdateUtils.readUpdateConfigSetting(backgroundUpdateEnabledPref),
    !defaultPrefValue,
    "Transition should not change the pref value if it already ran"
  );
}

async function run_test() {
  setupTestCommon(null);
  await standardInit();
  // The setup functions we use for update testing typically allow for update.
  // But we are just testing preferences here. We don't want anything to
  // actually attempt to update. Also, because we are messing with the pref
  // system itself in this test, we want to make sure to use a pref outside of
  // that system to disable update.
  Services.prefs.setBoolPref(PREF_APP_UPDATE_DISABLEDFORTESTING, true);

  const originalBackgroundUpdateEnabled =
    await UpdateUtils.readUpdateConfigSetting(backgroundUpdateEnabledPref);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref(transitionPerformedPref);
    await UpdateUtils.writeUpdateConfigSetting(
      backgroundUpdateEnabledPref,
      originalBackgroundUpdateEnabled
    );
    await UpdateUtils.writeUpdateConfigSetting(
      backgroundUpdateEnabledPref,
      defaultPrefValue,
      { setDefaultOnly: true }
    );
  });

  await testTransition({
    initialDefaultValue: true,
    initialUserValue: true,
    expectedPostTransitionValue: true,
  });

  // Make sure we don't interfere with a user's choice to turn the feature off.
  await testTransition({
    initialDefaultValue: true,
    initialUserValue: false,
    expectedPostTransitionValue: false,
  });

  // In this case, there effectively is no user value since the user value
  // equals the default value. So the effective value should change after
  // the transition switches the default.
  await testTransition({
    initialDefaultValue: false,
    initialUserValue: false,
    expectedPostTransitionValue: true,
  });

  await testTransition({
    initialDefaultValue: false,
    initialUserValue: true,
    expectedPostTransitionValue: true,
  });

  doTestFinish();
}
