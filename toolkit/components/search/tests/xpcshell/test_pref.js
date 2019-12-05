/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that MozParam condition="pref" values used in search URLs are from the
 * default branch, and that their special characters are URL encoded. */

"use strict";

const defaultBranch = Services.prefs.getDefaultBranch(
  SearchUtils.BROWSER_SEARCH_PREF
);
const baseURL = "https://www.google.com/search?q=foo";

add_task(async function setup() {
  // The test engines used in this test need to be recognized as 'default'
  // engines, or their MozParams will be ignored.
  await useTestEngines();
});

add_task(async function test_pref_initial_value() {
  defaultBranch.setCharPref("param.code", "good&id=unique");
  Services.prefs.setCharPref(
    SearchUtils.BROWSER_SEARCH_PREF + "param.code",
    "bad"
  );

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  const engine = Services.search.getEngineByName("engine-pref");
  const base = baseURL + "&code=";
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    base + "good%26id%3Dunique",
    "Should have got the submission URL with the correct code"
  );

  // Now clear the user-set preference. Having a user set preference means
  // we don't get updates from the pref service of changes on the default
  // branch. Normally, this won't be an issue, since we don't expect users
  // to be playing with these prefs, and worst-case, they'll just get the
  // actual change on restart.
  Services.prefs.clearUserPref(SearchUtils.BROWSER_SEARCH_PREF + "param.code");
});

add_task(async function test_pref_updated() {
  // Update the pref without re-init nor restart.
  defaultBranch.setCharPref("param.code", "supergood&id=unique123456");

  const engine = Services.search.getEngineByName("engine-pref");
  const base = baseURL + "&code=";
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    base + "supergood%26id%3Dunique123456",
    "Should have got the submission URL with the updated code"
  );
});

add_task(async function test_pref_cleared() {
  // Update the pref without re-init nor restart.
  // Note you can't delete a preference from the default branch.
  defaultBranch.setCharPref("param.code", "");

  let engine = Services.search.getEngineByName("engine-pref");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL,
    "Should have just the base URL after the pref was cleared"
  );
});
