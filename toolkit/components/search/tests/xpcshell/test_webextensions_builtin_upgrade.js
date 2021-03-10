/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Enable SCOPE_APPLICATION for builtin testing.  Default in tests is only SCOPE_PROFILE.
// AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION == 5;
Services.prefs.setIntPref("extensions.enabledScopes", 5);

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

SearchTestUtils.initXPCShellAddonManager(this);

const TEST_CONFIG = [
  {
    webExtension: {
      id: "multilocale@search.mozilla.org",
      locales: ["af", "an"],
    },
    appliesTo: [{ included: { everywhere: true } }],
  },
  {
    webExtension: {
      id: "plainengine@search.mozilla.org",
    },
    appliesTo: [{ included: { everywhere: true } }],
    params: {
      searchUrlGetParams: [
        {
          name: "config",
          value: "applied",
        },
      ],
    },
  },
];

async function getEngineNames() {
  let engines = await Services.search.getEngines();
  return engines.map(engine => engine._name);
}

function makePlainExtension(version, name = "Plain") {
  return {
    useAddonManager: "permanent",
    manifest: {
      name,
      version,
      applications: {
        gecko: {
          id: "plainengine@search.mozilla.org",
        },
      },
      chrome_settings_overrides: {
        search_provider: {
          name,
          search_url: "https://duckduckgo.com/",
          params: [
            {
              name: "q",
              value: "{searchTerms}",
            },
            {
              name: "t",
              condition: "purpose",
              purpose: "contextmenu",
              value: "ffcm",
            },
            {
              name: "t",
              condition: "purpose",
              purpose: "keyword",
              value: "ffab",
            },
            {
              name: "t",
              condition: "purpose",
              purpose: "searchbar",
              value: "ffsb",
            },
            {
              name: "t",
              condition: "purpose",
              purpose: "homepage",
              value: "ffhp",
            },
            {
              name: "t",
              condition: "purpose",
              purpose: "newtab",
              value: "ffnt",
            },
          ],
          suggest_url: "https://ac.duckduckgo.com/ac/q={searchTerms}&type=list",
        },
      },
    },
  };
}

function makeMultiLocaleExtension(version) {
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
  await SearchTestUtils.useTestEngines("test-extensions", null, TEST_CONFIG);
  await promiseStartupManager();

  registerCleanupFunction(promiseShutdownManager);
  await Services.search.init();
});

add_task(async function basic_multilocale_test() {
  Assert.deepEqual(await getEngineNames(), [
    "Multilocale AF",
    "Multilocale AN",
    "Plain",
  ]);

  let ext = ExtensionTestUtils.loadExtension(makeMultiLocaleExtension("2.0"));
  await ext.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext);

  Assert.deepEqual(await getEngineNames(), [
    "Multilocale AF",
    "Multilocale AN",
    "Plain",
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
});

add_task(async function upgrade_with_configuration_change_test() {
  Assert.deepEqual(await getEngineNames(), [
    "Multilocale AF",
    "Multilocale AN",
    "Plain",
  ]);

  let engine = await Services.search.getEngineByName("Plain");
  Assert.ok(engine.isAppProvided);
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    // This test engine specifies the q and t params in its search_url, therefore
    // we get both those and the extra parameter specified in the test config.
    "https://duckduckgo.com/?q=test&t=ffsb&config=applied",
    "Should have the configuration applied before update."
  );

  let ext = ExtensionTestUtils.loadExtension(makePlainExtension("2.0"));
  await ext.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext);

  Assert.deepEqual(await getEngineNames(), [
    "Multilocale AF",
    "Multilocale AN",
    "Plain",
  ]);

  engine = await Services.search.getEngineByName("Plain");
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    // This test engine specifies the q and t params in its search_url, therefore
    // we get both those and the extra parameter specified in the test config.
    "https://duckduckgo.com/?q=test&t=ffsb&config=applied",
    "Should still have the configuration applied after update."
  );

  await ext.unload();
});

add_task(async function test_upgrade_with_name_change() {
  Assert.deepEqual(await getEngineNames(), [
    "Multilocale AF",
    "Multilocale AN",
    "Plain",
  ]);

  let ext = ExtensionTestUtils.loadExtension(
    makePlainExtension("2.0", "Plain2")
  );
  await ext.startup();
  await AddonTestUtils.waitForSearchProviderStartup(ext);

  Assert.deepEqual(await getEngineNames(), [
    "Multilocale AF",
    "Multilocale AN",
    "Plain2",
  ]);

  let engine = await Services.search.getEngineByName("Plain2");
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    // This test engine specifies the q and t params in its search_url, therefore
    // we get both those and the extra parameter specified in the test config.
    "https://duckduckgo.com/?q=test&t=ffsb&config=applied",
    "Should still have the configuration applied after update."
  );

  await ext.unload();
});
