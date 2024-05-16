/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

let gPolicyFunctionResult;

async function testSetup() {
  // The setup functions we use for update testing typically allow for update.
  // But we are just testing preferences here. We don't want anything to
  // actually attempt to update. Also, because we are messing with the pref
  // system itself in this test, we want to make sure to use a pref outside of
  // that system to disable update.
  Services.prefs.setBoolPref(PREF_APP_UPDATE_DISABLEDFORTESTING, true);

  // We run these tests whether per-installation prefs are supported or not,
  // because this API needs to work in both cases.
  logTestInfo(
    "PER_INSTALLATION_PREFS_SUPPORTED = " +
      UpdateUtils.PER_INSTALLATION_PREFS_SUPPORTED.toString()
  );

  // Save the original state so we can restore it after the test.
  const originalPerInstallationPrefs = UpdateUtils.PER_INSTALLATION_PREFS;
  let configFile = getUpdateDirFile(FILE_UPDATE_CONFIG_JSON);
  try {
    configFile.moveTo(null, FILE_BACKUP_UPDATE_CONFIG_JSON);
  } catch (e) {}

  // Currently, features exist for per-installation prefs that are not used by
  // any actual pref. We added them because we intend to use these features in
  // the future. Thus, we will override the normally defined per-installation
  // prefs and provide our own, for the purposes of testing.
  UpdateUtils.PER_INSTALLATION_PREFS = {
    "test.pref.boolean": {
      type: UpdateUtils.PER_INSTALLATION_PREF_TYPE_BOOL,
      defaultValue: true,
      observerTopic: "test-pref-change-observer-boolean",
    },
    "test.pref.integer": {
      type: UpdateUtils.PER_INSTALLATION_PREF_TYPE_INT,
      defaultValue: 1234,
      observerTopic: "test-pref-change-observer-integer",
    },
    "test.pref.string": {
      type: UpdateUtils.PER_INSTALLATION_PREF_TYPE_ASCII_STRING,
      defaultValue: "<default>",
      observerTopic: "test-pref-change-observer-string",
    },
    "test.pref.policy": {
      type: UpdateUtils.PER_INSTALLATION_PREF_TYPE_INT,
      defaultValue: 1234,
      observerTopic: "test-pref-change-observer-policy",
      policyFn: () => gPolicyFunctionResult,
    },
  };
  // We need to re-initialize the pref system with these new prefs
  UpdateUtils.initPerInstallPrefs();

  registerCleanupFunction(() => {
    if (UpdateUtils.PER_INSTALLATION_PREFS_SUPPORTED) {
      let testConfigFile = getUpdateDirFile(FILE_UPDATE_CONFIG_JSON);
      let backupConfigFile = getUpdateDirFile(FILE_BACKUP_UPDATE_CONFIG_JSON);
      try {
        testConfigFile.remove(false);
        backupConfigFile.moveTo(null, FILE_UPDATE_CONFIG_JSON);
      } catch (ex) {}
    } else {
      for (const prefName in UpdateUtils.PER_INSTALLATION_PREFS) {
        Services.prefs.clearUserPref(prefName);
      }
    }

    UpdateUtils.PER_INSTALLATION_PREFS = originalPerInstallationPrefs;
    UpdateUtils.initPerInstallPrefs();
  });
}

let gObserverSeenCount = 0;
let gExpectedObserverData;
function observerCallback(subject, topic, data) {
  gObserverSeenCount += 1;
  Assert.equal(
    data,
    gExpectedObserverData,
    `Expected observer to have data: "${gExpectedObserverData}". ` +
      `It actually has data: "${data}"`
  );
}

async function changeAndVerifyPref(
  prefName,
  expectedInitialValue,
  newValue,
  setterShouldThrow = false
) {
  let initialValue = await UpdateUtils.readUpdateConfigSetting(prefName);
  Assert.strictEqual(
    initialValue,
    expectedInitialValue,
    `Expected pref '${prefName}' to have an initial value of ` +
      `${JSON.stringify(expectedInitialValue)}. Its actual initial value is ` +
      `${JSON.stringify(initialValue)}`
  );

  let expectedObserverCount = 1;
  if (initialValue == newValue || setterShouldThrow) {
    expectedObserverCount = 0;
  }

  let observerTopic =
    UpdateUtils.PER_INSTALLATION_PREFS[prefName].observerTopic;
  gObserverSeenCount = 0;
  gExpectedObserverData = newValue.toString();
  Services.obs.addObserver(observerCallback, observerTopic);

  let returned;
  let exceptionThrown;
  try {
    returned = await UpdateUtils.writeUpdateConfigSetting(prefName, newValue);
  } catch (e) {
    exceptionThrown = e;
  }
  if (setterShouldThrow) {
    Assert.ok(!!exceptionThrown, "Expected an exception to be thrown");
  } else {
    Assert.ok(
      !exceptionThrown,
      `Unexpected exception thrown by writeUpdateConfigSetting: ` +
        `${exceptionThrown}`
    );
  }

  if (!exceptionThrown) {
    Assert.strictEqual(
      returned,
      newValue,
      `Expected writeUpdateConfigSetting to return ` +
        `${JSON.stringify(newValue)}. It actually returned ` +
        `${JSON.stringify(returned)}`
    );
  }

  let readValue = await UpdateUtils.readUpdateConfigSetting(prefName);
  let expectedReadValue = exceptionThrown ? expectedInitialValue : newValue;
  Assert.strictEqual(
    readValue,
    expectedReadValue,
    `Expected pref '${prefName}' to be ${JSON.stringify(expectedReadValue)}.` +
      ` It was actually ${JSON.stringify(readValue)}.`
  );

  Assert.equal(
    gObserverSeenCount,
    expectedObserverCount,
    `Expected to see observer fire ${expectedObserverCount} times. It ` +
      `actually fired ${gObserverSeenCount} times.`
  );
  Services.obs.removeObserver(observerCallback, observerTopic);
}

