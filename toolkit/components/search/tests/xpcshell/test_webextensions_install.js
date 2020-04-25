/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

const {
  createAppInfo,
  promiseRestartManager,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

ExtensionTestUtils.init(this);
AddonTestUtils.usePrivilegedSignatures = false;
AddonTestUtils.overrideCertDB();

async function restart() {
  Services.search.reset();
  await promiseRestartManager();
  await Services.search.init(false);
}

async function getEngineNames() {
  let engines = await Services.search.getEngines();
  return engines.map(engine => engine._name);
}

async function installSearchExtension(id, name) {
  let extensionInfo = {
    useAddonManager: "permanent",
    manifest: {
      version: "1.0",
      applications: {
        gecko: {
          id: id + "@tests.mozilla.org",
        },
      },
      chrome_settings_overrides: {
        search_provider: {
          name,
          search_url: "https://example.com/?q={searchTerms}",
        },
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionInfo);
  await extension.startup();

  return extension;
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
  let extension = await installSearchExtension("example", "Example");
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
