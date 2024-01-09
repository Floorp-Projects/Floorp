/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchEngineSelectorOld:
    "resource://gre/modules/SearchEngineSelectorOld.sys.mjs",
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

const engineSelectorOld = new SearchEngineSelectorOld();

add_setup(async function () {
  const settingsOld = await RemoteSettings(SearchUtils.OLD_SETTINGS_KEY);
  sinon.stub(settingsOld, "get").returns(TEST_CONFIG_OLD);
  const overridesOld = await RemoteSettings(
    SearchUtils.OLD_SETTINGS_OVERRIDES_KEY
  );
  sinon.stub(overridesOld, "get").returns(TEST_CONFIG_OVERRIDE_OLD);
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
