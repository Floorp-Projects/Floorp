/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that WebExtensions correctly load on startup without errors.
 */

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExtensionTestUtils:
    "resource://testing-common/ExtensionXPCShellUtils.sys.mjs",
});

const { promiseShutdownManager, promiseStartupManager } = AddonTestUtils;

let extension;

add_setup(async function () {
  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");
  await SearchTestUtils.useTestEngines("test-extensions");
  await promiseStartupManager();

  registerCleanupFunction(async () => {
    await promiseShutdownManager();
  });
});

add_task(async function test_startup_with_new_addon() {
  // Although rare, handling loading an add-on on startup should work.
  // Additionally, this sub-test allows us to pre-fill the search settings
  // for the subsequent tests.

  // Do not use SearchTestUtils.installSearchExtension, as we need to manually
  // start the search service after installing the extension.
  let extensionInfo = {
    useAddonManager: "permanent",
    files: {},
    manifest: SearchTestUtils.createEngineManifest({
      name: "startup",
      search_url: "https://example.com/",
    }),
  };

  extension = lazy.ExtensionTestUtils.loadExtension(extensionInfo);
  await extension.startup();

  let settingsWritten = promiseAfterSettings();
  await Services.search.init();

  await AddonTestUtils.waitForSearchProviderStartup(extension);
  await settingsWritten;

  let engine = await Services.search.getEngineByName("startup");
  Assert.ok(engine, "Should have loaded the engine");
  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://example.com/?q=foo",
    "Should have the expected search url."
  );
});

add_task(async function test_startup_with_existing_addon_from_settings() {
  Services.search.wrappedJSObject.reset();

  let settingsWritten = promiseAfterSettings();
  await Services.search.init();
  await settingsWritten;

  let engine = await Services.search.getEngineByName("startup");
  Assert.ok(engine, "Should have loaded the engine");
  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://example.com/?q=foo",
    "Should have the expected search url."
  );
});

add_task(
  async function test_startup_with_existing_addon_with_startup_notification() {
    // Checks that we correctly load the add-on on startup when we are notified
    // about it from the add-on manager before search has initialised. Also
    // ensures that we don't raise an error when loading it from settings
    // when the add-on is already there. The console check is handled by
    // TestUtils.listenForConsoleMessages() in head_search.js.

    Services.search.wrappedJSObject.reset();

    await Services.search.addEnginesFromExtension(extension.extension);

    let settingsWritten = promiseAfterSettings();
    await Services.search.init();
    await settingsWritten;

    let engine = await Services.search.getEngineByName("startup");
    Assert.ok(engine, "Should have loaded the engine");
    let submission = engine.getSubmission("foo");
    Assert.equal(
      submission.uri.spec,
      "https://example.com/?q=foo",
      "Should have the expected search url."
    );

    await extension.unload();
  }
);
