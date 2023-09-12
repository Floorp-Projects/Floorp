/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { promiseShutdownManager, promiseStartupManager } = AddonTestUtils;

add_setup(async function () {
  await SearchTestUtils.useTestEngines("data1");
  await promiseStartupManager();
  await Services.search.init();
  await promiseAfterSettings();

  registerCleanupFunction(promiseShutdownManager);
});

add_task(async function test_basic_upgrade() {
  let extension = await SearchTestUtils.installSearchExtension(
    {
      version: "1.0",
      search_url_get_params: `q={searchTerms}&version=1.0`,
      keyword: "foo",
    },
    { skipUnload: true }
  );

  let engine = await Services.search.getEngineByAlias("foo");
  Assert.ok(engine, "Can fetch engine with alias");
  engine.alias = "testing";

  engine = await Services.search.getEngineByAlias("testing");
  Assert.ok(engine, "Can fetch engine by alias");
  let params = engine.getSubmission("test").uri.query.split("&");
  Assert.ok(params.includes("version=1.0"), "Correct version installed");

  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  let promiseChanged = TestUtils.topicObserved(
    "browser-search-engine-modified",
    (eng, verb) => verb == "engine-changed"
  );

  let manifest = SearchTestUtils.createEngineManifest({
    version: "2.0",
    search_url_get_params: `q={searchTerms}&version=2.0`,
    keyword: "bar",
  });
  await extension.upgrade({
    useAddonManager: "permanent",
    manifest,
  });
  await AddonTestUtils.waitForSearchProviderStartup(extension);
  await promiseChanged;

  engine = await Services.search.getEngineByAlias("testing");
  Assert.ok(engine, "Engine still has alias set");

  params = engine.getSubmission("test").uri.query.split("&");
  Assert.ok(params.includes("version=2.0"), "Correct version installed");

  Assert.equal(
    Services.search.defaultEngine.name,
    "Example",
    "Should have retained the same default engine"
  );

  await extension.unload();
  await promiseAfterSettings();
});

add_task(async function test_upgrade_changes_name() {
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "engine",
      id: "engine@tests.mozilla.org",
      search_url_get_params: `q={searchTerms}&version=1.0`,
      version: "1.0",
    },
    { skipUnload: true }
  );

  let engine = Services.search.getEngineByName("engine");
  Assert.ok(!!engine, "Should have loaded the engine");

  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  // When we add engines currently, we normally force using the saved order.
  // Reset that here, so we can check the order is reset in the case this
  // is a application provided engine change.
  Services.search.wrappedJSObject._settings.setMetaDataAttribute(
    "useSavedOrder",
    false
  );
  Services.search.getEngineByName("engine1").wrappedJSObject._orderHint = null;
  Services.search.getEngineByName("engine2").wrappedJSObject._orderHint = null;

  Assert.deepEqual(
    (await Services.search.getVisibleEngines()).map(e => e.name),
    ["engine1", "engine2", "engine"],
    "Should have the expected order initially"
  );

  let promiseChanged = TestUtils.topicObserved(
    "browser-search-engine-modified",
    (eng, verb) => verb == "engine-changed"
  );

  let manifest = SearchTestUtils.createEngineManifest({
    name: "Bar",
    id: "engine@tests.mozilla.org",
    search_url_get_params: `q={searchTerms}&version=2.0`,
    version: "2.0",
  });
  await extension.upgrade({
    useAddonManager: "permanent",
    manifest,
  });
  await AddonTestUtils.waitForSearchProviderStartup(extension);

  await promiseChanged;

  engine = Services.search.getEngineByName("Bar");
  Assert.ok(!!engine, "Should be able to get the new engine");

  Assert.equal(
    (await Services.search.getDefault()).name,
    "Bar",
    "Should have kept the default engine the same"
  );

  Assert.deepEqual(
    (await Services.search.getVisibleEngines()).map(e => e.name),
    // Expected order: Default, then others in alphabetical.
    ["engine1", "Bar", "engine2"],
    "Should have updated the engine order"
  );

  await extension.unload();
  await promiseAfterSettings();
});

add_task(async function test_upgrade_to_existing_name_not_allowed() {
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "engine",
      search_url_get_params: `q={searchTerms}&version=1.0`,
      version: "1.0",
    },
    { skipUnload: true }
  );

  let engine = Services.search.getEngineByName("engine");
  Assert.ok(!!engine, "Should have loaded the engine");

  let promise = AddonTestUtils.waitForSearchProviderStartup(extension);
  let name = "engine1";
  consoleAllowList.push(`An engine called ${name} already exists`);
  let manifest = SearchTestUtils.createEngineManifest({
    name,
    search_url_get_params: `q={searchTerms}&version=2.0`,
    version: "2.0",
  });
  await extension.upgrade({
    useAddonManager: "permanent",
    manifest,
  });
  await promise;

  Assert.equal(
    Services.search.getEngineByName("engine1").getSubmission("").uri.spec,
    "https://1.example.com/",
    "Should have not changed the original engine"
  );

  console.log((await Services.search.getEngines()).map(e => e.name));

  engine = Services.search.getEngineByName("engine");
  Assert.ok(!!engine, "Should still be able to get the engine by the old name");

  await extension.unload();
  await promiseAfterSettings();
});
