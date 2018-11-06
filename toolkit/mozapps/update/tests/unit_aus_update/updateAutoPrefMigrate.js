/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

ChromeUtils.import("resource://gre/modules/osfile.jsm", this);

// Checks both the value returned by the update service and the one written to
// the app update config file.
async function verifyPref(configFile, expectedValue) {
  let decoder = new TextDecoder();
  let configValue = await gAUS.getAutoUpdateIsEnabled();
  Assert.equal(configValue, expectedValue,
               "Value returned should have matched the expected value");
  let fileContents = await OS.File.read(configFile.path);
  let saveObject = JSON.parse(decoder.decode(fileContents));
  Assert.equal(saveObject["app.update.auto"], expectedValue,
               "Value in file should match expected");
}

async function run_test() {
  setupTestCommon();
  standardInit();

  let configFile = getUpdateConfigFile();

  let originalFileValue = await gAUS.getAutoUpdateIsEnabled();
  let originalConfigValue = Services.prefs.getBoolPref("app.update.auto", null);
  let originalMigrationValue =
    Services.prefs.getBoolPref("app.update.auto.migrated", null);

  // Test migration of a |false| value
  Services.prefs.setBoolPref("app.update.auto.migrated", false);
  Services.prefs.setBoolPref("app.update.auto", false);
  debugDump(`about to remove config file`);
  configFile.remove(false);
  Assert.ok(!configFile.exists(), "Pref file should have been removed");
  await verifyPref(configFile, false);

  // Test that migration doesn't happen twice
  Services.prefs.setBoolPref("app.update.auto", true);
  await verifyPref(configFile, false);

  // If the file is deleted after migration, the default value should be
  // returned, regardless of the pref value.
  debugDump(`about to remove config file`);
  configFile.remove(false);
  Assert.ok(!configFile.exists(), "Pref file should have been removed");
  let configValue = await gAUS.getAutoUpdateIsEnabled();
  Assert.equal(configValue, true, "getAutoUpdateIsEnabled should have " +
                                  "returned the default value (true)");

  // Setting a new value should cause the value to get written out again
  await gAUS.setAutoUpdateIsEnabled(false);
  await verifyPref(configFile, false);

  // Test migration of a |true| value
  Services.prefs.setBoolPref("app.update.auto.migrated", false);
  Services.prefs.setBoolPref("app.update.auto", true);
  configFile.remove(false);
  Assert.ok(!configFile.exists(), "App update config file should have been " +
                                  "removed");
  await verifyPref(configFile, true);

  // Test that setting app.update.auto without migrating also works
  await gAUS.setAutoUpdateIsEnabled(false);
  await verifyPref(configFile, false);

  // Restore original state
  await gAUS.setAutoUpdateIsEnabled(originalFileValue);
  if (originalConfigValue == null) {
    Services.prefs.clearUserPref("app.update.auto");
  } else {
    Services.prefs.setBoolPref("app.update.auto", originalConfigValue);
  }
  if (originalMigrationValue == null) {
    Services.prefs.clearUserPref("app.update.auto.migrated");
  } else {
    Services.prefs.setBoolPref("app.update.auto.migrated",
                               originalMigrationValue);
  }
  doTestFinish();
}
