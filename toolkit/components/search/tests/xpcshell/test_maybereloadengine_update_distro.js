/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SEARCH_SERVICE_TOPIC = "browser-search-service";

add_task(async function setup() {
  await useTestEngines("simple-engines");
  const defaultBranch = Services.prefs.getDefaultBranch(null);

  defaultBranch.setCharPref("distribution.id", "partner-test");
  defaultBranch.setCharPref(
    kDefaultenginenamePref,
    "data:text/plain,browser.search.defaultenginename=basic"
  );

  installDistributionEngine();

  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_maybereloadengine_update_distro() {
  await Services.search.init();

  Region._setHomeRegion("FR", true);
  await SearchTestUtils.promiseSearchNotification("engines-reloaded");

  let defaultEngine = await Services.search.getDefault();
  Assert.equal(
    defaultEngine._shortName,
    "basic",
    "Should have kept the same name"
  );
  Assert.equal(
    defaultEngine._loadPath,
    "[distribution]/searchplugins/common/basic.xml",
    "Should have kept the distribution engine"
  );
  Assert.equal(
    defaultEngine
      ._getURLOfType("text/html")
      .getSubmission("", defaultEngine, "searchbar").uri.spec,
    "http://searchtest.local/?search=",
    "Should have kept the same submission URL"
  );
});
