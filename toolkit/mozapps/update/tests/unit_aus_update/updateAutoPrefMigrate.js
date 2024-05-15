/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

async function verifyPref(expectedValue) {
  let configValue = await UpdateUtils.getAppUpdateAutoEnabled();
  Assert.equal(
    configValue,
    expectedValue,
    "Value returned by getAppUpdateAutoEnabled should have " +
      "matched the expected value"
  );
}

async function run_test() {
  setupTestCommon(null);
  await standardInit();

  let configFile = getUpdateDirFile(FILE_UPDATE_CONFIG_JSON);

  // Test that, if there is no value to migrate, the default value is set.
  Services.prefs.setBoolPref("app.update.auto.migrated", false);
  Services.prefs.clearUserPref("app.update.auto");
  Assert.ok(!configFile.exists(), "Config file should not exist yet");
  await verifyPref(
    UpdateUtils.PER_INSTALLATION_PREFS["app.update.auto"].defaultValue
  );

  debugDump("about to remove config file");
  configFile.remove(false);

  // Test migration of a |false| value
  Services.prefs.setBoolPref("app.update.auto.migrated", false);
  Services.prefs.setBoolPref("app.update.auto", false);
  Assert.ok(!configFile.exists(), "Config file should have been removed");
  await verifyPref(false);

  // Test that migration doesn't happen twice
  Services.prefs.setBoolPref("app.update.auto", true);
  await verifyPref(false);

  // If the file is deleted after migration, the default value should be
  // returned, regardless of the pref value.
  debugDump("about to remove config file");
  configFile.remove(false);
  Assert.ok(!configFile.exists(), "Config file should have been removed");
  let configValue = await UpdateUtils.getAppUpdateAutoEnabled();
  Assert.equal(
    configValue,
    true,
    "getAppUpdateAutoEnabled should have returned the default value (true)"
  );

  // Setting a new value should cause the value to get written out again
  await UpdateUtils.setAppUpdateAutoEnabled(false);
  await verifyPref(false);

  // Test migration of a |true| value
  Services.prefs.setBoolPref("app.update.auto.migrated", false);
  Services.prefs.setBoolPref("app.update.auto", true);
  configFile.remove(false);
  Assert.ok(
    !configFile.exists(),
    "App update config file should have been removed"
  );
  await verifyPref(true);

  // Test that setting app.update.auto without migrating also works
  await UpdateUtils.setAppUpdateAutoEnabled(false);
  await verifyPref(false);

  doTestFinish();
}
