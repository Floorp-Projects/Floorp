/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported teardown, setup, toggleExtension,
   spawnInNewReaderTab, TEST_ARTICLE  */

"use strict";

const TEST_ARTICLE = "http://example.com/browser/browser/base/content/test/" +
  "general/readerModeArticle.html";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");

const TEST_PREFS = [
  ["reader.parse-on-load.enabled", true],
  ["media.webspeech.synth.enabled", true],
  ["media.webspeech.synth.test", true],
  ["narrate.enabled", true],
  ["narrate.test", true]
];

function setup() {
  // Set required test prefs.
  TEST_PREFS.forEach(([name, value]) => {
    setBoolPref(name, value);
  });
}

function teardown() {
  // Reset test prefs.
  TEST_PREFS.forEach(pref => {
    clearUserPref(pref[0]);
  });
}

function spawnInNewReaderTab(url, func) {
  return BrowserTestUtils.withNewTab(
    { gBrowser,
      url: `about:reader?url=${encodeURIComponent(url)}` },
      function* (browser) {
        yield ContentTask.spawn(browser, null, function* () {
          Components.utils.import("chrome://mochitests/content/browser/" +
            "toolkit/components/narrate/test/NarrateTestUtils.jsm");

          yield NarrateTestUtils.getReaderReadyPromise(content);
        });

        yield ContentTask.spawn(browser, null, func);
      });
}

function setBoolPref(name, value) {
  Services.prefs.setBoolPref(name, value);
}

function clearUserPref(name) {
  Services.prefs.clearUserPref(name);
}

