/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchEngineSelectorOld:
    "resource://gre/modules/SearchEngineSelectorOld.sys.mjs",
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
        application: {
          channel: ["nightly"],
        },
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
        application: {
          channel: ["nightly", "esr"],
        },
      },
    ],
  },
  {
    webExtension: {
      id: "excite@example.com",
    },
    appliesTo: [
      {
        included: { everywhere: true },
      },
      {
        included: { everywhere: true },
        application: {
          channel: ["release"],
        },
        default: "yes",
      },
    ],
  },
];

const expectedEnginesPerChannel = {
  default: ["aol@example.com", "excite@example.com"],
  nightly: [
    "lycos@example.com",
    "aol@example.com",
    "altavista@example.com",
    "excite@example.com",
  ],
  beta: ["aol@example.com", "excite@example.com"],
  release: ["excite@example.com", "aol@example.com"],
  esr: ["aol@example.com", "altavista@example.com", "excite@example.com"],
};

const expectedDefaultEngine = {
  default: "aol@example.com",
  nightly: "lycos@example.com",
  beta: "aol@example.com",
  release: "excite@example.com",
  esr: "aol@example.com",
};

const engineSelector = new SearchEngineSelectorOld();

add_task(async function test_engine_selector_channels() {
  const settings = await RemoteSettings(SearchUtils.OLD_SETTINGS_KEY);
  sinon.stub(settings, "get").returns(TEST_CONFIG);

  for (let [channel, expected] of Object.entries(expectedEnginesPerChannel)) {
    const { engines } = await engineSelector.fetchEngineConfiguration({
      locale: "en-US",
      region: "us",
      channel,
    });

    const engineIds = engines.map(obj => obj.webExtension.id);
    Assert.deepEqual(
      engineIds,
      expected,
      `Should have the expected engines for channel "${channel}"`
    );

    Assert.equal(
      engineIds[0],
      expectedDefaultEngine[channel],
      `Should have the correct default for channel "${channel}"`
    );
  }
});
