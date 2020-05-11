/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  createAppInfo,
  promiseRestartManager,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

SearchTestUtils.initXPCShellAddonManager(this);

async function restart() {
  Services.search.reset();
  await promiseRestartManager();
  await Services.search.init(false);
}

async function getEngineNames() {
  let engines = await Services.search.getEngines();
  return engines.map(engine => engine._name);
}

add_task(async function setup() {
  await useTestEngines("test-extensions");
  await promiseStartupManager();

  registerCleanupFunction(async () => {
    await promiseShutdownManager();
    Services.prefs.clearUserPref("browser.search.geoSpecificDefaults");
    Services.prefs.clearUserPref("browser.search.region");
  });
});

add_task(async function basic_install_test() {
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);
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

  await forceExpiration();

  // User uninstalls their engine
  await extension.awaitStartup();
  await extension.unload();
  await promiseAfterCache();
  Assert.deepEqual(await getEngineNames(), ["Plain", "Special"]);
});

add_task(async function basic_multilocale_test() {
  await forceExpiration();
  Services.prefs.setCharPref("browser.search.region", "an");

  await withGeoServer(
    async function cont(requests) {
      await restart();
      Assert.deepEqual(await getEngineNames(), [
        "Plain",
        "Special",
        "Multilocale AN",
      ]);
    },
    { visibleDefaultEngines: ["multilocale-an"] }
  );
});

add_task(async function complex_multilocale_test() {
  await forceExpiration();
  Services.prefs.setCharPref("browser.search.region", "af");

  await withGeoServer(
    async function cont(requests) {
      await restart();
      Assert.deepEqual(await getEngineNames(), [
        "Plain",
        "Special",
        "Multilocale AF",
        "Multilocale AN",
      ]);
    },
    { visibleDefaultEngines: ["multilocale-af", "multilocale-an"] }
  );
});
