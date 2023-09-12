/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from the search settings.
 */

"use strict";

var { getAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

var settingsTemplate;

/**
 * Test reading from search.json.mozlz4
 */
add_setup(async function () {
  await AddonTestUtils.promiseStartupManager();

  await setupRemoteSettings();

  settingsTemplate = await readJSONFile(
    do_get_file("data/search_ignorelist.json")
  );
  settingsTemplate.buildID = getAppInfo().platformBuildID;

  await promiseSaveSettingsData(settingsTemplate);
});

/**
 * Start the search service and confirm the settings were reset
 */
add_task(async function test_settings_rest() {
  info("init search service");

  let updatePromise = SearchTestUtils.promiseSearchNotification(
    "settings-update-complete"
  );

  let result = await Services.search.init();

  Assert.ok(
    Components.isSuccessCode(result),
    "Search service should be successfully initialized"
  );
  await updatePromise;

  const engines = await Services.search.getEngines();

  // Engine list will have been reset to the default,
  // Not the one engine in the settings.
  // It should have more than one engine.
  Assert.greater(
    engines.length,
    1,
    "Should have more than one engine in the list"
  );

  removeSettingsFile();
});
