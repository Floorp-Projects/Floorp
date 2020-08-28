/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  ExtensionTestUtils: "resource://testing-common/ExtensionXPCShellUtils.jsm",
});

const {
  promiseRestartManager,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

SearchTestUtils.initXPCShellAddonManager(this);

async function getEngineNames() {
  let engines = await Services.search.getEngines();
  return engines.map(engine => engine._name);
}

add_task(async function setup() {
  await useTestEngines("test-extensions");
  await promiseStartupManager();

  registerCleanupFunction(async () => {
    await promiseShutdownManager();
    Services.prefs.clearUserPref("browser.search.region");
  });
});

add_task(async function basic_install_test() {
  await Services.search.init();
  await promiseAfterCache();

  // On first boot, we get the list.json defaults
  Assert.deepEqual(await getEngineNames(), ["Plain", "Special"]);

  // User installs a new search engine
  let extension = await SearchTestUtils.installSearchExtension();
  Assert.deepEqual((await getEngineNames()).sort(), [
    "Example",
    "Plain",
    "Special",
  ]);

  // User uninstalls their engine
  await extension.awaitStartup();
  await extension.unload();
  await promiseAfterCache();
  Assert.deepEqual(await getEngineNames(), ["Plain", "Special"]);
});

add_task(async function basic_multilocale_test() {
  let promise = SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Region._setHomeRegion("an");
  await promise;

  Assert.deepEqual(await getEngineNames(), [
    "Plain",
    "Special",
    "Multilocale AN",
  ]);
});

add_task(async function complex_multilocale_test() {
  let promise = SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Region._setHomeRegion("af");
  await promise;

  Assert.deepEqual(await getEngineNames(), [
    "Plain",
    "Special",
    "Multilocale AF",
    "Multilocale AN",
  ]);
});
add_task(async function test_manifest_selection() {
  Region._setHomeRegion("an", false);
  let promise = SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Services.locale.availableLocales = ["af"];
  Services.locale.requestedLocales = ["af"];
  await promise;

  let engine = await Services.search.getEngineByName("Multilocale AN");
  Assert.ok(
    engine.iconURI.spec.endsWith("favicon-an.ico"),
    "Should have the correct favicon for an extension of one locale using a different locale."
  );
  Assert.equal(
    engine.description,
    "A enciclopedia Libre",
    "Should have the correct engine name for an extension of one locale using a different locale."
  );
});
