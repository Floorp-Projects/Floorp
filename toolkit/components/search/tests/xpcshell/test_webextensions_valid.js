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

  // For these tests, we sometimes stub-out the removeEngine function,
  // so that when we remove it from the add-on manager,
  // the engine is left in the search settings.
  // Save the original function, so we can access it if necessary and restore
  // it appropriately.
  oldRemoveEngineFunc = Services.search.wrappedJSObject.removeEngine.bind(
    Services.search.wrappedJSObject
  );

  registerCleanupFunction(async () => {
    Services.search.wrappedJSObject.removeEngine = oldRemoveEngineFunc;
    await promiseShutdownManager();
  });
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
    chrome_settings_overrides: {
      search_provider: {
        name: "test_policy_engine",
        search_url: "https://www.example.org/?search={searchTerms}",
      },
    },
  });

  await Services.search.runBackgroundChecks();
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);

  Assert.deepEqual(
    scalars,
    {},
    "Should not have recorded any issues for a policy defined engine"
  );
});

async function check_with_extension(
  runTest,
  expectedScalarValue,
  postTest = () => {},
  skipUnload = false
) {
  extension = await SearchTestUtils.installSearchExtension();
  let extensionId = extension.id;

  Services.telemetry.clearScalars();

  let engine = Services.search.getEngineByName("Example");
  Assert.ok(engine, "Should have installed the engine");
  // The test might cause an engine to be removed, but we don't want to
  // do that here as we want that tested via runBackgroundChecks.
  Services.search.wrappedJSObject.removeEngine = () => {};
  await runTest(engine);

  Services.search.wrappedJSObject.removeEngine = oldRemoveEngineFunc;
  await Services.search.runBackgroundChecks();

  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  if (!expectedScalarValue) {
    Assert.deepEqual(scalars, {}, "Should not have recorded any issues");
  } else {
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "browser.searchinit.engine_invalid_webextension",
      extensionId,
      expectedScalarValue
    );
  }

  await postTest(engine);

  if (!skipUnload) {
    await extension.unload();
  }
  Assert.ok(
    !Services.search.getEngineByName("Example"),
    "Should have been removed"
  );
}

add_task(async function test_valid_extensions_do_nothing() {
  await check_with_extension(
    () => {},
    null,
    async () => {
      let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
      Assert.deepEqual(scalars, {}, "Should not have recorded any issues");
    }
  );
});

add_task(async function test_different_name() {
  await check_with_extension(
    async engine => {
      engine.wrappedJSObject._name = "Example Test";
    },
    5,
    async engine => {
      engine.wrappedJSObject._name = "Example";
    }
  );
});

add_task(async function test_different_url() {
  await check_with_extension(
    async engine => {
      engine.wrappedJSObject._urls = [];
      engine.wrappedJSObject._setUrls({
        search_url: "https://example.com/123",
        search_url_get_params: "?q={searchTerms}",
      });
    },
    6,
    async () => {}
  );
});

add_task(async function test_extension_no_longer_specifies_engine() {
  await check_with_extension(async () => {
    await extension.upgrade({
      useAddonManager: "permanent",
      manifest: {
        version: "2.0",
        applications: {
          gecko: {
            id: "example@tests.mozilla.org",
          },
        },
      },
    });
  }, 4);
});

add_task(async function test_disabled_extension_pref_disabled() {
  Services.prefs.setBoolPref("browser.search.fixupEngines", false);
  await check_with_extension(
    async () => {
      await extension.addon.disable();
    },
    2,
    async () => {
      let engine = Services.search.getEngineByName("Example");
      Assert.ok(engine, "Should have kept the engine");

      extension.addon.enable();
      await extension.awaitStartup();
      await AddonTestUtils.waitForSearchProviderStartup(extension);
    }
  );
});

add_task(async function test_disabled_extension() {
  Services.prefs.clearUserPref("browser.search.fixupEngines");
  await check_with_extension(
    async () => {
      await extension.addon.disable();
    },
    2,
    async () => {
      let engine = Services.search.getEngineByName("Example");
      Assert.ok(!engine, "Should have removed the engine");

      extension.addon.enable();
      await extension.awaitStartup();
      await AddonTestUtils.waitForSearchProviderStartup(extension);
    }
  );
});

add_task(async function test_missing_extension_pref_disabled() {
  Services.prefs.setBoolPref("browser.search.fixupEngines", false);
  await check_with_extension(
    async () => {
      await extension.unload();
    },
    1,
    async () => {
      let engine = Services.search.getEngineByName("Example");
      Assert.ok(engine, "Should have kept the engine");

      // Make the test harness happy.
      await Services.search.removeEngine(engine);
    },
    true
  );
});

add_task(async function test_missing_extension() {
  Services.prefs.clearUserPref("browser.search.fixupEngines");
  await check_with_extension(
    async () => {
      await extension.unload();
    },
    1,
    async () => {
      // Make the test harness happy.
      let engine = Services.search.getEngineByName("Example");
      Assert.ok(!engine, "Should have removed the engine");
    },
    true
  );
});
