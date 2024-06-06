/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Simplified version of browser_urlQueryStripping.js to test that the Nimbus
 * integration works correctly in both normal and private browsing.
 */

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

const TEST_URI = TEST_DOMAIN + TEST_PATH + "file_stripping.html";
const TEST_QUERY_STRING = "paramToStrip1=123&paramToKeep=456";
const TEST_QUERY_STRING_STRIPPED = "paramToKeep=456";
const TEST_URI_WITH_QUERY = TEST_URI + "?" + TEST_QUERY_STRING;

let listService;

async function waitForListServiceInit(strippingEnabled) {
  info("Waiting for nsIURLQueryStrippingListService to be initialized.");
  let isInitialized = await listService.testWaitForInit();
  is(
    isInitialized,
    strippingEnabled,
    "nsIURLQueryStrippingListService should be initialized when the feature is enabled."
  );
}

/**
 * Set a list of prefs on the default branch and restore the original values on test end.
 * @param {*} prefs - Key value pairs in an array.
 */
function setDefaultPrefs(prefs) {
  let originalValues = new Map();
  let defaultPrefs = Services.prefs.getDefaultBranch("");

  let prefValueToSetter = prefValue => {
    let type = typeof prefValue;
    if (type == "string") {
      return defaultPrefs.setStringPref;
    }
    if (type == "boolean") {
      return defaultPrefs.setBoolPref;
    }
    throw new Error("unexpected pref type");
  };

  prefs.forEach(([key, value]) => {
    prefValueToSetter(value)(key, value);
    originalValues.set(key, value);
  });

  registerCleanupFunction(function () {
    prefs.forEach(([key, value]) => {
      prefValueToSetter(value)(key, originalValues.get(key));
    });
  });
}

add_setup(async function () {
  // Disable the feature via the default pref. This is required so we can set
  // user values via Nimbus.
  setDefaultPrefs([
    ["privacy.query_stripping.enabled", false],
    ["privacy.query_stripping.enabled.pbmode", false],
    ["privacy.query_stripping.strip_list", ""],
    ["privacy.query_stripping.strip_on_share.enabled", false],
  ]);

  await SpecialPowers.pushPrefEnv({
    set: [["privacy.query_stripping.listService.logLevel", "Debug"]],
  });

  // Get the list service so we can wait for it to be fully initialized before running tests.
  listService = Cc["@mozilla.org/query-stripping-list-service;1"].getService(
    Ci.nsIURLQueryStrippingListService
  );
  // Here we don't care about the actual enabled state, we just want any init to be done so we get reliable starting conditions.
  await listService.testWaitForInit();
});

add_task(async function test() {
  let [normalWindow, pbWindow] = await Promise.all([
    BrowserTestUtils.openNewBrowserWindow(),
    BrowserTestUtils.openNewBrowserWindow({ private: true }),
  ]);

  for (let enableStripPBM of [false, true]) {
    for (let enableStrip of [false, true]) {
      let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
        featureId: "queryStripping",
        value: {
          enabledNormalBrowsing: enableStrip,
          enabledPrivateBrowsing: enableStripPBM,
          stripList: "paramToStrip1 paramToStrip2",
        },
      });

      for (let testPBM of [false, true]) {
        let shouldStrip =
          (testPBM && enableStripPBM) || (!testPBM && enableStrip);
        let expectedQueryString = shouldStrip
          ? TEST_QUERY_STRING_STRIPPED
          : TEST_QUERY_STRING;

        info(
          "Test stripping " +
            JSON.stringify({
              enableStripPBM,
              enableStrip,
              testPBM,
              expectedQueryString,
            })
        );

        await waitForListServiceInit(enableStripPBM || enableStrip);

        let tabBrowser = testPBM ? pbWindow.gBrowser : normalWindow.gBrowser;
        await BrowserTestUtils.withNewTab(
          { gBrowser: tabBrowser, url: TEST_URI_WITH_QUERY },
          async browser => {
            is(
              browser.currentURI.query,
              expectedQueryString,
              "Correct query string"
            );
          }
        );
      }

      doExperimentCleanup();
    }
  }

  // Cleanup
  await Promise.all([
    BrowserTestUtils.closeWindow(normalWindow),
    BrowserTestUtils.closeWindow(pbWindow),
  ]);
});
