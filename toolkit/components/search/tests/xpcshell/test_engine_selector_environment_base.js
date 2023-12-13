/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.sys.mjs",
});

const CONFIG_EVERYWHERE = [
  {
    recordType: "engine",
    identifier: "engine-everywhere",
    base: {
      environment: {
        allRegionsAndLocales: true,
      },
    },
  },
  {
    recordType: "engine",
    identifier: "engine-everywhere-except-en-US",
    base: {
      environment: {
        allRegionsAndLocales: true,
        excludedLocales: ["en-US"],
      },
    },
  },
  {
    recordType: "engine",
    identifier: "engine-everywhere-except-FI",
    base: {
      environment: {
        allRegionsAndLocales: true,
        excludedRegions: ["FI"],
      },
    },
  },
  {
    recordType: "engine",
    identifier: "engine-everywhere-except-en-CA-and-CA",
    base: {
      environment: {
        allRegionsAndLocales: true,
        excludedRegions: ["CA"],
        excludedLocales: ["en-CA"],
      },
    },
  },
  {
    recordType: "defaultEngines",
    specificDefaults: [],
  },
];

const CONFIG_EXPERIMENT = [
  {
    recordType: "engine",
    identifier: "engine-experiment",
    base: {
      environment: {
        allRegionsAndLocales: true,
        experiment: "experiment",
      },
    },
  },
  {
    recordType: "defaultEngines",
    specificDefaults: [],
  },
];

const CONFIG_LOCALES_AND_REGIONS = [
  {
    recordType: "engine",
    identifier: "engine-canada",
    base: {
      environment: {
        locales: ["en-CA"],
        regions: ["CA"],
      },
    },
  },
  {
    recordType: "engine",
    identifier: "engine-exclude-regions",
    base: {
      environment: {
        locales: ["en-GB"],
        excludedRegions: ["US"],
      },
    },
  },
  {
    recordType: "engine",
    identifier: "engine-specific-locale-in-all-regions",
    base: {
      environment: {
        locales: ["en-US"],
      },
    },
  },
  {
    recordType: "engine",
    identifier: "engine-exclude-locale",
    base: {
      environment: {
        excludedLocales: ["fr"],
        regions: ["BE"],
      },
    },
  },
  {
    recordType: "engine",
    identifier: "engine-specific-region-with-any-locales",
    base: {
      environment: {
        regions: ["FI"],
      },
    },
  },
  {
    recordType: "defaultEngines",
    specificDefaults: [],
  },
];

const CONFIG_DISTRIBUTION = [
  {
    recordType: "engine",
    identifier: "engine-distribution-1",
    base: {
      environment: {
        distributions: ["distribution-1"],
      },
    },
  },
  {
    recordType: "engine",
    identifier: ["engine-multiple-distributions"],
    base: {
      environment: {
        distributions: ["distribution-2", "distribution-3"],
      },
    },
  },
  {
    recordType: "engine",
    identifier: "engine-distribution-region-locales",
    base: {
      environment: {
        distributions: ["distribution-4"],
        locales: ["fi"],
        regions: ["FI"],
      },
    },
  },
  {
    recordType: "engine",
    identifier: "engine-distribution-experiment",
    base: {
      environment: {
        distributions: ["distribution-5"],
        experiment: "experiment",
      },
    },
  },
  {
    recordType: "engine",
    identifier: "engine-distribution-excluded",
    base: {
      environment: {
        distributions: ["distribution-include"],
        excludedDistributions: ["distribution-exclude"],
      },
    },
  },
  {
    recordType: "defaultEngines",
    specificDefaults: [],
  },
];

const engineSelector = new SearchEngineSelector();
let settings;
let configStub;

/**
 * This function asserts if the actual engine identifiers returned equals
 * the expected engines.
 *
 * @param {object} config
 *   A mock search config contain engines.
 * @param {object} userEnv
 *   A fake user's environment including locale and region, experiment, etc.
 * @param {Array} expectedEngines
 *   The array of expected engine identifiers to be returned from the config.
 * @param {string} message
 *   The assertion message.
 */
async function assertActualEnginesEqualsExpected(
  config,
  userEnv,
  expectedEngines,
  message
) {
  engineSelector._configuration = null;
  configStub.returns(config);

  let { engines } = await engineSelector.fetchEngineConfiguration(userEnv);
  let actualEngines = engines.map(engine => engine.identifier);
  Assert.deepEqual(actualEngines, expectedEngines, message);
}

add_setup(async function () {
  settings = await RemoteSettings(SearchUtils.NEW_SETTINGS_KEY);
  configStub = sinon.stub(settings, "get");
});

