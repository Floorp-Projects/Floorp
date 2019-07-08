/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

ChromeUtils.import("resource://gre/modules/osfile.jsm", this);

// Checks both the value returned by the update service and the one written to
// the app update config file.
async function verifyPref(configFile, expectedValue) {
  let decoder = new TextDecoder();
  let configValue = await UpdateUtils.getAppUpdateAutoEnabled();
  Assert.equal(
    configValue,
    expectedValue,
    "Value returned by getAppUpdateAutoEnabled should have " +
      "matched the expected value"
  );
  let fileContents = await OS.File.read(configFile.path);
  let saveObject = JSON.parse(decoder.decode(fileContents));
  Assert.equal(
    saveObject["app.update.auto"],
    expectedValue,
    "Value in update config file should match expected"
  );
}

async function run_test() {
  setupTestCommon(null);
  standardInit();

  let configFile = getUpdateDirFile(FILE_UPDATE_CONFIG_JSON);

  // Test migration of a |false| value
  Services.prefs.setBoolPref("app.update.auto.migrated", false);
  Services.prefs.setBoolPref("app.update.auto", false);
  Assert.ok(!configFile.exists(), "Config file should not exist yet");
  await verifyPref(configFile, false);

  // Test that migration doesn't happen twice
  Services.prefs.setBoolPref("app.update.auto", true);
  await verifyPref(configFile, false);

  // If the file is deleted after migration, the default value should be
  // returned, regardless of the pref value.
  debugDump("about to remove config file");
  configFile.remove(false);
  Assert.ok(!configFile.exists(), "Pref file should have been removed");
  let configValue = await UpdateUtils.getAppUpdateAutoEnabled();
  Assert.equal(
    configValue,
    true,
    "getAppUpdateAutoEnabled should have returned the default value (true)"
  );

  // Setting a new value should cause the value to get written out again
  await UpdateUtils.setAppUpdateAutoEnabled(false);
  await verifyPref(configFile, false);

  // Test migration of a |true| value
  Services.prefs.setBoolPref("app.update.auto.migrated", false);
  Services.prefs.setBoolPref("app.update.auto", true);
  configFile.remove(false);
  Assert.ok(
    !configFile.exists(),
    "App update config file should have been removed"
  );
  await verifyPref(configFile, true);

  // Test that setting app.update.auto without migrating also works
  await UpdateUtils.setAppUpdateAutoEnabled(false);
  await verifyPref(configFile, false);

  doTestFinish();
}
