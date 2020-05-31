/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kBaseURL = "https://example.com/";
const kSearchEngineURL = `${kBaseURL}?q={searchTerms}&foo=myparams`;
const kOverriddenEngineName = "Simple Engine";

SearchTestUtils.initXPCShellAddonManager(this);

const whitelist = [
  {
    thirdPartyId: "test@thirdparty.example.com",
    overridesId: "simple@search.mozilla.org",
    urls: [],
  },
];

const tests = [
  {
    title: "test_not_changing_anything",
    startupReason: "ADDON_INSTALL",
    search_provider: {
      is_default: true,
      name: "MozParamsTest2",
      keyword: "MozSearch",
      search_url: kSearchEngineURL,
    },
    expected: {
      switchToDefaultAllowed: false,
      canInstallEngine: true,
      overridesEngine: false,
    },
  },
  {
    title: "test_changing_default_engine",
    startupReason: "ADDON_INSTALL",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kSearchEngineURL,
    },
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: false,
    },
  },
  {
    title: "test_changing_default_engine",
    startupReason: "ADDON_ENABLE",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kSearchEngineURL,
    },
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: false,
    },
  },
  {
    title: "test_overriding_default_engine",
    startupReason: "ADDON_INSTALL",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kSearchEngineURL,
    },
    whitelistUrls: [
      {
        search_url: kSearchEngineURL,
      },
    ],
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: true,
      searchUrl: kSearchEngineURL,
    },
  },
  {
    title: "test_overriding_default_engine_enable",
    startupReason: "ADDON_ENABLE",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kSearchEngineURL,
    },
    whitelistUrls: [
      {
        search_url: kSearchEngineURL,
      },
    ],
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: true,
      searchUrl: kSearchEngineURL,
    },
  },
  {
    title: "test_overriding_default_engine_different_url",
    startupReason: "ADDON_INSTALL",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kSearchEngineURL + "a",
    },
    whitelistUrls: [
      {
        search_url: kSearchEngineURL,
      },
    ],
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: false,
    },
  },
  {
    title: "test_overriding_default_engine_get_params",
    startupReason: "ADDON_INSTALL",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kBaseURL,
      search_url_get_params: "q={searchTerms}&enc=UTF-8",
    },
    whitelistUrls: [
      {
        search_url: kBaseURL,
        search_url_get_params: "q={searchTerms}&enc=UTF-8",
      },
    ],
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: true,
      searchUrl: `${kBaseURL}?q={searchTerms}&enc=UTF-8`,
    },
  },
  {
    title: "test_overriding_default_engine_different_get_params",
    startupReason: "ADDON_INSTALL",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kBaseURL,
      search_url_get_params: "q={searchTerms}&enc=UTF-8a",
    },
    whitelistUrls: [
      {
        search_url: kBaseURL,
        search_url_get_params: "q={searchTerms}&enc=UTF-8",
      },
    ],
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: false,
    },
  },
  {
    title: "test_overriding_default_engine_post_params",
    startupReason: "ADDON_INSTALL",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kBaseURL,
      search_url_post_params: "q={searchTerms}&enc=UTF-8",
    },
    whitelistUrls: [
      {
        search_url: kBaseURL,
        search_url_post_params: "q={searchTerms}&enc=UTF-8",
      },
    ],
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: true,
      searchUrl: `${kBaseURL}`,
      postData: "q={searchTerms}&enc=UTF-8",
    },
  },
  {
    title: "test_overriding_default_engine_different_post_params",
    startupReason: "ADDON_INSTALL",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kBaseURL,
      search_url_post_params: "q={searchTerms}&enc=UTF-8a",
    },
    whitelistUrls: [
      {
        search_url: kBaseURL,
        search_url_post_params: "q={searchTerms}&enc=UTF-8",
      },
    ],
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: false,
    },
  },
  {
    title: "test_overriding_default_engine_search_form",
    startupReason: "ADDON_INSTALL",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kBaseURL,
      search_form: "https://example.com/form",
    },
    whitelistUrls: [
      {
        search_url: kBaseURL,
        search_form: "https://example.com/form",
      },
    ],
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: true,
      searchUrl: `${kBaseURL}`,
      searchForm: "https://example.com/form",
    },
  },
  {
    title: "test_overriding_default_engine_different_search_form",
    startupReason: "ADDON_INSTALL",
    search_provider: {
      is_default: true,
      name: kOverriddenEngineName,
      keyword: "MozSearch",
      search_url: kBaseURL,
      search_form: "https://example.com/forma",
    },
    whitelistUrls: [
      {
        search_url: kBaseURL,
        search_form: "https://example.com/form",
      },
    ],
    expected: {
      switchToDefaultAllowed: true,
      canInstallEngine: false,
      overridesEngine: false,
    },
  },
];

