/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const { promiseShutdownManager, promiseStartupManager } = AddonTestUtils;

let extension;
let oldRemoveEngineFunc;

add_task(async function setup() {
  await SearchTestUtils.useTestEngines("simple-engines");
  await promiseStartupManager();

  Services.telemetry.canRecordExtended = true;

  await Services.search.init();
  await promiseAfterSettings();

  extension = await SearchTestUtils.installSearchExtension({}, true);
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

  await Services.search.runBackgroundChecks();

  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);

  Assert.deepEqual(scalars, {}, "Should not have recorded any issues");
});

add_task(async function test_different_name() {
  Services.telemetry.clearScalars();

  let engine = Services.search.getEngineByName("Example");

  engine.wrappedJSObject._name = "Example Test";

  await Services.search.runBackgroundChecks();

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "browser.searchinit.engine_invalid_webextension",
    extension.id,
    5
  );

  engine.wrappedJSObject._name = "Example";
});

add_task(async function test_different_url() {
  Services.telemetry.clearScalars();

  let engine = Services.search.getEngineByName("Example");

  engine.wrappedJSObject._urls = [];
  engine.wrappedJSObject._setUrls({
    search_url: "https://example.com/123",
    search_url_get_params: "?q={searchTerms}",
  });

  await Services.search.runBackgroundChecks();

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "browser.searchinit.engine_invalid_webextension",
    extension.id,
    6
  );
});

add_task(async function test_extension_no_longer_specifies_engine() {
  Services.telemetry.clearScalars();

  let extensionInfo = {
    useAddonManager: "permanent",
    manifest: {
      version: "2.0",
      applications: {
        gecko: {
          id: "example@tests.mozilla.org",
        },
      },
    },
  };

  await extension.upgrade(extensionInfo);

  await Services.search.runBackgroundChecks();

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    "browser.searchinit.engine_invalid_webextension",
    extension.id,
    4
  );
});

add_task(async function test_disabled_extension() {
  // We don't clear scalars between tests to ensure the scalar gets set
  // to the new value, rather than added.

  // Disable the extension, this won't remove the search engine because we've
  // stubbed removeEngine.
  await extension.addon.disable();

  await Services.search.runBackgroundChecks();

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

  await Services.search.runBackgroundChecks();

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

  await Services.search.runBackgroundChecks();
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
    name: "test_policy_engine",
    search_url: "https://www.example.org/?search={searchTerms}",
  });

  await Services.search.runBackgroundChecks();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);

  Assert.deepEqual(
    scalars,
    {},
    "Should not have recorded any issues for a policy defined engine"
  );
});
