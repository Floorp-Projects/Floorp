/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};

const { promiseShutdownManager, promiseStartupManager } = AddonTestUtils;

ChromeUtils.defineESModuleGetters(lazy, {
  ExtensionTestUtils:
    "resource://testing-common/ExtensionXPCShellUtils.sys.mjs",
});

add_setup(async function () {
  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");
  await SearchTestUtils.useTestEngines("test-extensions");
  await promiseStartupManager();

  registerCleanupFunction(async () => {
    await promiseShutdownManager();
  });
});

add_task(async function test_install_duplicate_engine_startup() {
  let name = "Plain";
  let id = "plain@tests.mozilla.org";
  consoleAllowList.push(
    `#installExtensionEngine failed for ${id}`,
    `An engine called ${name} already exists`
  );
  // Do not use SearchTestUtils.installSearchExtension, as we need to manually
  // start the search service after installing the extension.
  let extensionInfo = {
    useAddonManager: "permanent",
    files: {},
    manifest: SearchTestUtils.createEngineManifest({
      name,
      search_url: "https://example.com/plain",
    }),
  };

  let extension = lazy.ExtensionTestUtils.loadExtension(extensionInfo);
  await extension.startup();

  await Services.search.init();

  await AddonTestUtils.waitForSearchProviderStartup(extension);
  let engine = await Services.search.getEngineByName(name);
  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://duckduckgo.com/?q=foo&t=ffsb",
    "Should have not changed the app provided engine."
  );

  await extension.unload();
});
