/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/osfile.jsm");

const kSelectedEnginePref = "browser.search.selectedEngine";

// waitForSearchNotification is in head_search.js
let waitForNotification = waitForSearchNotification;

// Check that the default engine matches the defaultenginename pref
add_task(function* test_defaultEngine() {
  yield asyncInit();

  do_check_eq(Services.search.currentEngine.name, getDefaultEngineName());
});

// Giving prefs a user value shouldn't change the selected engine.
add_task(function* test_selectedEngine() {
  let defaultEngineName = getDefaultEngineName();
  // Test the selectedEngine pref.
  Services.prefs.setCharPref(kSelectedEnginePref, kTestEngineName);

  yield asyncReInit();
  do_check_eq(Services.search.currentEngine.name, defaultEngineName);

  Services.prefs.clearUserPref(kSelectedEnginePref);

  // Test the defaultenginename pref.
  Services.prefs.setCharPref(kDefaultenginenamePref, kTestEngineName);

  yield asyncReInit();
  do_check_eq(Services.search.currentEngine.name, defaultEngineName);

  Services.prefs.clearUserPref(kDefaultenginenamePref);
});

// Setting the search engine should be persisted across restarts.
add_task(function* test_persistAcrossRestarts() {
  // Set the engine through the API.
  Services.search.currentEngine = Services.search.getEngineByName(kTestEngineName);
  do_check_eq(Services.search.currentEngine.name, kTestEngineName);
  yield waitForNotification("write-metadata-to-disk-complete");

  // Check that the a hash was saved.
  let path = OS.Path.join(OS.Constants.Path.profileDir, "search-metadata.json");
  let bytes = yield OS.File.read(path);
  let json = JSON.parse(new TextDecoder().decode(bytes));
  do_check_eq(json["[global]"].hash.length, 44);

  // Re-init and check the engine is still the same.
  yield asyncReInit();
  do_check_eq(Services.search.currentEngine.name, kTestEngineName);

  // Cleanup (set the engine back to default).
  Services.search.currentEngine = Services.search.defaultEngine;
  // This check is no longer valid with bug 1102416's patch - defaultEngine
  // is not based on the same value as _originalDefaultEngine in non-Firefox
  // users of the search service.
  //do_check_eq(Services.search.currentEngine.name, getDefaultEngineName());
});

// An engine set without a valid hash should be ignored.
add_task(function* test_ignoreInvalidHash() {
  // Set the engine through the API.
  Services.search.currentEngine = Services.search.getEngineByName(kTestEngineName);
  do_check_eq(Services.search.currentEngine.name, kTestEngineName);
  yield waitForNotification("write-metadata-to-disk-complete");

  // Then mess with the file.
  let path = OS.Path.join(OS.Constants.Path.profileDir, "search-metadata.json");
  let bytes = yield OS.File.read(path);
  let json = JSON.parse(new TextDecoder().decode(bytes));

  // Make the hask invalid.
  json["[global]"].hash = "invalid";

  let data = new TextEncoder().encode(JSON.stringify(json));
  yield OS.File.writeAtomic(path, data);//, { tmpPath: path + ".tmp" });

  // Re-init the search service, and check that the json file is ignored.
  yield asyncReInit();
  do_check_eq(Services.search.currentEngine.name, getDefaultEngineName());
});

// Resetting the engine to the default should remove the saved value.
add_task(function* test_settingToDefault() {
  // Set the engine through the API.
  Services.search.currentEngine = Services.search.getEngineByName(kTestEngineName);
  do_check_eq(Services.search.currentEngine.name, kTestEngineName);
  yield waitForNotification("write-metadata-to-disk-complete");

  // Check that the current engine was saved.
  let path = OS.Path.join(OS.Constants.Path.profileDir, "search-metadata.json");
  let bytes = yield OS.File.read(path);
  let json = JSON.parse(new TextDecoder().decode(bytes));
  do_check_eq(json["[global]"].current, kTestEngineName);

  // Then set the engine back to the default through the API.
  Services.search.currentEngine =
    Services.search.getEngineByName(getDefaultEngineName());
  yield waitForNotification("write-metadata-to-disk-complete");

  // Check that the current engine is no longer saved in the JSON file.
  bytes = yield OS.File.read(path);
  json = JSON.parse(new TextDecoder().decode(bytes));
  do_check_eq(json["[global]"].current, "");
});


function run_test() {
  removeMetadata();
  removeCacheFile();

  do_check_false(Services.search.isInitialized);

  let engineDummyFile = gProfD.clone();
  engineDummyFile.append("searchplugins");
  engineDummyFile.append("test-search-engine.xml");
  let engineDir = engineDummyFile.parent;
  engineDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file("data/engine.xml").copyTo(engineDir, "engine.xml");

  do_register_cleanup(function() {
    removeMetadata();
    removeCacheFile();
  });

  run_next_test();
}
