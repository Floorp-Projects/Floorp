/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.sys.mjs",
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
      id: "lycos@example.com",
    },
    appliesTo: [
      {
        included: { everywhere: true },
        application: {
          name: ["firefox"],
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
          name: ["fenix"],
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
        application: {
          name: ["firefox"],
          minVersion: "10",
          maxVersion: "30",
        },
        default: "yes",
      },
    ],
  },
];

const engineSelector = new SearchEngineSelector();

const tests = [
  {
    name: "Firefox",
    version: "1",
    expected: ["lycos@example.com", "aol@example.com"],
  },
  {
    name: "Firefox",
    version: "20",
    expected: ["lycos@example.com", "aol@example.com", "excite@example.com"],
  },
  {
    name: "Fenix",
    version: "20",
    expected: ["aol@example.com", "altavista@example.com"],
  },
  {
    name: "Firefox",
    version: "31",
    expected: ["lycos@example.com", "aol@example.com"],
  },
  {
    name: "Firefox",
    version: "30",
    expected: ["lycos@example.com", "aol@example.com", "excite@example.com"],
  },
  {
    name: "Firefox",
    version: "10",
    expected: ["lycos@example.com", "aol@example.com", "excite@example.com"],
  },
];

add_task(async function setup() {
  await SearchTestUtils.useTestEngines("data", null, CONFIG);
  await AddonTestUtils.promiseStartupManager();

  let confUrl = `data:application/json,${JSON.stringify(CONFIG)}`;
  Services.prefs.setStringPref("search.config.url", confUrl);
});

add_task(async function test_application_name() {
  for (const { name, version, expected } of tests) {
    let { engines } = await engineSelector.fetchEngineConfiguration({
      locale: "default",
      region: "default",
      name,
      version,
    });
    const engineIds = engines.map(obj => obj.webExtension.id);
    Assert.deepEqual(
      engineIds,
      expected,
      `Should have the expected engines for app: "${name}"
      and version: "${version}"`
    );
  }
});
