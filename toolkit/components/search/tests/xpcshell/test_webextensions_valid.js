/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const { promiseShutdownManager, promiseStartupManager } = AddonTestUtils;

SearchTestUtils.initXPCShellAddonManager(this);

let extension;
let oldRemoveEngineFunc;

async function getEngineNames() {
  let engines = await Services.search.getEngines();
  return engines.map(engine => engine._name);
}

add_task(async function setup() {
  await SearchTestUtils.useTestEngines("simple-engines");
  await promiseStartupManager();

  Services.telemetry.canRecordExtended = true;

  await Services.search.init();
  await promiseAfterSettings();

  extension = await SearchTestUtils.installSearchExtension();
  await extension.awaitStartup();

  // For these tests, stub-out the removeEngine function, so that when we
  // remove it from the add-on manager, the engine is left in the search
  // settings.
  oldRemoveEngineFunc = Services.search.wrappedJSObject.removeEngine.bind(
    Services.search.wrappedJSObject
  );
  Services.search.wrappedJSObject.removeEngine = () => {};

  registerCleanupFunction(async () => {
    await promiseShutdownManager();
  });
});

add_task(async function test_valid_extensions_do_nothing() {
  Services.telemetry.clearScalars();

  Assert.ok(
    Services.search.getEngineByName("Example"),
    "Should have installed the engine"
  );

  await Services.search.checkWebExtensionEngines();

  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);

  Assert.deepEqual(scalars, {}, "Should not have recorded any issues");
});

add_task(async function test_different_url() {
  Services.telemetry.clearScalars();

  let engine = Services.search.getEngineByName("Example");

  engine.wrappedJSObject._urls = [];
  engine.wrappedJSObject._setUrls({
    name: "Example",
    search_url: "https://example.com/123",
    search_url_get_params: "?q={searchTerms}",
  });

  await Services.search.checkWebExtensionEngines();

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "browser.searchinit.engine_invalid_webextension",
    extension.id,
    3
  );
});

add_task(async function test_disabled_extension() {
  // We don't clear scalars between tests to ensure the scalar gets set
  // to the new value, rather than added.

  // Disable the extension, this won't remove the search engine because we've
  // stubbed removeEngine.
  await extension.addon.disable();

  await Services.search.checkWebExtensionEngines();

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "browser.searchinit.engine_invalid_webextension",
    extension.id,
    2
  );

  extension.addon.enable();
  await extension.awaitStartup();
});

add_task(async function test_missing_extension() {
  // We don't clear scalars between tests to ensure the scalar gets set
  // to the new value, rather than added.

  let extensionId = extension.id;
  // Remove the extension, this won't remove the search engine because we've
  // stubbed removeEngine.
  await extension.unload();

  await Services.search.checkWebExtensionEngines();

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "browser.searchinit.engine_invalid_webextension",
    extensionId,
    1
  );

  await oldRemoveEngineFunc(Services.search.getEngineByName("Example"));
});

add_task(async function test_user_engine() {
  Services.telemetry.clearScalars();

  await Services.search.addUserEngine("test", "https://example.com/", "fake");

  await Services.search.checkWebExtensionEngines();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);

  Assert.deepEqual(
    scalars,
    {},
    "Should not have recorded any issues for a user-defined engine"
  );
});

add_task(async function test_policy_engine() {
  Services.telemetry.clearScalars();

  await Services.search.addPolicyEngine({
    description: "test policy engine",
    chrome_settings_overrides: {
      search_provider: {
        name: "test_policy_engine",
        search_url: "https://www.example.org/?search={searchTerms}",
      },
    },
  });

  await Services.search.checkWebExtensionEngines();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);

  Assert.deepEqual(
    scalars,
    {},
    "Should not have recorded any issues for a policy defined engine"
  );
});
