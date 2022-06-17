/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kExtensionID = "simple@tests.mozilla.org";

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
  let extension = await SearchTestUtils.installSearchExtension(
    {
      id: "simple",
      name: "simple",
      search_url: "https://example.com/",
    },
    true
  );

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

add_task(async function test_migrateLegacyEngineDifferentName() {
  await Services.search.addOpenSearchEngine(gDataUrl + "simple.xml", null);

  // Modify the loadpath so it looks like an legacy plugin loadpath
  let engine = Services.search.getEngineByName("simple");
  engine.wrappedJSObject._loadPath = `jar:[profile]/extensions/${kExtensionID}.xpi!/simple.xml`;
  engine.wrappedJSObject._extensionID = null;

  await Services.search.setDefault(engine);

  // This should replace the existing engine - it has the same id, but a different name.
  let extension = await SearchTestUtils.installSearchExtension(
    {
      id: "simple",
      name: "simple search",
      search_url: "https://example.com/",
    },
    true
  );

  engine = Services.search.getEngineByName("simple");
  Assert.equal(engine, null, "Should have removed the old engine");

  // The engine should have changed its name.
  engine = Services.search.getEngineByName("simple search");
  Assert.equal(
    engine.wrappedJSObject._loadPath,
    "[other]addEngineWithDetails:" + kExtensionID
  );
  Assert.equal(engine.wrappedJSObject._extensionID, kExtensionID);

  Assert.equal(
    (await Services.search.getDefault()).name,
    "simple search",
    "Should have made the new engine default"
  );

  await extension.unload();
});

add_task(async function test_migrateLegacySkipsPolicyAndUser() {
  const loadPath = "jar:[profile]/extensions/set-via-policy/simple.xml";
  await Services.search.addOpenSearchEngine(gDataUrl + "simple.xml", null);

  // Modify the loadpath so it looks like an legacy plugin loadpath
  let engine = Services.search.getEngineByName("simple");
  // Although this load path should never have existed, we ensure that policy/user
  // engines don't upgrade existing OpenSearch engines.
  engine.wrappedJSObject._loadPath = loadPath;
  engine.wrappedJSObject._extensionID = null;

  await Services.search.setDefault(engine);

  // This should not replace the existing engine, but be rejected with a
  // duplicate engine warning.
  await Assert.rejects(
    Services.search.addPolicyEngine({
      name: "simple",
      search_url: "https://example.com",
    }),
    /NS_ERROR_FILE_ALREADY_EXISTS/,
    "Should have rejected adding the engine"
  );

  // This will be added is the name is different, but will also not replace
  // the existing engine.
  await Services.search.addPolicyEngine({
    name: "simple search",
    search_url: "https://example.com",
  });

  engine = Services.search.getEngineByName("simple");
  Assert.ok(engine, "Should have kept the old engine");
  Assert.equal(
    engine.wrappedJSObject._loadPath,
    loadPath,
    "Should have kept the load path"
  );

  engine = Services.search.getEngineByName("simple search");
  Assert.ok(engine, "Should have added the new engine");
  Assert.equal(
    engine.wrappedJSObject._loadPath,
    "[other]addEngineWithDetails:set-via-policy",
    "Should have the correct load path"
  );

  Assert.equal(
    (await Services.search.getDefault()).name,
    "simple",
    "Should have kept the original engine as default"
  );
});
