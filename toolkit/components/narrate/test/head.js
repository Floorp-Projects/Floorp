/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* exported teardown, setup, toggleExtension,
   spawnInNewReaderTab, TEST_ARTICLE, TEST_ITALIAN_ARTICLE  */

"use strict";

const TEST_ARTICLE =
  "http://example.com/browser/toolkit/components/narrate/test/moby_dick.html";

const TEST_ITALIAN_ARTICLE =
  "http://example.com/browser/toolkit/components/narrate/test/inferno.html";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");

const TEST_PREFS = {
  "reader.parse-on-load.enabled": true,
  "media.webspeech.synth.enabled": true,
  "media.webspeech.synth.test": true,
  "narrate.enabled": true,
  "narrate.test": true,
  "narrate.voice": null,
  "narrate.filter-voices": false,
};

function setup(voiceUri = "automatic", filterVoices = false) {
  let prefs = Object.assign({}, TEST_PREFS, {
    "narrate.filter-voices": filterVoices,
    "narrate.voice": JSON.stringify({ en: voiceUri })
  });

  // Set required test prefs.
  Object.entries(prefs).forEach(([name, value]) => {
    switch (typeof value) {
      case "boolean":
        setBoolPref(name, value);
        break;
      case "string":
        setCharPref(name, value);
        break;
    }
  });
}

function teardown() {
  // Reset test prefs.
  Object.entries(TEST_PREFS).forEach(pref => {
    clearUserPref(pref[0]);
  });
}

function spawnInNewReaderTab(url, func) {
  return BrowserTestUtils.withNewTab(
    { gBrowser,
      url: `about:reader?url=${encodeURIComponent(url)}` },
      async function(browser) {
        await ContentTask.spawn(browser, null, async function() {
          // This imports the test utils for all tests, so we'll declare it as
          // a global here which will make it ESLint happy.
          /* global NarrateTestUtils */
          Components.utils.import("chrome://mochitests/content/browser/" +
            "toolkit/components/narrate/test/NarrateTestUtils.jsm");

          await NarrateTestUtils.getReaderReadyPromise(content);
        });

        await ContentTask.spawn(browser, null, func);
      });
}

function setBoolPref(name, value) {
  Services.prefs.setBoolPref(name, value);
}

function setCharPref(name, value) {
  Services.prefs.setCharPref(name, value);
}

function clearUserPref(name) {
  Services.prefs.clearUserPref(name);
}
