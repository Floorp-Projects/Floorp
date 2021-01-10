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
  await SearchTestUtils.useTestEngines("test-extensions");
  await promiseStartupManager();

  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "af",
  ];

  registerCleanupFunction(async () => {
    await promiseShutdownManager();
    Services.prefs.clearUserPref("browser.search.region");
  });
});

add_task(async function basic_install_test() {
  await Services.search.init();
  await promiseAfterSettings();

  // On first boot, we get the configuration defaults
  Assert.deepEqual(await getEngineNames(), ["Plain", "Special"]);

  // User installs a new search engine
  let extension = await SearchTestUtils.installSearchExtension({
    encoding: "windows-1252",
  });
  Assert.deepEqual((await getEngineNames()).sort(), [
    "Example",
    "Plain",
    "Special",
  ]);

  let engine = await Services.search.getEngineByName("Example");
  Assert.equal(
    engine.wrappedJSObject.queryCharset,
    "windows-1252",
    "Should have the correct charset"
  );

  // User uninstalls their engine
  await extension.awaitStartup();
  await extension.unload();
  await promiseAfterSettings();
  Assert.deepEqual(await getEngineNames(), ["Plain", "Special"]);
});

add_task(async function basic_multilocale_test() {
  await promiseSetHomeRegion("an");

  Assert.deepEqual(await getEngineNames(), [
    "Plain",
    "Special",
    "Multilocale AN",
  ]);
});

add_task(async function complex_multilocale_test() {
  await promiseSetHomeRegion("af");

  Assert.deepEqual(await getEngineNames(), [
    "Plain",
    "Special",
    "Multilocale AF",
    "Multilocale AN",
  ]);
});

add_task(async function test_manifest_selection() {
  // Sets the home region without updating.
  Region._setHomeRegion("an", false);
  await promiseSetLocale("af");

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
