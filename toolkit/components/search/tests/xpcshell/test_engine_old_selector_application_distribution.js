/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchEngineSelectorOld:
    "resource://gre/modules/SearchEngineSelectorOld.sys.mjs",
});

const CONFIG = [
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
      id: "excite@example.com",
    },
    appliesTo: [
      {
        included: { everywhere: true },
        // Test with a application/distributions section present but an
        // empty list.
        application: {
          distributions: [],
        },
      },
    ],
  },
  {
    webExtension: {
      id: "lycos@example.com",
    },
    appliesTo: [
      {
        included: { everywhere: true },
        application: {
          distributions: ["cake"],
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
          excludedDistributions: ["apples"],
        },
      },
    ],
  },
];

const engineSelector = new SearchEngineSelectorOld();
add_setup(async function () {
  Services.prefs.setBoolPref("browser.search.newSearchConfig.enabled", false);
  await SearchTestUtils.useTestEngines("data", null, CONFIG);
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_no_distribution_preference() {
  let { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "default",
    region: "default",
    channel: "",
    distroID: "",
  });
  const engineIds = engines.map(obj => obj.webExtension.id);
  Assert.deepEqual(
    engineIds,
    ["aol@example.com", "excite@example.com", "altavista@example.com"],
    `Should have the expected engines for a normal build.`
  );
});

add_task(async function test_distribution_included() {
  let { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "default",
    region: "default",
    channel: "",
    distroID: "cake",
  });
  const engineIds = engines.map(obj => obj.webExtension.id);
  Assert.deepEqual(
    engineIds,
    [
      "lycos@example.com",
      "aol@example.com",
      "excite@example.com",
      "altavista@example.com",
    ],
    `Should have the expected engines for the "cake" distribution.`
  );
});

add_task(async function test_distribution_excluded() {
  let { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "default",
    region: "default",
    channel: "",
    distroID: "apples",
  });
  const engineIds = engines.map(obj => obj.webExtension.id);
  Assert.deepEqual(
    engineIds,
    ["aol@example.com", "excite@example.com"],
    `Should have the expected engines for the "apples" distribution.`
  );
});
