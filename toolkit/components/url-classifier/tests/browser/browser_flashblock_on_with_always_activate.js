/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
requestLongerTimeout(3);

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Task.jsm");
const scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
scriptLoader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/url-classifier/tests/browser/classifierHelper.js",
  this);
scriptLoader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/url-classifier/tests/browser/classifierTester.js",
  this);

add_task(function* checkFlashBlockLists() {
  classifierTester.setPrefs({
    setDBs: true,
    flashBlockEnable: true,
    flashSetting: classifierTester.ALWAYS_ACTIVATE_PREF_VALUE
  });

  yield classifierHelper.waitForInit();
  yield classifierHelper.addUrlToDB(classifierTester.dbUrls);

  for (let testCase of classifierTester.testCases) {
    info(`RUNNING TEST: ${testCase.name} (Ask to Activate, Flashblock on)`);
    let tab = yield classifierTester.buildTestCaseInNewTab(gBrowser, testCase);

    let depth = testCase.domains.length - 1;
    let pluginInfo = yield classifierTester.getPluginInfo(tab.linkedBrowser,
                                                          depth);

    classifierTester.checkPluginInfo(pluginInfo,
                                     testCase.expectedFlashClassification,
                                     classifierTester.ALWAYS_ACTIVATE_PREF_VALUE);

    yield BrowserTestUtils.removeTab(tab);
  }
});
