/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test migrating legacy add-on engines in background.
 */

"use strict";

add_setup(async function () {
  useHttpServer("opensearch");
  await AddonTestUtils.promiseStartupManager();
  await SearchTestUtils.useTestEngines("data1");

  let data = await readJSONFile(do_get_file("data/search-migration.json"));

  await promiseSaveSettingsData(data);

  await Services.search.init();

  // We need the extension installed for this test, but we do not want to
  // trigger the functions that happen on installation, so stub that out.
  // The manifest already has details of this engine.
  let oldFunc = Services.search.wrappedJSObject.addEnginesFromExtension;
  Services.search.wrappedJSObject.addEnginesFromExtension = () => {};

  // Add the add-on so add-on manager has a valid item.
  await SearchTestUtils.installSearchExtension({
    id: "simple",
    name: "simple search",
    search_url: "https://example.com/",
  });

  Services.search.wrappedJSObject.addEnginesFromExtension = oldFunc;
});

add_task(async function test_migrateLegacyEngineDifferentName() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("simple");
  Assert.ok(engine, "Should have the legacy add-on engine.");

  // Set this engine as default, the new engine should become the default
  // after migration.
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  engine = Services.search.getEngineByName("simple search");
  Assert.ok(engine, "Should have the WebExtension engine.");

  await Services.search.runBackgroundChecks();

  engine = Services.search.getEngineByName("simple");
  Assert.ok(!engine, "Should have removed the legacy add-on engine");

  engine = Services.search.getEngineByName("simple search");
  Assert.ok(engine, "Should have kept the WebExtension engine.");

  Assert.equal(
    (await Services.search.getDefault()).name,
    engine.name,
    "Should have switched to the WebExtension engine as default."
  );
});