let baseExtension;
let remoteSettingsStub;

add_task(async function setup() {
  await SearchTestUtils.useTestEngines("simple-engines");
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  baseExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "test@thirdparty.example.com",
        },
      },
    },
    useAddonManager: "permanent",
  });
  await baseExtension.startup();

  const settings = await RemoteSettings(SearchUtils.SETTINGS_ALLOWLIST_KEY);
  remoteSettingsStub = sinon.stub(settings, "get").returns([]);

  registerCleanupFunction(async () => {
    await baseExtension.unload();
  });
});

for (const test of tests) {
  add_task(async () => {
    info(test.title);

    let extension = {
      ...baseExtension,
      startupReason: test.startupReason,
      manifest: {
        chrome_settings_overrides: {
          search_provider: test.search_provider,
        },
      },
    };

    if (test.expected.overridesEngine) {
      remoteSettingsStub.returns([
        { ...whitelist[0], urls: test.whitelistUrls },
      ]);
    }

    let result = await Services.search.maybeSetAndOverrideDefault(extension);
    Assert.equal(
      result.canChangeToAppProvided,
      test.expected.switchToDefaultAllowed,
      "Should have returned the correct value for allowing switch to default or not."
    );
    Assert.equal(
      result.canInstallEngine,
      test.expected.canInstallEngine,
      "Should have returned the correct value for allowing to install the engine or not."
    );

    let engine = await Services.search.getEngineByName(kOverriddenEngineName);
    Assert.equal(
      !!engine.wrappedJSObject.getAttr("overriddenBy"),
      test.expected.overridesEngine,
      "Should have correctly overridden or not."
    );

    Assert.equal(
      engine.telemetryId,
      "simple" + (test.expected.overridesEngine ? "-addon" : ""),
      "Should set the correct telemetry Id"
    );

    if (test.expected.overridesEngine) {
      let submission = engine.getSubmission("{searchTerms}");
      Assert.equal(
        decodeURI(submission.uri.spec),
        test.expected.searchUrl,
        "Should have set the correct url on an overriden engine"
      );

      if (test.expected.search_form) {
        Assert.equal(
          engine.wrappedJSObject._searchForm,
          test.expected.searchForm,
          "Should have overridden the search form."
        );
      }

      if (test.expected.postData) {
        let sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
          Ci.nsIScriptableInputStream
        );
        sis.init(submission.postData);
        let data = sis.read(submission.postData.available());
        Assert.equal(
          decodeURIComponent(data),
          test.expected.postData,
          "Should have overridden the postData"
        );
      }

      // As we're not testing the WebExtension manager as well,
      // set this engine as default so we can check the telemetry data.
      let oldDefaultEngine = Services.search.defaultEngine;
      Services.search.defaultEngine = engine;

      let engineInfo = await Services.search.getDefaultEngineInfo();
      Assert.deepEqual(
        engineInfo,
        {
          defaultSearchEngine: "simple-addon",
          defaultSearchEngineData: {
            loadPath: "[other]addEngineWithDetails:simple@search.mozilla.org",
            name: "Simple Engine",
            origin: "default",
            submissionURL: test.expected.searchUrl.replace("{searchTerms}", ""),
          },
        },
        "Should return the extended identifier and alternate submission url to telemetry"
      );
      Services.search.defaultEngine = oldDefaultEngine;

      engine.wrappedJSObject.removeExtensionOverride();
    }
  });
}
