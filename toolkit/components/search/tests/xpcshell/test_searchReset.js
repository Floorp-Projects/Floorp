/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const kTestEngineShortName = "test-search-engine";
const kWhiteListPrefName = "reset.whitelist";

add_task(async function run_test() {
  Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF)
          .setBoolPref("reset.enabled", true);

  await asyncInit();
  await promiseAfterCache();

  // Install kTestEngineName and wait for it to reach the disk.
  await installTestEngine();
  await promiseAfterCache();

  // Simulate an engine found in the profile directory and imported
  // by a previous version of Firefox.
  await removeLoadPathHash();
});

async function removeLoadPathHash() {
  // Remove the loadPathHash and re-initialize the search service.
  let cache = await promiseCacheData();
  for (let engine of cache.engines) {
    if (engine._shortName == kTestEngineShortName) {
      delete engine._metaData.loadPathHash;
      break;
    }
  }
  await promiseSaveCacheData(cache);
  await asyncReInit();
}

add_task(async function test_no_prompt_when_valid_loadPathHash() {
  // test the engine is loaded ok.
  let engine = Services.search.getEngineByName(kTestEngineName);
  do_check_neq(engine, null);

  // The test engine has been found in the profile directory and imported,
  // so it shouldn't have a loadPathHash.
  let metadata = await promiseEngineMetadata();
  do_check_true(kTestEngineShortName in metadata);
  do_check_false("loadPathHash" in metadata[kTestEngineShortName]);

  // After making it the currentEngine with the search service API,
  // the test engine should have a valid loadPathHash.
  Services.search.currentEngine = engine;
  await promiseAfterCache();
  metadata = await promiseEngineMetadata();
  do_check_true("loadPathHash" in metadata[kTestEngineShortName]);
  let loadPathHash = metadata[kTestEngineShortName].loadPathHash;
  do_check_eq(typeof loadPathHash, "string");
  do_check_eq(loadPathHash.length, 44);

  // A search should not cause the search reset prompt.
  let submission =
    Services.search.currentEngine.getSubmission("foo", null, "searchbar");
  do_check_eq(submission.uri.spec,
              "http://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t");
});

add_task(async function test_promptURLs() {
  await removeLoadPathHash();

  // The default should still be the test engine.
  let currentEngine = Services.search.currentEngine;
  do_check_eq(currentEngine.name, kTestEngineName);
  // but the submission url should be about:searchreset
  let url = (data, purpose) =>
    currentEngine.getSubmission(data, null, purpose).uri.spec;
  do_check_eq(url("foo", "searchbar"),
              "about:searchreset?data=foo&purpose=searchbar");
  do_check_eq(url("foo"), "about:searchreset?data=foo");
  do_check_eq(url("", "searchbar"), "about:searchreset?purpose=searchbar");
  do_check_eq(url(""), "about:searchreset");
  do_check_eq(url("", ""), "about:searchreset");

  // Calling the currentEngine setter for the same engine should
  // prevent further prompts.
  Services.search.currentEngine = Services.search.currentEngine;
  do_check_eq(url("foo", "searchbar"),
              "http://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t");

  // And the loadPathHash should be back.
  await promiseAfterCache();
  let metadata = await promiseEngineMetadata();
  do_check_true("loadPathHash" in metadata[kTestEngineShortName]);
  let loadPathHash = metadata[kTestEngineShortName].loadPathHash;
  do_check_eq(typeof loadPathHash, "string");
  do_check_eq(loadPathHash.length, 44);
});

add_task(async function test_whitelist() {
  await removeLoadPathHash();

  // The default should still be the test engine.
  let currentEngine = Services.search.currentEngine;
  do_check_eq(currentEngine.name, kTestEngineName);
  let expectPrompt = shouldPrompt => {
    let expectedURL =
      shouldPrompt ? "about:searchreset?data=foo&purpose=searchbar"
                   : "http://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t";
    let url = currentEngine.getSubmission("foo", null, "searchbar").uri.spec;
    do_check_eq(url, expectedURL);
  };
  expectPrompt(true);

  // Unless we whitelist our test engine.
  let branch = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF);
  let initialWhiteList = branch.getCharPref(kWhiteListPrefName);
  branch.setCharPref(kWhiteListPrefName, "example.com,test.tld");
  expectPrompt(true);
  branch.setCharPref(kWhiteListPrefName, "www.google.com");
  expectPrompt(false);
  branch.setCharPref(kWhiteListPrefName, "example.com,www.google.com,test.tld");
  expectPrompt(false);

  // The loadPathHash should not be back after the prompt was skipped due to the
  // whitelist.
  await asyncReInit();
  let metadata = await promiseEngineMetadata();
  do_check_false("loadPathHash" in metadata[kTestEngineShortName]);

  branch.setCharPref(kWhiteListPrefName, initialWhiteList);
  expectPrompt(true);
});
