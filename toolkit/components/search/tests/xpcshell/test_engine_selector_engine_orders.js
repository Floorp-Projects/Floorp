/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests the SearchEngineSelector in ordering the engines correctly based
 * on the user's environment.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.sys.mjs",
});

const ENGINE_ORDERS_CONFIG = [
  {
    recordType: "engine",
    identifier: "b-engine",
    base: {},
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "a-engine",
    base: {},
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "c-engine",
    base: {},
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    recordType: "defaultEngines",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [
      {
        environment: { distributions: ["distro"] },
        order: ["a-engine", "b-engine", "c-engine"],
      },
      {
        environment: {
          distributions: ["distro"],
          locales: ["en-CA"],
          regions: ["CA"],
        },
        order: ["c-engine", "b-engine", "a-engine"],
      },
      {
        environment: {
          distributions: ["distro-2"],
        },
        order: ["a-engine", "b-engine"],
      },
    ],
  },
];

const STARTS_WITH_WIKI_CONFIG = [
  {
    recordType: "engine",
    identifier: "wiki-ca",
    base: {},
    variants: [
      {
        environment: {
          locales: ["en-CA"],
          regions: ["CA"],
        },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "wiki-uk",
    base: {},
    variants: [
      {
        environment: {
          locales: ["en-GB"],
          regions: ["GB"],
        },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "engine-1",
    base: {},
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "engine-2",
    base: {},
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    recordType: "defaultEngines",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [
      {
        environment: {
          locales: ["en-CA"],
          regions: ["CA"],
        },
        order: ["wiki*", "engine-1", "engine-2"],
      },
      {
        environment: {
          locales: ["en-GB"],
          regions: ["GB"],
        },
        order: ["wiki*", "engine-1", "engine-2"],
      },
    ],
  },
];

const DEFAULTS_CONFIG = [
  {
    recordType: "engine",
    identifier: "b-engine",
    base: {},
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "a-engine",
    base: {},
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "default-engine",
    base: {},
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "default-private-engine",
    base: {},
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    recordType: "defaultEngines",
    globalDefault: "default-engine",
    globalDefaultPrivate: "default-private-engine",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [
      {
        environment: {
          locales: ["en-CA"],
          regions: ["CA"],
        },
        order: ["a-engine", "b-engine"],
      },
    ],
  },
];

const engineSelector = new SearchEngineSelector();
let settings;
let settingOverrides;
let configStub;
let overrideStub;

/**
 * This function asserts if the actual engine identifiers returned equals
 * the expected engines.
 *
 * @param {object} config
 *   A mock search config contain engines.
 * @param {object} userEnv
 *   A fake user's environment including locale and region, experiment, etc.
 * @param {Array} expectedEngineOrders
 *   The array of engine identifers in the expected order.
 * @param {string} message
 *   The description of the test.
 */
async function assertActualEnginesEqualsExpected(
  config,
  userEnv,
  expectedEngineOrders,
  message
) {
  engineSelector._configuration = null;
  configStub.returns(config);

  let { engines } = await engineSelector.fetchEngineConfiguration(userEnv);
  let actualEngineOrders = engines.map(engine => engine.identifier);

  info(`${message}`);
  Assert.deepEqual(actualEngineOrders, expectedEngineOrders, message);
}

add_setup(async function () {
  settings = await RemoteSettings(SearchUtils.NEW_SETTINGS_KEY);
  configStub = sinon.stub(settings, "get");
  settingOverrides = await RemoteSettings(
    SearchUtils.NEW_SETTINGS_OVERRIDES_KEY
  );
  overrideStub = sinon.stub(settingOverrides, "get");
  overrideStub.returns([]);
});

add_task(async function test_selector_match_engine_orders() {
  await assertActualEnginesEqualsExpected(
    ENGINE_ORDERS_CONFIG,
    {
      locale: "fr",
      region: "FR",
      distroID: "distro",
    },
    ["a-engine", "b-engine", "c-engine"],
    "Should match engine orders with the distro distribution."
  );

  await assertActualEnginesEqualsExpected(
    ENGINE_ORDERS_CONFIG,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distro",
    },
    ["c-engine", "b-engine", "a-engine"],
    "Should match engine orders with the distro distribution, en-CA locale and CA region."
  );

  await assertActualEnginesEqualsExpected(
    ENGINE_ORDERS_CONFIG,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distro-2",
    },
    ["a-engine", "b-engine", "c-engine"],
    "Should order the first two engines correctly for distro-2 distribution"
  );

  await assertActualEnginesEqualsExpected(
    ENGINE_ORDERS_CONFIG,
    {
      locale: "en-CA",
      region: "CA",
    },
    ["b-engine", "a-engine", "c-engine"],
    "Should be in the same engine order as the config when there's no engine order environments matched."
  );
});

add_task(async function test_selector_match_engine_orders_starts_with() {
  await assertActualEnginesEqualsExpected(
    STARTS_WITH_WIKI_CONFIG,
    {
      locale: "en-CA",
      region: "CA",
    },
    ["wiki-ca", "engine-1", "engine-2"],
    "Should list the wiki-ca engine and other engines in correct orders with the en-CA and CA locale region environment."
  );

  await assertActualEnginesEqualsExpected(
    STARTS_WITH_WIKI_CONFIG,
    {
      locale: "en-GB",
      region: "GB",
    },
    ["wiki-uk", "engine-1", "engine-2"],
    "Should list the wiki-ca engine and other engines in correct orders with the en-CA and CA locale region environment."
  );
});

add_task(async function test_selector_match_engine_orders_with_defaults() {
  await assertActualEnginesEqualsExpected(
    DEFAULTS_CONFIG,
    {
      locale: "en-CA",
      region: "CA",
    },
    ["default-engine", "default-private-engine", "a-engine", "b-engine"],
    "Should order the default engine first, default private engine second, and the rest of the engines in the correct order."
  );
});
