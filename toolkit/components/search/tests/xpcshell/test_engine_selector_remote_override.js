/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchEngineSelectorOld:
    "resource://gre/modules/SearchEngineSelectorOld.sys.mjs",
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.sys.mjs",
});

const TEST_CONFIG_OLD = [
  {
    engineName: "aol",
    telemetryId: "aol",
    appliesTo: [
      {
        included: { everywhere: true },
      },
    ],
    params: {
      searchUrlGetParams: [
        {
          name: "original_param",
          value: "original_value",
        },
      ],
    },
  },
];

const TEST_CONFIG_OVERRIDE_OLD = [
  {
    telemetryId: "aol",
    telemetrySuffix: "tsfx",
    params: {
      searchUrlGetParams: [
        {
          name: "new_param",
          value: "new_value",
        },
      ],
    },
  },
];

const TEST_CONFIG = [
  {
    base: {
      urls: {
        search: {
          base: "https://www.bing.com/search",
          params: [
            {
              name: "old_param",
              value: "old_value",
            },
          ],
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: {
          locales: ["en-US"],
        },
      },
    ],
    identifier: "aol",
    recordType: "engine",
  },
  {
    recordType: "defaultEngines",
    globalDefault: "aol",
    specificDefaults: [],
  },
  {
    orders: [],
    recordType: "engineOrders",
  },
];

const TEST_CONFIG_OVERRIDE = [
  {
    identifier: "aol",
    urls: {
      search: {
        params: [{ name: "new_param", value: "new_value" }],
      },
    },
    telemetrySuffix: "tsfx",
    clickUrl: "https://aol.url",
  },
];

const engineSelectorOld = new SearchEngineSelectorOld();
const engineSelector = new SearchEngineSelector();

add_setup(async function () {
  const settingsOld = await RemoteSettings(SearchUtils.OLD_SETTINGS_KEY);
  sinon.stub(settingsOld, "get").returns(TEST_CONFIG_OLD);
  const overridesOld = await RemoteSettings(
    SearchUtils.OLD_SETTINGS_OVERRIDES_KEY
  );
  sinon.stub(overridesOld, "get").returns(TEST_CONFIG_OVERRIDE_OLD);

  const settings = await RemoteSettings(SearchUtils.NEW_SETTINGS_KEY);
  sinon.stub(settings, "get").returns(TEST_CONFIG);
  const overrides = await RemoteSettings(
    SearchUtils.NEW_SETTINGS_OVERRIDES_KEY
  );
  sinon.stub(overrides, "get").returns(TEST_CONFIG_OVERRIDE);
});

add_task(async function test_engine_selector_old() {
  let { engines } = await engineSelectorOld.fetchEngineConfiguration({
    locale: "en-US",
    region: "us",
  });
  Assert.equal(engines[0].telemetryId, "aol-tsfx");
  Assert.equal(engines[0].params.searchUrlGetParams[0].name, "new_param");
  Assert.equal(engines[0].params.searchUrlGetParams[0].value, "new_value");
});

add_task(async function test_engine_selector() {
  let { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "us",
  });
  Assert.equal(engines[0].telemetrySuffix, "tsfx");
  Assert.equal(engines[0].clickUrl, "https://aol.url");
  Assert.equal(engines[0].urls.search.params[0].name, "new_param");
  Assert.equal(engines[0].urls.search.params[0].value, "new_value");
});
