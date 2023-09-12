/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test is to ensure that we remove xml files from searchplugins/ in the
// profile directory when a user removes the actual engine from their profile.

add_setup(async function () {
  await SearchTestUtils.useTestEngines("data1");
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function run_test() {
  // Copy an engine to [profile]/searchplugin/
  let dir = do_get_profile().clone();
  dir.append("searchplugins");
  if (!dir.exists()) {
    dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  }
  do_get_file("data/engine.xml").copyTo(dir, "test-search-engine.xml");

  let file = dir.clone();
  file.append("test-search-engine.xml");
  Assert.ok(file.exists());

  let data = await readJSONFile(do_get_file("data/search-legacy.json"));

  // Put the filePath inside the settings file, to simulate what a pre-58 version
  // of Firefox would have done.
  for (let engine of data.engines) {
    if (engine._name == "Test search engine") {
      engine.filePath = file.path;
    }
  }

  await promiseSaveSettingsData(data);

  await Services.search.init();

  // test the engine is loaded ok.
  let engine = Services.search.getEngineByName("Test search engine");
  Assert.notEqual(engine, null, "Should have found the engine");

  // remove the engine and verify the file has been removed too.
  await Services.search.removeEngine(engine);
  Assert.ok(!file.exists(), "Should have removed the file.");
});
