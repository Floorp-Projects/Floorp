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

function fetchWithConfig(name, version) {
  Services.appinfo = { name, version };
  return engineSelector.fetchEngineConfiguration("default", "default");
}

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
  await useTestEngines("data", null, CONFIG);
  await AddonTestUtils.promiseStartupManager();

  let confUrl = `data:application/json,${JSON.stringify(CONFIG)}`;
  Services.prefs.setStringPref("search.config.url", confUrl);
});

add_task(async function test_application_name() {
  for (const { name, version, expected } of tests) {
    Services.appinfo = { name, version };
    let { engines } = await engineSelector.fetchEngineConfiguration(
      "default",
      "default"
    );
    const engineIds = engines.map(obj => obj.webExtension.id);
    Assert.deepEqual(
      engineIds,
      expected,
      `Should have the expected engines for app: "${name}"
      and version: "${version}"`
    );
  }
});