add_task(async function test_selector_match_experiment() {
  assertActualEnginesEqualsExpected(
    CONFIG_EXPERIMENT,
    {
      locale: "en-CA",
      region: "ca",
      experiment: "experiment",
    },
    ["engine-experiment"],
    "Should match engine with the same experiment."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_EXPERIMENT,
    {
      locale: "en-CA",
      region: "ca",
      experiment: "no-match-experiment",
    },
    [],
    "Should not match any engines without experiment."
  );
});

add_task(async function test_everywhere_and_excluded_locale() {
  assertActualEnginesEqualsExpected(
    CONFIG_EVERYWHERE,
    {
      locale: "en-GB",
      region: "GB",
    },
    [
      "engine-everywhere",
      "engine-everywhere-except-en-US",
      "engine-everywhere-except-FI",
      "engine-everywhere-except-en-CA-and-CA",
    ],
    "Should match the engines for all locales and regions."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_EVERYWHERE,
    {
      locale: "en-US",
      region: "US",
    },
    [
      "engine-everywhere",
      "engine-everywhere-except-FI",
      "engine-everywhere-except-en-CA-and-CA",
    ],
    "Should match engines that do not exclude user's locale."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_EVERYWHERE,
    {
      locale: "fi",
      region: "FI",
    },
    [
      "engine-everywhere",
      "engine-everywhere-except-en-US",
      "engine-everywhere-except-en-CA-and-CA",
    ],
    "Should match engines that do not exclude user's region."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_LOCALES_AND_REGIONS,
    {
      locale: "en-CA",
      region: "CA",
    },
    [
      "engine-everywhere",
      "engine-everywhere-except-en-US",
      "engine-everywhere-except-FI",
    ],
    "Should match engine that do not exclude user's region and locale."
  );
});

add_task(async function test_selector_locales_and_regions() {
  assertActualEnginesEqualsExpected(
    CONFIG_LOCALES_AND_REGIONS,
    {
      locale: "en-CA",
      region: "CA",
    },
    ["engine-canada"],
    "Should match engine with same locale and region."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_LOCALES_AND_REGIONS,
    {
      locale: "en-GB",
      region: "US",
    },
    [],
    "Should not match any engines when region is excluded."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_LOCALES_AND_REGIONS,
    {
      locale: "en-US",
      region: "AU",
    },
    ["engine-specific-locale-in-all-regions"],
    "Should match engine with specified locale in any region."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_LOCALES_AND_REGIONS,
    {
      locale: "en-US",
      region: "NL",
    },
    ["engine-specific-locale-in-all-regions"],
    "Should match engine with specified locale in any region."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_LOCALES_AND_REGIONS,
    {
      locale: "fr",
      region: "BE",
    },
    [],
    "Should not match any engines when locale is excluded."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_LOCALES_AND_REGIONS,
    {
      locale: "fi",
      region: "FI",
    },
    ["engine-specific-region-with-any-locales"],
    "Should match engine with specified region with any locale."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_LOCALES_AND_REGIONS,
    {
      locale: "tlh",
      region: "FI",
    },
    ["engine-specific-region-with-any-locales"],
    "Should match engine with specified region with any locale."
  );
});

add_task(async function test_selector_match_distribution() {
  assertActualEnginesEqualsExpected(
    CONFIG_DISTRIBUTION,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distribution-1",
    },
    ["engine-distribution-1"],
    "Should match engine with the same distribution."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_DISTRIBUTION,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distribution-2",
    },
    ["engine-multiple-distributions"],
    "Should match engine with multiple distributions."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_DISTRIBUTION,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distribution-3",
    },
    ["engine-multiple-distributions"],
    "Should match engine with multiple distributions."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_DISTRIBUTION,
    {
      locale: "fi",
      region: "FI",
      distroID: "distribution-4",
    },
    ["engine-distribution-region-locales"],
    "Should match engine with distribution, specific region and locale."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_DISTRIBUTION,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distribution-4",
    },
    [],
    "Should not match any engines with no matching distribution, region and locale."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_DISTRIBUTION,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distribution-5",
      experiment: "experiment",
    },
    ["engine-distribution-experiment"],
    "Should match engine with distribution and experiment."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_DISTRIBUTION,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distribution-5",
      experiment: "no-match-experiment",
    },
    [],
    "Should not match any engines with no matching distribution and experiment."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_DISTRIBUTION,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distribution-include",
    },
    ["engine-distribution-excluded"],
    "Should match engines with included distributions."
  );

  assertActualEnginesEqualsExpected(
    CONFIG_DISTRIBUTION,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distribution-exclude",
    },
    [],
    "Should not match any engines with excluded distribution."
  );
});
