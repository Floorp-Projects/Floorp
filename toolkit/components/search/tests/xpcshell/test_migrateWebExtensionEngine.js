/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kExtensionID = "simple@tests.mozilla.org";

add_setup(async function () {
  useHttpServer("opensearch");
  await AddonTestUtils.promiseStartupManager();
  await SearchTestUtils.useTestEngines("data1");
  await Services.search.init();
});

add_task(async function test_migrateLegacyEngine() {
  let engine = await SearchTestUtils.promiseNewSearchEngine({
    url: gDataUrl + "simple.xml",
  });

  // Modify the loadpath so it looks like a legacy plugin loadpath
  engine.wrappedJSObject._loadPath = `jar:[profile]/extensions/${kExtensionID}.xpi!/simple.xml`;
  engine.wrappedJSObject._extensionID = null;

  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  // This should replace the existing engine
  let extension = await SearchTestUtils.installSearchExtension(
    {
      id: "simple",
      name: "simple",
      search_url: "https://example.com/",
    },
    { skipUnload: true }
  );

  engine = Services.search.getEngineByName("simple");
  Assert.equal(engine.wrappedJSObject._loadPath, "[addon]" + kExtensionID);
  Assert.equal(engine.wrappedJSObject._extensionID, kExtensionID);

  Assert.equal(
    (await Services.search.getDefault()).name,
    "simple",
    "Should have kept the default engine the same"
  );

  await extension.unload();
});

add_task(async function test_migrateLegacyEngineDifferentName() {
  let engine = await SearchTestUtils.promiseNewSearchEngine({
    url: gDataUrl + "simple.xml",
  });

  // Modify the loadpath so it looks like an legacy plugin loadpath
  engine.wrappedJSObject._loadPath = `jar:[profile]/extensions/${kExtensionID}.xpi!/simple.xml`;
  engine.wrappedJSObject._extensionID = null;

  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  // This should replace the existing engine - it has the same id, but a different name.
  let extension = await SearchTestUtils.installSearchExtension(
    {
      id: "simple",
      name: "simple search",
      search_url: "https://example.com/",
    },
    { skipUnload: true }
  );

  engine = Services.search.getEngineByName("simple");
  Assert.equal(engine, null, "Should have removed the old engine");

  // The engine should have changed its name.
  engine = Services.search.getEngineByName("simple search");
  Assert.equal(engine.wrappedJSObject._loadPath, "[addon]" + kExtensionID);
  Assert.equal(engine.wrappedJSObject._extensionID, kExtensionID);

  Assert.equal(
    (await Services.search.getDefault()).name,
    "simple search",
    "Should have made the new engine default"
  );

  await extension.unload();
});
