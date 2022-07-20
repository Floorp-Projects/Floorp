/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.sys.mjs",
});

const TEST_CONFIG = [
  {
    webExtension: {
      id: "aol@example.com",
    },
    appliesTo: [
      {
        included: { everywhere: true },
      },
      {
        override: true,
        application: {
          distributions: ["distro1"],
        },
        params: {
          searchUrlGetParams: [
            {
              name: "field-keywords",
              value: "{searchTerms}",
            },
          ],
        },
      },
    ],
    default: "yes-if-no-other",
  },
  {
    webExtension: {
      id: "lycos@example.com",
    },
    appliesTo: [
      {
        included: { everywhere: true },
      },
      {
        override: true,
        experiment: "experiment1",
        sendAttributionRequest: true,
      },
    ],
    default: "yes",
  },
  {
    webExtension: {
      id: "altavista@example.com",
    },
    appliesTo: [
      {
        included: { everywhere: true },
      },
      {
        override: true,
        application: {
          distributions: ["distro2"],
        },
        included: { regions: ["gb"] },
        params: {
          searchUrlGetParams: [
            {
              name: "field-keywords2",
              value: "{searchTerms}",
            },
          ],
        },
      },
    ],
    default: "yes",
  },
];

const engineSelector = new SearchEngineSelector();

add_task(async function setup() {
  const settings = await RemoteSettings(SearchUtils.SETTINGS_KEY);
  sinon.stub(settings, "get").returns(TEST_CONFIG);
});

add_task(async function test_engine_selector_defaults() {
  // Check that with no override sections matching, we have no overrides active.
  const { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "us",
  });

  let engine = engines.find(e => e.webExtension.id == "aol@example.com");

  Assert.ok(
    !("params" in engine),
    "Should not have overriden the parameters of the engine."
  );

  engine = engines.find(e => e.webExtension.id == "lycos@example.com");

  Assert.ok(
    !("sendAttributionRequest" in engine),
    "Should have overriden the sendAttributionRequest field of the engine."
  );
});

add_task(async function test_engine_selector_override_distributions() {
  const { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "us",
    distroID: "distro1",
  });

  let engine = engines.find(e => e.webExtension.id == "aol@example.com");

  Assert.deepEqual(
    engine.params,
    {
      searchUrlGetParams: [
        {
          name: "field-keywords",
          value: "{searchTerms}",
        },
      ],
    },
    "Should have overriden the parameters of the engine."
  );
});

add_task(async function test_engine_selector_override_experiments() {
  const { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "us",
    experiment: "experiment1",
  });

  let engine = engines.find(e => e.webExtension.id == "lycos@example.com");

  Assert.equal(
    engine.sendAttributionRequest,
    true,
    "Should have overriden the sendAttributionRequest field of the engine."
  );
});

add_task(async function test_engine_selector_override_with_included() {
  let { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "us",
    distroID: "distro2",
  });

  let engine = engines.find(e => e.webExtension.id == "altavista@example.com");
  Assert.ok(
    !("params" in engine),
    "Should not have overriden the parameters of the engine."
  );

  let result = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "gb",
    distroID: "distro2",
  });
  engine = result.engines.find(
    e => e.webExtension.id == "altavista@example.com"
  );
  Assert.deepEqual(
    engine.params,
    {
      searchUrlGetParams: [
        {
          name: "field-keywords2",
          value: "{searchTerms}",
        },
      ],
    },
    "Should have overriden the parameters of the engine."
  );
});
