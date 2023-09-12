/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Loads the settings file and ensures it has not already been migrated.
 *
 * @param {string} settingsFile The settings file to load
 */
async function loadSettingsFile(settingsFile) {
  let settingsTemplate = await readJSONFile(do_get_file(settingsFile));

  Assert.less(
    settingsTemplate.version,
    7,
    "Should be a version older than when hideOneOffs was moved into settings"
  );
  for (let engine of settingsTemplate.engines) {
    Assert.ok(!("id" in engine));
  }

  await promiseSaveSettingsData(settingsTemplate);
}

/**
 * Test reading from search.json.mozlz4
 */
add_setup(async function () {
  await SearchTestUtils.useTestEngines("data1");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_migration_from_pre_ids() {
  await loadSettingsFile("data/search-legacy.json");

  Services.prefs.setStringPref("browser.search.hiddenOneOffs", "engine1");

  const settingsFileWritten = promiseAfterSettings();

  await Services.search.wrappedJSObject.reset();
  await Services.search.init();

  await settingsFileWritten;

  const engine1 = await Services.search.getEngineByName("engine1");
  const engine2 = await Services.search.getEngineByName("engine2");

  Assert.ok(
    engine1.hideOneOffButton,
    "Should have hideOneOffButton set to true"
  );
  Assert.ok(
    !engine2.hideOneOffButton,
    "Should have hideOneOffButton set to false"
  );

  Assert.ok(
    !Services.prefs.prefHasUserValue("browser.search.hiddenOneOffs"),
    "HiddenOneOffs pref is cleared"
  );
});
