/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONFIG = [
  {
    recordType: "engine",
    identifier: "engine-1",
    base: {},
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
        partnerCode: "variant-partner-code",
        subVariants: [
          {
            environment: { regions: ["CA", "FR"] },
            telemetrySuffix: "subvariant-telemetry",
          },
          {
            environment: { regions: ["GB", "FR"] },
            partnerCode: "subvariant-partner-code",
          },
        ],
      },
    ],
  },
  {
    recordType: "defaultEngines",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

const engineSelector = new SearchEngineSelector();
let settings;
let configStub;

/**
 * This function asserts if the actual engines returned equals the expected
 * engines.
 *
 * @param {object} config
 *   A fake search config containing engines.
 * @param {object} userEnv
 *   A fake user's environment including locale and region, experiment, etc.
 * @param {Array} expectedEngines
 *   The array of expected engines to be returned from the fake config.
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

  Assert.deepEqual(engines, expectedEngines, message);
}

add_setup(async function () {
  settings = await RemoteSettings(SearchUtils.NEW_SETTINGS_KEY);
  configStub = sinon.stub(settings, "get");
});

add_task(async function test_no_subvariants_match() {
  await assertActualEnginesEqualsExpected(
    CONFIG,
    {
      locale: "fi",
      region: "FI",
    },
    [
      {
        identifier: "engine-1",
        partnerCode: "variant-partner-code",
      },
    ],
    "Should match no subvariants."
  );
});

add_task(async function test_matching_subvariant_with_properties() {
  await assertActualEnginesEqualsExpected(
    CONFIG,
    {
      locale: "en-GB",
      region: "GB",
    },
    [
      {
        identifier: "engine-1",
        partnerCode: "subvariant-partner-code",
      },
    ],
    "Should match subvariant with subvariant properties."
  );
});

add_task(async function test_matching_variant_and_subvariant_with_properties() {
  await assertActualEnginesEqualsExpected(
    CONFIG,
    {
      locale: "en-CA",
      region: "CA",
    },
    [
      {
        identifier: "engine-1",
        partnerCode: "variant-partner-code",
        telemetrySuffix: "subvariant-telemetry",
      },
    ],
    "Should match subvariant with subvariant properties."
  );
});

add_task(async function test_matching_two_subvariant_with_properties() {
  await assertActualEnginesEqualsExpected(
    CONFIG,
    {
      locale: "fr",
      region: "FR",
    },
    [
      {
        identifier: "engine-1",
        partnerCode: "subvariant-partner-code",
      },
    ],
    "Should match the last subvariant with subvariant properties."
  );
});
