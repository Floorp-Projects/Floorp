/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test removing obsolete engine types on upgrade of settings.
 */

"use strict";

async function loadSettingsFile(settingsFile, name) {
  let settings = await readJSONFile(do_get_file(settingsFile));

  settings.metaData.current = name;
  settings.metaData.hash = SearchUtils.getVerificationHash(name);

  await promiseSaveSettingsData(settings);
}

/**
 * Start the search service and confirm the engine properties match the expected values.
 *
 * @param {string} settingsFile
 *   The path to the settings file to use.
 * @param {string} engineName
 *   The engine name that should be default and is being removed.
 */
async function checkLoadSettingProperties(settingsFile, engineName) {
  await loadSettingsFile(settingsFile, engineName);

  const settingsFileWritten = promiseAfterSettings();
  let ss = new SearchService();
  let result = await ss.init();

  Assert.ok(
    Components.isSuccessCode(result),
    "Should have successfully initialized the search service"
  );

  await settingsFileWritten;

  let engines = await ss.getEngines();

  Assert.deepEqual(
    engines.map(e => e.name),
    ["engine1", "engine2"],
    "Should have only loaded the app-provided engines"
  );

  Assert.equal(
    (await Services.search.getDefault()).name,
    "engine1",
    "Should have used the configured default engine"
  );

  removeSettingsFile();
  ss._removeObservers();
}

/**
 * Test reading from search.json.mozlz4
 */
add_setup(async function () {
  await SearchTestUtils.useTestEngines("data1");
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_obsolete_distribution_engine() {
  await checkLoadSettingProperties(
    "data/search-obsolete-distribution.json",
    "Distribution"
  );
});

add_task(async function test_obsolete_langpack_engine() {
  await checkLoadSettingProperties(
    "data/search-obsolete-langpack.json",
    "Langpack"
  );
});

add_task(async function test_obsolete_app_engine() {
  await checkLoadSettingProperties("data/search-obsolete-app.json", "App");
});
