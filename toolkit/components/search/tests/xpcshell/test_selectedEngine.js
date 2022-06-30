/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const kDefaultEngineName = "engine1";

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
  await SearchTestUtils.useTestEngines("data1");
  Assert.ok(!Services.search.isInitialized);
  Services.prefs.setBoolPref(
    "browser.search.removeEngineInfobar.enabled",
    false
  );
});

// Check that the default engine matches the defaultenginename pref
add_task(async function test_defaultEngine() {
  await Services.search.init();
  await SearchTestUtils.promiseNewSearchEngine(`${gDataUrl}engine.xml`);

  Assert.equal(Services.search.defaultEngine.name, kDefaultEngineName);
});

// Setting the search engine should be persisted across restarts.
add_task(async function test_persistAcrossRestarts() {
  // Set the engine through the API.
  await Services.search.setDefault(
    Services.search.getEngineByName(kTestEngineName)
  );
  Assert.equal(Services.search.defaultEngine.name, kTestEngineName);
  await promiseAfterSettings();

  // Check that the a hash was saved.
  let metadata = await promiseGlobalMetadata();
  Assert.equal(metadata.hash.length, 44);

  // Re-init and check the engine is still the same.
  Services.search.wrappedJSObject.reset();
  await Services.search.init(true);
  Assert.equal(Services.search.defaultEngine.name, kTestEngineName);

  // Cleanup (set the engine back to default).
  Services.search.resetToAppDefaultEngine();
  Assert.equal(Services.search.defaultEngine.name, kDefaultEngineName);
});

// An engine set without a valid hash should be ignored.
add_task(async function test_ignoreInvalidHash() {
  // Set the engine through the API.
  await Services.search.setDefault(
    Services.search.getEngineByName(kTestEngineName)
  );
  Assert.equal(Services.search.defaultEngine.name, kTestEngineName);
  await promiseAfterSettings();

  // Then mess with the file (make the hash invalid).
  let metadata = await promiseGlobalMetadata();
  metadata.hash = "invalid";
  await promiseSaveGlobalMetadata(metadata);

  // Re-init the search service, and check that the json file is ignored.
  Services.search.wrappedJSObject.reset();
  await Services.search.init(true);
  Assert.equal(Services.search.defaultEngine.name, kDefaultEngineName);
});

// Resetting the engine to the default should remove the saved value.
add_task(async function test_settingToDefault() {
  // Set the engine through the API.
  await Services.search.setDefault(
    Services.search.getEngineByName(kTestEngineName)
  );
  Assert.equal(Services.search.defaultEngine.name, kTestEngineName);
  await promiseAfterSettings();

  // Check that the current engine was saved.
  let metadata = await promiseGlobalMetadata();
  Assert.equal(metadata.current, kTestEngineName);

  // Then set the engine back to the default through the API.
  await Services.search.setDefault(
    Services.search.getEngineByName(kDefaultEngineName)
  );
  await promiseAfterSettings();

  // Check that the current engine is no longer saved in the JSON file.
  metadata = await promiseGlobalMetadata();
  Assert.equal(metadata.current, "");
});

add_task(async function test_resetToOriginalDefaultEngine() {
  Assert.equal(Services.search.defaultEngine.name, kDefaultEngineName);

  await Services.search.setDefault(
    Services.search.getEngineByName(kTestEngineName)
  );
  Assert.equal(Services.search.defaultEngine.name, kTestEngineName);
  await promiseAfterSettings();

  Services.search.resetToAppDefaultEngine();
  Assert.equal(Services.search.defaultEngine.name, kDefaultEngineName);
  await promiseAfterSettings();
});

add_task(async function test_fallback_kept_after_restart() {
  // Set current engine to a default engine that isn't the original default.
  let builtInEngines = await Services.search.getAppProvidedEngines();
  let nonDefaultBuiltInEngine;
  for (let engine of builtInEngines) {
    if (engine.name != kDefaultEngineName) {
      nonDefaultBuiltInEngine = engine;
      break;
    }
  }
  await Services.search.setDefault(nonDefaultBuiltInEngine);
  Assert.equal(
    Services.search.defaultEngine.name,
    nonDefaultBuiltInEngine.name
  );
  await promiseAfterSettings();

  // Remove that engine...
  await Services.search.removeEngine(nonDefaultBuiltInEngine);
  // The engine being a default (built-in) one, it should be hidden
  // rather than actually removed.
  Assert.ok(nonDefaultBuiltInEngine.hidden);

  // Using the defaultEngine getter should force a fallback to the
  // original default engine.
  Assert.equal(Services.search.defaultEngine.name, kDefaultEngineName);

  // Restoring the default engines should unhide our built-in test
  // engine, but not change the value of defaultEngine.
  Services.search.restoreDefaultEngines();
  Assert.ok(!nonDefaultBuiltInEngine.hidden);
  Assert.equal(Services.search.defaultEngine.name, kDefaultEngineName);
  await promiseAfterSettings();

  // After a restart, the defaultEngine value should still be unchanged.
  Services.search.wrappedJSObject.reset();
  await Services.search.init(true);
  Assert.equal(Services.search.defaultEngine.name, kDefaultEngineName);
});
