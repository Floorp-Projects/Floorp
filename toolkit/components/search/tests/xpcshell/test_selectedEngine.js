/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const kSelectedEnginePref = "browser.search.selectedEngine";

// Check that the default engine matches the defaultenginename pref
add_task(async function test_defaultEngine() {
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
  await installTestEngine();

  Assert.equal(Services.search.defaultEngine.name, getDefaultEngineName());
});

// Giving prefs a user value shouldn't change the selected engine.
add_task(async function test_selectedEngine() {
  let defaultEngineName = getDefaultEngineName();
  // Test the selectedEngine pref.
  Services.prefs.setCharPref(kSelectedEnginePref, kTestEngineName);

  Services.search.reset();
  await Services.search.init(true);
  Assert.equal(Services.search.defaultEngine.name, defaultEngineName);

  Services.prefs.clearUserPref(kSelectedEnginePref);

  // Test the defaultenginename pref.
  Services.prefs.setCharPref(kDefaultenginenamePref, kTestEngineName);

  Services.search.reset();
  await Services.search.init(true);
  Assert.equal(Services.search.defaultEngine.name, defaultEngineName);

  Services.prefs.clearUserPref(kDefaultenginenamePref);
});

// Setting the search engine should be persisted across restarts.
add_task(async function test_persistAcrossRestarts() {
  await installTestEngine();
  // Set the engine through the API.
  await Services.search.setDefault(Services.search.getEngineByName(kTestEngineName));
  Assert.equal(Services.search.defaultEngine.name, kTestEngineName);
  await promiseAfterCache();

  // Check that the a hash was saved.
  let metadata = await promiseGlobalMetadata();
  Assert.equal(metadata.hash.length, 44);

  // Re-init and check the engine is still the same.
  Services.search.reset();
  await Services.search.init(true);
  Assert.equal(Services.search.defaultEngine.name, kTestEngineName);

  // Cleanup (set the engine back to default).
  Services.search.resetToOriginalDefaultEngine();
  Assert.equal(Services.search.defaultEngine.name, getDefaultEngineName());
});

// An engine set without a valid hash should be ignored.
add_task(async function test_ignoreInvalidHash() {
  // Set the engine through the API.
  await Services.search.setDefault(Services.search.getEngineByName(kTestEngineName));
  Assert.equal(Services.search.defaultEngine.name, kTestEngineName);
  await promiseAfterCache();

  // Then mess with the file (make the hash invalid).
  let metadata = await promiseGlobalMetadata();
  metadata.hash = "invalid";
  await promiseSaveGlobalMetadata(metadata);

  // Re-init the search service, and check that the json file is ignored.
  Services.search.reset();
  await Services.search.init(true);
  Assert.equal(Services.search.defaultEngine.name, getDefaultEngineName());
});

// Resetting the engine to the default should remove the saved value.
add_task(async function test_settingToDefault() {
  // Set the engine through the API.
  await Services.search.setDefault(Services.search.getEngineByName(kTestEngineName));
  Assert.equal(Services.search.defaultEngine.name, kTestEngineName);
  await promiseAfterCache();

  // Check that the current engine was saved.
  let metadata = await promiseGlobalMetadata();
  Assert.equal(metadata.current, kTestEngineName);

  // Then set the engine back to the default through the API.
  await Services.search.setDefault(Services.search.getEngineByName(getDefaultEngineName()));
  await promiseAfterCache();

  // Check that the current engine is no longer saved in the JSON file.
  metadata = await promiseGlobalMetadata();
  Assert.equal(metadata.current, "");
});

add_task(async function test_resetToOriginalDefaultEngine() {
  let defaultName = getDefaultEngineName();
  Assert.equal(Services.search.defaultEngine.name, defaultName);

  await Services.search.setDefault(Services.search.getEngineByName(kTestEngineName));
  Assert.equal(Services.search.defaultEngine.name, kTestEngineName);
  await promiseAfterCache();

  Services.search.resetToOriginalDefaultEngine();
  Assert.equal(Services.search.defaultEngine.name, defaultName);
  await promiseAfterCache();
});

add_task(async function test_fallback_kept_after_restart() {
  // Set current engine to a default engine that isn't the original default.
  let builtInEngines = await Services.search.getDefaultEngines();
  let defaultName = getDefaultEngineName();
  let nonDefaultBuiltInEngine;
  for (let engine of builtInEngines) {
    if (engine.name != defaultName) {
      nonDefaultBuiltInEngine = engine;
      break;
    }
  }
  await Services.search.setDefault(nonDefaultBuiltInEngine);
  Assert.equal(Services.search.defaultEngine.name, nonDefaultBuiltInEngine.name);
  await promiseAfterCache();

  // Remove that engine...
  await Services.search.removeEngine(nonDefaultBuiltInEngine);
  // The engine being a default (built-in) one, it should be hidden
  // rather than actually removed.
  Assert.ok(nonDefaultBuiltInEngine.hidden);

  // Using the defaultEngine getter should force a fallback to the
  // original default engine.
  Assert.equal(Services.search.defaultEngine.name, defaultName);

  // Restoring the default engines should unhide our built-in test
  // engine, but not change the value of defaultEngine.
  Services.search.restoreDefaultEngines();
  Assert.ok(!nonDefaultBuiltInEngine.hidden);
  Assert.equal(Services.search.defaultEngine.name, defaultName);
  await promiseAfterCache();

  // After a restart, the defaultEngine value should still be unchanged.
  Services.search.reset();
  await Services.search.init(true);
  Assert.equal(Services.search.defaultEngine.name, defaultName);
});


function run_test() {
  Assert.ok(!Services.search.isInitialized);

  let engineDummyFile = do_get_profile().clone();
  engineDummyFile.append("searchplugins");
  engineDummyFile.append("test-search-engine.xml");
  let engineDir = engineDummyFile.parent;
  engineDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file("data/engine.xml").copyTo(engineDir, "engine.xml");

  run_next_test();
}
