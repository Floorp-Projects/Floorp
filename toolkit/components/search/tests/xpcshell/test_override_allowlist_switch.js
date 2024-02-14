/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that we correctly switch and update engines when
 * adding and removing application provided engines which overlap
 * with engines in the override allow list.
 */

"use strict";

const SEARCH_URL_BASE = "https://example.com/";
const SEARCH_URL_PARAMS = `?sourceId=enterprise&q={searchTerms}`;
const ENGINE_NAME = "Simple Engine";

const ALLOWLIST = [
  {
    thirdPartyId: "simpleengine@tests.mozilla.org",
    overridesId: "simple@search.mozilla.org",
    urls: [
      { search_url: SEARCH_URL_BASE, search_url_get_params: SEARCH_URL_PARAMS },
    ],
  },
];

const CONFIG_SIMPLE_LOCALE_DE = [
  {
    webExtension: {
      id: "basic@search.mozilla.org",
      name: "basic",
      search_url:
        "https://ar.wikipedia.org/wiki/%D8%AE%D8%A7%D8%B5:%D8%A8%D8%AD%D8%AB",
      params: [
        {
          name: "search",
          value: "{searchTerms}",
        },
        {
          name: "sourceId",
          value: "Mozilla-search",
        },
      ],
    },
    appliesTo: [
      {
        included: { everywhere: true },
        default: "yes",
      },
    ],
  },
  {
    webExtension: {
      id: "simple@search.mozilla.org",
      name: "Simple Engine",
      search_url: "https://example.com",
      params: [
        {
          name: "sourceId",
          value: "Mozilla-search",
        },
        {
          name: "search",
          value: "{searchTerms}",
        },
      ],
    },
    appliesTo: [
      {
        included: { locales: { matches: ["de"] } },
        default: "no",
      },
    ],
  },
];

const CONFIG_SIMPLE_EVERYWHERE = [
  {
    webExtension: {
      id: "basic@search.mozilla.org",
      name: "basic",
      search_url:
        "https://ar.wikipedia.org/wiki/%D8%AE%D8%A7%D8%B5:%D8%A8%D8%AD%D8%AB",
      params: [
        {
          name: "search",
          value: "{searchTerms}",
        },
        {
          name: "sourceId",
          value: "Mozilla-search",
        },
      ],
    },
    appliesTo: [
      {
        included: { everywhere: true },
        default: "yes",
      },
    ],
  },
  {
    webExtension: {
      id: "simple@search.mozilla.org",
      name: "Simple Engine",
      search_url: "https://example.com",
      params: [
        {
          name: "sourceId",
          value: "Mozilla-search",
        },
        {
          name: "search",
          value: "{searchTerms}",
        },
      ],
    },
    appliesTo: [
      {
        included: { everywhere: true },
        default: "no",
      },
    ],
  },
];

let notificationBoxStub;

add_setup(async function () {
  SearchTestUtils.useMockIdleService();
  await SearchTestUtils.useTestEngines("simple-engines");
  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "en",
    "de",
  ];
  Services.locale.requestedLocales = ["en"];

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  const settings = await RemoteSettings(SearchUtils.SETTINGS_ALLOWLIST_KEY);
  sinon.stub(settings, "get").returns(ALLOWLIST);

  notificationBoxStub = sinon.stub(
    Services.search.wrappedJSObject,
    "_showRemovalOfSearchEngineNotificationBox"
  );
});

/**
 * Tests that overrides are correctly applied when the deployment of the app
 * provided engine is extended into an area where a user has the WebExtension
 * installed and set as default.
 */
add_task(async function test_app_provided_engine_deployment_extended() {
  await assertCorrectlySwitchedWhenAdded(async () => {
    info("Change configuration");

    await SearchTestUtils.updateRemoteSettingsConfig(CONFIG_SIMPLE_EVERYWHERE);
  });
});

/**
 * Tests that overrides are correctly applied when the user's environment changes
 * e.g. they have the WebExtension installed and change to a locale where the
 * application provided engine is available.
 */
add_task(async function test_user_environment_changes() {
  await assertCorrectlySwitchedWhenAdded(async () => {
    info("Change locale to de");

    await promiseSetLocale("de");
  });
});

async function assertCorrectlySwitchedWhenAdded(changeFn) {
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG_SIMPLE_LOCALE_DE);
  notificationBoxStub.resetHistory();

  info("Install WebExtension based engine and set as default");

  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: ENGINE_NAME,
      search_url: SEARCH_URL_BASE,
      search_url_get_params: SEARCH_URL_PARAMS,
    },
    { skipUnload: true }
  );
  await extension.awaitStartup();

  await Services.search.setDefault(
    Services.search.getEngineById("simpleengine@tests.mozilla.orgdefault"),
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  await assertEngineCorrectlySet({
    expectedId: "simpleengine@tests.mozilla.orgdefault",
    appEngineOverriden: false,
  });

  await changeFn();

  await assertEngineCorrectlySet({
    expectedId: "simple@search.mozilla.orgdefault",
    appEngineOverriden: true,
  });
  Assert.ok(
    notificationBoxStub.notCalled,
    "Should not have attempted to display a notification box"
  );

  info("Test restarting search service");

  await promiseAfterSettings();
  Services.search.wrappedJSObject.reset();
  await Services.search.init();

  let extensionData = {
    ...extension.extension,
    startupReason: "APP_STARTUP",
  };
  await Services.search.maybeSetAndOverrideDefault(extensionData);

  Assert.ok(
    notificationBoxStub.notCalled,
    "Should not have attempted to display a notification box"
  );
  await assertEngineCorrectlySet({
    expectedId: "simple@search.mozilla.orgdefault",
    appEngineOverriden: true,
  });

  await extension.unload();
}

async function assertEngineCorrectlySet({ expectedId, appEngineOverriden }) {
  let engines = await Services.search.getEngines();
  Assert.equal(
    engines.filter(e => e.name == ENGINE_NAME).length,
    1,
    "Should only be one engine with matching name after changing configuration"
  );

  let engine = await Services.search.getDefault();
  Assert.equal(
    engine.id,
    expectedId,
    "Should have kept the WebExtension engine as default"
  );
  Assert.equal(
    decodeURI(engine.getSubmission("{searchTerms}").uri.spec),
    SEARCH_URL_BASE + SEARCH_URL_PARAMS,
    "Should have used the WebExtension's URLs"
  );
  Assert.equal(
    !!engine.wrappedJSObject.getAttr("overriddenBy"),
    appEngineOverriden,
    "Should have correctly overridden or not."
  );

  Assert.equal(
    engine.telemetryId,
    appEngineOverriden ? "simple-addon" : "other-Simple Engine",
    "Should set the correct telemetry Id"
  );
}
