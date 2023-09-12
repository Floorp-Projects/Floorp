/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * test_nosettings: Start search engine
 * - without search.json.mozlz4
 *
 * Ensure that :
 * - nothing explodes;
 * - search.json.mozlz4 is created.
 */

add_setup(async function () {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_nosettings() {
  let search = Services.search;

  let afterSettingsPromise = promiseAfterSettings();

  await search.init();

  // Check that the settings is created at startup
  await afterSettingsPromise;

  // Check that search.json.mozlz4 has been created.
  let settingsFile = do_get_profile().clone();
  settingsFile.append(SETTINGS_FILENAME);
  Assert.ok(settingsFile.exists());

  await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}engine.xml`,
  });

  info("Engine has been added, let's wait for the settings to be built");
  await promiseAfterSettings();

  info("Searching test engine in settings");
  let settings = await promiseSettingsData();
  let found = false;
  for (let engine of settings.engines) {
    if (engine._name == "Test search engine") {
      found = true;
      break;
    }
  }
  Assert.ok(found);
});