async function run_test() {
  setupTestCommon(null);
  await standardInit();
  await testSetup();

  logTestInfo("Testing boolean pref and its observer");
  let pref = "test.pref.boolean";
  let defaultValue = UpdateUtils.PER_INSTALLATION_PREFS[pref].defaultValue;
  await changeAndVerifyPref(pref, defaultValue, defaultValue);
  await changeAndVerifyPref(pref, defaultValue, !defaultValue);
  await changeAndVerifyPref(pref, !defaultValue, !defaultValue);
  await changeAndVerifyPref(pref, !defaultValue, defaultValue);
  await changeAndVerifyPref(pref, defaultValue, defaultValue);
  await changeAndVerifyPref(pref, defaultValue, "true", true);
  await changeAndVerifyPref(pref, defaultValue, 1, true);

  logTestInfo("Testing string pref and its observer");
  pref = "test.pref.string";
  defaultValue = UpdateUtils.PER_INSTALLATION_PREFS[pref].defaultValue;
  await changeAndVerifyPref(pref, defaultValue, defaultValue);
  await changeAndVerifyPref(pref, defaultValue, defaultValue + "1");
  await changeAndVerifyPref(pref, defaultValue + "1", "");
  await changeAndVerifyPref(pref, "", 1, true);
  await changeAndVerifyPref(pref, "", true, true);

  logTestInfo("Testing integer pref and its observer");
  pref = "test.pref.integer";
  defaultValue = UpdateUtils.PER_INSTALLATION_PREFS[pref].defaultValue;
  await changeAndVerifyPref(pref, defaultValue, defaultValue);
  await changeAndVerifyPref(pref, defaultValue, defaultValue + 1);
  await changeAndVerifyPref(pref, defaultValue + 1, 0);
  await changeAndVerifyPref(pref, 0, "1", true);
  await changeAndVerifyPref(pref, 0, true, true);

  // Testing that the default pref branch works the same way that the default
  // branch works for our per-profile prefs.
  logTestInfo("Testing default branch behavior");
  pref = "test.pref.integer";
  let originalDefault = UpdateUtils.PER_INSTALLATION_PREFS[pref].defaultValue;
  // Make sure the value is the default value, then change the default value
  // and check that the effective value changes.
  await UpdateUtils.writeUpdateConfigSetting(pref, originalDefault, {
    setDefaultOnly: true,
  });
  await UpdateUtils.writeUpdateConfigSetting(pref, originalDefault);
  await UpdateUtils.writeUpdateConfigSetting(pref, originalDefault + 1, {
    setDefaultOnly: true,
  });
  Assert.strictEqual(
    await UpdateUtils.readUpdateConfigSetting(pref),
    originalDefault + 1,
    `Expected that changing the default of a pref with no user value should ` +
      `change the effective value`
  );
  // Now make the user value different from the default value and ensure that
  // changing the default value does not affect the effective value
  await UpdateUtils.writeUpdateConfigSetting(pref, originalDefault + 10);
  await UpdateUtils.writeUpdateConfigSetting(pref, originalDefault + 20, {
    setDefaultOnly: true,
  });
  Assert.strictEqual(
    await UpdateUtils.readUpdateConfigSetting(pref),
    originalDefault + 10,
    `Expected that changing the default of a pref with a user value should ` +
      `NOT change the effective value`
  );

  logTestInfo("Testing policy behavior");
  pref = "test.pref.policy";
  defaultValue = UpdateUtils.PER_INSTALLATION_PREFS[pref].defaultValue;
  gPolicyFunctionResult = null;
  await changeAndVerifyPref(pref, defaultValue, defaultValue + 1);
  gPolicyFunctionResult = defaultValue + 10;
  await changeAndVerifyPref(pref, gPolicyFunctionResult, 0, true);

  doTestFinish();
}
