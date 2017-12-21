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
  Assert.notEqual(engine, null);

  // The test engine has been found in the profile directory and imported,
  // so it shouldn't have a loadPathHash.
  let metadata = await promiseEngineMetadata();
  Assert.ok(kTestEngineShortName in metadata);
  Assert.equal(false, "loadPathHash" in metadata[kTestEngineShortName]);

  // After making it the currentEngine with the search service API,
  // the test engine should have a valid loadPathHash.
  Services.search.currentEngine = engine;
  await promiseAfterCache();
  metadata = await promiseEngineMetadata();
  Assert.ok("loadPathHash" in metadata[kTestEngineShortName]);
  let loadPathHash = metadata[kTestEngineShortName].loadPathHash;
  Assert.equal(typeof loadPathHash, "string");
  Assert.equal(loadPathHash.length, 44);

  // A search should not cause the search reset prompt.
  let submission =
    Services.search.currentEngine.getSubmission("foo", null, "searchbar");
  Assert.equal(submission.uri.spec,
               "http://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t");
});

add_task(async function test_pending() {
  let checkWithPrefValue = (value, expectPrompt = false) => {
    Services.prefs.setCharPref(BROWSER_SEARCH_PREF + "reset.status", value);
    let submission =
      Services.search.currentEngine.getSubmission("foo", null, "searchbar");
    Assert.equal(submission.uri.spec,
                 expectPrompt ? "about:searchreset?data=foo&purpose=searchbar" :
                   "http://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t");
  };

  // Should show the reset prompt only if the reset status is 'pending'.
  checkWithPrefValue("pending", true);
  checkWithPrefValue("accepted");
  checkWithPrefValue("declined");
  checkWithPrefValue("customized");
  checkWithPrefValue("");
});

add_task(async function test_promptURLs() {
  await removeLoadPathHash();

  // The default should still be the test engine.
  let currentEngine = Services.search.currentEngine;
  Assert.equal(currentEngine.name, kTestEngineName);
  // but the submission url should be about:searchreset
  let url = (data, purpose) =>
    currentEngine.getSubmission(data, null, purpose).uri.spec;
  Assert.equal(url("foo", "searchbar"),
               "about:searchreset?data=foo&purpose=searchbar");
  Assert.equal(url("foo"), "about:searchreset?data=foo");
  Assert.equal(url("", "searchbar"), "about:searchreset?purpose=searchbar");
  Assert.equal(url(""), "about:searchreset");
  Assert.equal(url("", ""), "about:searchreset");

  // Calling the currentEngine setter for the same engine should
  // prevent further prompts.
  Services.search.currentEngine = Services.search.currentEngine;
  Assert.equal(url("foo", "searchbar"),
               "http://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t");

  // And the loadPathHash should be back.
  await promiseAfterCache();
  let metadata = await promiseEngineMetadata();
  Assert.ok("loadPathHash" in metadata[kTestEngineShortName]);
  let loadPathHash = metadata[kTestEngineShortName].loadPathHash;
  Assert.equal(typeof loadPathHash, "string");
  Assert.equal(loadPathHash.length, 44);
});

add_task(async function test_whitelist() {
  await removeLoadPathHash();

  // The default should still be the test engine.
  let currentEngine = Services.search.currentEngine;
  Assert.equal(currentEngine.name, kTestEngineName);
  let expectPrompt = shouldPrompt => {
    let expectedURL =
      shouldPrompt ? "about:searchreset?data=foo&purpose=searchbar"
                   : "http://www.google.com/search?q=foo&ie=utf-8&oe=utf-8&aq=t";
    let url = currentEngine.getSubmission("foo", null, "searchbar").uri.spec;
    Assert.equal(url, expectedURL);
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
  Assert.equal(false, "loadPathHash" in metadata[kTestEngineShortName]);

  branch.setCharPref(kWhiteListPrefName, initialWhiteList);
  expectPrompt(true);
});
