/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
});

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

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

const engineSelector = new SearchEngineSelector();
add_task(async function setup() {
  await useTestEngines("data", null, CONFIG);
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_no_distribution_preference() {
  let { engines } = await engineSelector.fetchEngineConfiguration(
    "default",
    "default",
    "",
    ""
  );
  const engineIds = engines.map(obj => obj.webExtension.id);
  Assert.deepEqual(
    engineIds,
    ["aol@example.com", "excite@example.com", "altavista@example.com"],
    `Should have the expected engines for a normal build.`
  );
});

add_task(async function test_distribution_included() {
  let { engines } = await engineSelector.fetchEngineConfiguration(
    "default",
    "default",
    "",
    "cake"
  );
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
  let { engines } = await engineSelector.fetchEngineConfiguration(
    "default",
    "default",
    "",
    "apples"
  );
  const engineIds = engines.map(obj => obj.webExtension.id);
  Assert.deepEqual(
    engineIds,
    ["aol@example.com", "excite@example.com"],
    `Should have the expected engines for the "apples" distribution.`
  );
});
