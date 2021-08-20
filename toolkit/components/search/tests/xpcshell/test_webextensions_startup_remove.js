/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

const ENGINE_ID = "enginetest@example.com";
let xpi;
let profile = do_get_profile().clone();

add_task(async function setup() {
  await SearchTestUtils.useTestEngines("data1");
  xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      applications: {
        gecko: { id: ENGINE_ID },
      },
      chrome_settings_overrides: {
        search_provider: {
          name: "Test Engine",
          search_url: `https://example.com/?q={searchTerms}`,
        },
      },
    },
  });
  await AddonTestUtils.manuallyInstall(xpi);
});

add_task(async function test_removeAddonOnStartup() {
  // First startup the add-on manager and ensure the engine is installed.
  await AddonTestUtils.promiseStartupManager();
  let promise = promiseAfterSettings();
  await Services.search.init();

  let engine = Services.search.getEngineByName("Test Engine");
  let allEngines = await Services.search.getEngines();

  Assert.ok(!!engine, "Should have installed the test engine");

  await Services.search.setDefault(engine);
  await promise;

  await AddonTestUtils.promiseShutdownManager();

  // Now remove it, reset the search service and start up the add-on manager.
  // Note: the saved settings will have the engine in. If this didn't work,
  // the engine would still be present.
  await IOUtils.remove(
    OS.Path.join(profile.path, "extensions", `${ENGINE_ID}.xpi`)
  );

  let removePromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.REMOVED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  Services.search.wrappedJSObject.reset();
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
  await removePromise;

  Assert.ok(
    !Services.search.getEngineByName("Test Engine"),
    "Should have removed the test engine"
  );

  let newEngines = await Services.search.getEngines();
  Assert.deepEqual(
    newEngines.map(e => e.name),
    allEngines.map(e => e.name).filter(n => n != "Test Engine"),
    "Should no longer have the test engine in the full list"
  );
  let newDefault = await Services.search.getDefault();
  Assert.equal(
    newDefault.name,
    "engine1",
    "Should have changed the default engine back to the configuration default"
  );

  await AddonTestUtils.promiseShutdownManager();
});
