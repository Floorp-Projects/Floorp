/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
});

const TEST_CONFIG = [
  {
    engineName: "aol",
    orderHint: 500,
    webExtension: {
      locales: ["default"],
    },
    appliesTo: [
      {
        included: { everywhere: true },
      },
      {
        included: { regions: ["us"] },
        webExtension: {
          locales: ["$USER_LOCALE"],
        },
        telemetryId: "foo-$USER_LOCALE",
      },
      {
        included: { regions: ["fr"] },
        webExtension: {
          locales: ["$USER_REGION"],
        },
        telemetryId: "bar-$USER_REGION",
      },
    ],
  },
  {
    engineName: "lycos",
    orderHint: 1000,
    default: "yes",
    appliesTo: [
      {
        included: { everywhere: true },
        excluded: { locales: { matches: ["zh-CN"] } },
      },
    ],
  },
  {
    engineName: "altavista",
    orderHint: 2000,
    defaultPrivate: "yes",
    appliesTo: [
      {
        included: { locales: { matches: ["en-US"] } },
      },
      {
        included: { regions: ["default"] },
      },
    ],
  },
  {
    engineName: "excite",
    default: "yes-if-no-other",
    appliesTo: [
      {
        included: { everywhere: true },
        excluded: { regions: ["us"] },
      },
      {
        included: { everywhere: true },
        experiment: "acohortid",
      },
    ],
  },
  {
    engineName: "askjeeves",
  },
];

const engineSelector = new SearchEngineSelector();

add_task(async function setup() {
  const settings = await RemoteSettings(SearchUtils.SETTINGS_KEY);
  sinon.stub(settings, "get").returns(TEST_CONFIG);
});

add_task(async function test_engine_selector() {
  let {
    engines,
    privateDefault,
  } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "us",
  });
  Assert.equal(
    privateDefault.engineName,
    "altavista",
    "Should set altavista as privateDefault"
  );
  let names = engines.map(obj => obj.engineName);
  Assert.deepEqual(names, ["lycos", "altavista", "aol"], "Correct order");
  Assert.equal(
    engines[2].webExtension.locale,
    "en-US",
    "Subsequent matches in applies to can override default"
  );

  ({ engines, privateDefault } = await engineSelector.fetchEngineConfiguration({
    locale: "zh-CN",
    region: "kz",
  }));
  Assert.equal(engines.length, 2, "Correct engines are returns");
  Assert.equal(privateDefault, null, "There should be no privateDefault");
  names = engines.map(obj => obj.engineName);
  Assert.deepEqual(
    names,
    ["excite", "aol"],
    "The engines should be in the correct order"
  );

  ({ engines, privateDefault } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "us",
    experiment: "acohortid",
  }));
  Assert.deepEqual(
    engines.map(obj => obj.engineName),
    ["lycos", "altavista", "aol", "excite"],
    "Engines are in the correct order and include the experiment engine"
  );

  ({ engines, privateDefault } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "default",
    experiment: "acohortid",
  }));
  Assert.deepEqual(
    engines.map(obj => obj.engineName),
    ["lycos", "altavista", "aol", "excite"],
    "The engines should be in the correct order"
  );
  Assert.equal(
    privateDefault.engineName,
    "altavista",
    "Should set altavista as privateDefault"
  );
});

add_task(async function test_locale_region_replacement() {
  let { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "us",
  });
  let engine = engines.find(e => e.engineName == "aol");
  Assert.equal(
    engine.webExtension.locale,
    "en-US",
    "The locale is correctly inserted into the locale field"
  );
  Assert.equal(
    engine.telemetryId,
    "foo-en-US",
    "The locale is correctly inserted into the telemetryId"
  );

  ({ engines } = await engineSelector.fetchEngineConfiguration({
    locale: "it",
    region: "us",
  }));
  engine = engines.find(e => e.engineName == "aol");

  Assert.equal(
    engines.find(e => e.engineName == "aol").webExtension.locale,
    "it",
    "The locale is correctly inserted into the locale field"
  );
  Assert.equal(
    engine.telemetryId,
    "foo-it",
    "The locale is correctly inserted into the telemetryId"
  );

  ({ engines } = await engineSelector.fetchEngineConfiguration({
    locale: "en-CA",
    region: "fr",
  }));
  engine = engines.find(e => e.engineName == "aol");

  Assert.equal(
    engines.find(e => e.engineName == "aol").webExtension.locale,
    "fr",
    "The region is correctly inserted into the locale field"
  );
  Assert.equal(
    engine.telemetryId,
    "bar-fr",
    "The region is correctly inserted into the telemetryId"
  );
});
