/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

ExtensionTestUtils.init(this);
AddonTestUtils.usePrivilegedSignatures = false;
AddonTestUtils.overrideCertDB();

const TEST_CONFIG = [
  {
    webExtension: {
      id: "multilocale@search.mozilla.org",
      locales: ["af", "an"],
    },
    appliesTo: [{ included: { everywhere: true } }],
  },
];

async function getEngineNames() {
  let engines = await Services.search.getEngines();
  return engines.map(engine => engine._name);
}

function makeExtension(version) {
  return {
    useAddonManager: "permanent",
    manifest: {
      name: "__MSG_searchName__",
      version,
      applications: {
        gecko: {
          id: "multilocale@search.mozilla.org",
        },
      },
      default_locale: "an",
      chrome_settings_overrides: {
        search_provider: {
          name: "__MSG_searchName__",
          search_url: "__MSG_searchUrl__",
        },
      },
    },
    files: {
      "_locales/af/messages.json": {
        searchUrl: {
          message: `https://example.af/?q={searchTerms}&version=${version}`,
          description: "foo",
        },
        searchName: {
          message: `Multilocale AF`,
          description: "foo",
        },
      },
      "_locales/an/messages.json": {
        searchUrl: {
          message: `https://example.an/?q={searchTerms}&version=${version}`,
          description: "foo",
        },
        searchName: {
          message: `Multilocale AN`,
          description: "foo",
        },
      },
    },
  };
}

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.search.gModernConfig", true);

  await useTestEngines("test-extensions", null, TEST_CONFIG);
  await promiseStartupManager();

  registerCleanupFunction(promiseShutdownManager);
  await Services.search.init();
});

add_task(async function basic_multilocale_test() {
  Assert.deepEqual(await getEngineNames(), [
    "Multilocale AF",
    "Multilocale AN",
  ]);

  let ext = ExtensionTestUtils.loadExtension(makeExtension("2.0"));
  await ext.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext);

  Assert.deepEqual(await getEngineNames(), [
    "Multilocale AF",
    "Multilocale AN",
  ]);

  let engine = await Services.search.getEngineByName("Multilocale AF");
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    "https://example.af/?q=test&version=2.0",
    "Engine got update"
  );
  engine = await Services.search.getEngineByName("Multilocale AN");
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    "https://example.an/?q=test&version=2.0",
    "Engine got update"
  );

  await ext.unload();
  await promiseAfterCache();
});
