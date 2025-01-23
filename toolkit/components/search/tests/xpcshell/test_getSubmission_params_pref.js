/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that MozParam condition="pref" values used in search URLs are from the
 * default branch, and that their special characters are URL encoded. */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const defaultBranch = Services.prefs.getDefaultBranch(
  SearchUtils.BROWSER_SEARCH_PREF
);
const baseURL = "https://www.google.com/search?q=foo";

add_setup(async function () {
  // The test engines used in this test need to be recognized as 'default'
  // engines, or their MozParams will be ignored.
  await SearchTestUtils.useTestEngines("enterprise");
});

add_task(async function test_pref_initial_value() {
  defaultBranch.setCharPref("param.code", "good&id=unique");

  // Preference params are only allowed to be modified on the user branch
  // on nightly builds. For non-nightly builds, check that modifying on the
  // normal branch doesn't work.
  if (!AppConstants.NIGHTLY_BUILD) {
    Services.prefs.setCharPref(
      SearchUtils.BROWSER_SEARCH_PREF + "param.code",
      "bad"
    );
  }

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  const engine = Services.search.getEngineByName("engine-pref");
  let expectedCode =
    SearchUtils.MODIFIED_APP_CHANNEL == "esr" ? "enterprise" : "good";
  let searchParams = new URL(engine.getSubmission("foo").uri.spec).searchParams;
  Assert.equal(
    searchParams.get("code"),
    expectedCode,
    "Should have the correct code in the submissionURL"
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
  let expectedCode =
    SearchUtils.MODIFIED_APP_CHANNEL == "esr"
      ? "enterprise"
      : "supergood%26id%3Dunique123456";
  let searchParams = new URL(engine.getSubmission("foo").uri.spec).searchParams;

  Assert.equal(
    searchParams.get("code"),
    expectedCode,
    "Should have the correct code in the submissionURL"
  );
});

add_task(async function test_pref_cleared() {
  // Update the pref without re-init nor restart.
  // Note you can't delete a preference from the default branch.
  defaultBranch.setCharPref("param.code", "");

  let engine = Services.search.getEngineByName("engine-pref");
  // ESR always has an enterprise code
  if (SearchUtils.MODIFIED_APP_CHANNEL != "esr") {
    Assert.equal(
      engine.getSubmission("foo").uri.spec,
      baseURL,
      "Should have just the base URL after the pref was cleared"
    );
  }
});

add_task(async function test_pref_updated_enterprise() {
  // Set the pref to some value and enable enterprise mode at the same time.
  defaultBranch.setCharPref("param.code", "supergood&id=unique123456");
  await enableEnterprise();

  const engine = Services.search.getEngineByName("engine-pref");
  let searchParams = new URL(engine.getSubmission("foo").uri.spec).searchParams;
  Assert.equal(
    searchParams.get("code"),
    "enterprise",
    "Should have the correct code in the submissionURL"
  );
});
