/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that the ignoredJAREngines pref hides search engines  */

"use strict";

add_task(async function setup() {
  let defaultBranch = Services.prefs.getDefaultBranch(
    SearchUtils.BROWSER_SEARCH_PREF
  );
  defaultBranch.setCharPref("ignoredJAREngines", "engine");
  Services.prefs
    .getDefaultBranch("")
    .setCharPref("distribution.id", "partner-1");

  // The test engines used in this test need to be recognized as 'default'
  // engines or the resource URL won't be used
  await useTestEngines();

  Assert.ok(!Services.search.isInitialized);
});

add_task(async function test_disthidden() {
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  Assert.ok(Services.search.isInitialized);

  let engines = await Services.search.getEngines();
  // From data/list.json - only 5 out of 6
  // since one is hidden
  Assert.equal(engines.length, 5);

  // Verify that the Test search engine is not available
  let engine = Services.search.getEngineByName("Test search engine");
  Assert.equal(engine, null);
});
