/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kExtensionID = "simple@tests.mozilla.org";

SearchTestUtils.initXPCShellAddonManager(this);

add_task(async function setup() {
  useHttpServer("opensearch");
  await AddonTestUtils.promiseStartupManager();
  await SearchTestUtils.useTestEngines("data1");
  await Services.search.init();
});

add_task(async function test_migrateLegacyEngine() {
  await Services.search.addOpenSearchEngine(gDataUrl + "simple.xml", null);

  // Modify the loadpath so it looks like a legacy plugin loadpath
  let engine = Services.search.getEngineByName("simple");
  engine.wrappedJSObject._loadPath = `jar:[profile]/extensions/${kExtensionID}.xpi!/simple.xml`;
  engine.wrappedJSObject._extensionID = null;

  await Services.search.setDefault(engine);

  // This should replace the existing engine
  let extension = await SearchTestUtils.installSearchExtension({
    id: "simple",
    name: "simple",
    search_url: "https://example.com/",
  });

  engine = Services.search.getEngineByName("simple");
  Assert.equal(
    engine.wrappedJSObject._loadPath,
    "[other]addEngineWithDetails:" + kExtensionID
  );
  Assert.equal(engine.wrappedJSObject._extensionID, kExtensionID);

  Assert.equal(
    (await Services.search.getDefault()).name,
    "simple",
    "Should have kept the default engine the same"
  );

  await extension.unload();
});
