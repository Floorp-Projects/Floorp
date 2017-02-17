/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
requestLongerTimeout(3);

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Task.jsm");
Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochitests/content/browser/toolkit/components/url-classifier/tests/browser/classifierHelper.js",
                 this);

const URL_PATH = "/browser/toolkit/components/url-classifier/tests/browser/flash_block_frame.html";
const OBJECT_ID = "testObject";
const IFRAME_ID = "testFrame";
const FLASHBLOCK_ENABLE_PREF = "plugins.flashBlock.enabled";

var dbUrls = [
  {
    url: "flashallow.example.com/",
    db: "test-flashallow-simple",
    pref: "urlclassifier.flashAllowTable"
  },
  {
    url: "exception.flashallow.example.com/",
    db: "testexcept-flashallow-simple",
    pref: "urlclassifier.flashAllowExceptTable"
  },
  {
    url: "flashblock.example.com/",
    db: "test-flash-simple",
    pref: "urlclassifier.flashTable"
  },
  {
    url: "exception.flashblock.example.com/",
    db: "testexcept-flash-simple",
    pref: "urlclassifier.flashExceptTable"
  },
  {
    url: "subdocument.example.com/",
    db: "test-flashsubdoc-simple",
    pref: "urlclassifier.flashThirdPartyTable"
  },
  {
    url: "exception.subdocument.example.com/",
    db: "testexcept-flashsubdoc-simple",
    pref: "urlclassifier.flashThirdPartyExceptTable"
  }
];

function setDBPrefs() {
  for (let dbData of dbUrls) {
    Services.prefs.setCharPref(dbData.pref, dbData.db);
  }
  Services.prefs.setBoolPref(FLASHBLOCK_ENABLE_PREF, true);
}

function unsetDBPrefs() {
  for (let dbData of dbUrls) {
    Services.prefs.clearUserPref(dbData.pref);
  }
  Services.prefs.clearUserPref(FLASHBLOCK_ENABLE_PREF);
}
registerCleanupFunction(unsetDBPrefs);

// The |domains| property describes the domains of the nested documents making
// up the page. |domains[0]| represents the domain in the URL bar. The last
// domain in the list is the domain of the most deeply nested iframe.
// Only the plugin in the most deeply nested document will be checked.
var testCases = [
  {
    name: "Unknown domain",
    domains: ["http://example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: true,
    expectedFlashClassification: "unknown"
  },
  {
    name: "Nested unknown domains",
    domains: ["http://example.com", "http://example.org"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: true,
    expectedFlashClassification: "unknown"
  },
  {
    name: "Allowed domain",
    domains: ["http://flashallow.example.com"],
    expectedActivated: true,
    expectedHasRunningPlugin: true,
    pluginListed: true,
    expectedFlashClassification: "allowed"
  },
  {
    name: "Allowed nested domain",
    domains: ["http://example.com", "http://flashallow.example.com"],
    expectedActivated: true,
    expectedHasRunningPlugin: true,
    pluginListed: true,
    expectedFlashClassification: "allowed"
  },
  {
    name: "Subdocument of allowed domain",
    domains: ["http://flashallow.example.com", "http://example.com"],
    expectedActivated: true,
    expectedHasRunningPlugin: true,
    pluginListed: true,
    expectedFlashClassification: "allowed"
  },
  {
    name: "Exception to allowed domain",
    domains: ["http://exception.flashallow.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: true,
    expectedFlashClassification: "unknown"
  },
  {
    name: "Blocked domain",
    domains: ["http://flashblock.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_USER_DISABLED,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: false,
    expectedFlashClassification: "denied"
  },
  {
    name: "Nested blocked domain",
    domains: ["http://example.com", "http://flashblock.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_USER_DISABLED,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: false,
    expectedFlashClassification: "denied"
  },
  {
    name: "Subdocument of blocked subdocument",
    domains: ["http://example.com", "http://flashblock.example.com", "http://example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_USER_DISABLED,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: false,
    expectedFlashClassification: "denied"
  },
  {
    name: "Blocked subdocument nested among in allowed documents",
    domains: ["http://flashallow.example.com", "http://flashblock.example.com", "http://flashallow.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_USER_DISABLED,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: false,
    expectedFlashClassification: "denied"
  },
  {
    name: "Exception to blocked domain",
    domains: ["http://exception.flashblock.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: true,
    expectedFlashClassification: "unknown"
  },
  {
    name: "Sub-document blocked domain in top-level context",
    domains: ["http://subdocument.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: true,
    expectedFlashClassification: "unknown"
  },
  {
    name: "Sub-document blocked domain",
    domains: ["http://example.com", "http://subdocument.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_USER_DISABLED,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: false,
    expectedFlashClassification: "denied"
  },
  {
    name: "Sub-document blocked domain in non-Third-Party context",
    domains: ["http://subdocument.example.com", "http://subdocument.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: true,
    expectedFlashClassification: "unknown"
  },
  {
    name: "Sub-document blocked domain differing only by scheme",
    domains: ["http://subdocument.example.com", "https://subdocument.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_USER_DISABLED,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: false,
    expectedFlashClassification: "denied"
  },
  {
    name: "Sub-document blocked subdocument of an allowed domain",
    domains: ["http://flashallow.example.com", "http://subdocument.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_USER_DISABLED,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: false,
    expectedFlashClassification: "denied"
  },
  {
    name: "Subdocument of Sub-document blocked domain",
    domains: ["http://example.com", "http://subdocument.example.com", "http://example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_USER_DISABLED,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: false,
    expectedFlashClassification: "denied"
  },
  {
    name: "Sub-document exception in top-level context",
    domains: ["http://exception.subdocument.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: true,
    expectedFlashClassification: "unknown"
  },
  {
    name: "Sub-document blocked domain exception",
    domains: ["http://example.com", "http://exception.subdocument.example.com"],
    expectedPluginFallbackType: Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY,
    expectedActivated: false,
    expectedHasRunningPlugin: false,
    pluginListed: true,
    expectedFlashClassification: "unknown"
  }
];

function buildDocumentStructure(browser, iframeDomains) {
  return Task.spawn(function* () {
    let depth = 0;
    for (let domain of iframeDomains) {
      // Firefox does not like to load the same page in its own iframe. Put some
      // bogus query strings in the URL to make it happy.
      let url = domain + URL_PATH + "?date=" + Date.now() + "rand=" + Math.random();
      let domainLoaded = BrowserTestUtils.browserLoaded(browser, true, url);

      ContentTask.spawn(browser, {iframeId: IFRAME_ID, url: url, depth: depth},
                        function*({iframeId, url, depth}) {
        let doc = content.document;
        for (let i = 0; i < depth; ++i) {
          doc = doc.getElementById(iframeId).contentDocument;
        }
        doc.getElementById(iframeId).src = url;
      });

      yield domainLoaded;
      ++depth;
    }
  });
}

function getPluginInfo(browser, depth) {
  return ContentTask.spawn(browser,
                           {iframeId: IFRAME_ID, depth: depth},
                           function* ({iframeId, depth}) {
    let doc = content.document;
    let win = content.window;
    for (let i = 0; i < depth; ++i) {
      let frame = doc.getElementById(iframeId);
      doc = frame.contentDocument;
      win = frame.contentWindow;
    }

    let pluginObj = doc.getElementById("testObject");
    if (!(pluginObj instanceof Ci.nsIObjectLoadingContent)) {
      throw new Error("Unable to find plugin!");
    }
    return {
      pluginFallbackType: pluginObj.pluginFallbackType,
      activated: pluginObj.activated,
      hasRunningPlugin: pluginObj.hasRunningPlugin,
      listed: ("Shockwave Flash" in win.navigator.plugins),
      flashClassification: doc.documentFlashClassification
    };
  });
}

add_task(function* checkFlashBlockLists() {
  setDBPrefs();

  yield classifierHelper.waitForInit();
  yield classifierHelper.addUrlToDB(dbUrls);

  for (let testCase of testCases) {
    info(`RUNNING TEST: ${testCase.name}`);

    let iframeDomains = testCase.domains.slice();
    let pageDomain = iframeDomains.shift();
    let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser,
                                                          pageDomain + URL_PATH);

    yield buildDocumentStructure(tab.linkedBrowser, iframeDomains);

    let pluginInfo = yield getPluginInfo(tab.linkedBrowser, iframeDomains.length);

    if ("expectedPluginFallbackType" in testCase) {
      is(pluginInfo.pluginFallbackType, testCase.expectedPluginFallbackType,
        "Plugin should have the correct fallback type");
    }
    if ("expectedActivated" in testCase) {
      is(pluginInfo.activated, testCase.expectedActivated,
        "Plugin should have the correct activation");
    }
    if ("expectedHasRunningPlugin" in testCase) {
      is(pluginInfo.hasRunningPlugin, testCase.expectedHasRunningPlugin,
        "Plugin should have the correct 'plugin running' state");
    }
    if ("pluginListed" in testCase) {
      is(pluginInfo.listed, testCase.pluginListed,
        "Plugin's existance in navigator.plugins should match expected")
    }
    if ("expectedFlashClassification" in testCase) {
      is(pluginInfo.flashClassification, testCase.expectedFlashClassification,
        "Page's classification should match expected");
    }

    yield BrowserTestUtils.removeTab(tab);
  }
});

add_task(function* checkFlashBlockDisabled() {
  setDBPrefs();
  Services.prefs.setBoolPref(FLASHBLOCK_ENABLE_PREF, false);

  yield classifierHelper.waitForInit();
  yield classifierHelper.addUrlToDB(dbUrls);

  for (let testCase of testCases) {
    info(`RUNNING TEST: ${testCase.name} (flashblock disabled)`);

    let iframeDomains = testCase.domains.slice();
    let pageDomain = iframeDomains.shift();
    let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser,
                                                          pageDomain + URL_PATH);

    yield buildDocumentStructure(tab.linkedBrowser, iframeDomains);

    let pluginInfo = yield getPluginInfo(tab.linkedBrowser, iframeDomains.length);

    // With flashblock disabled, all plugins should be activated.
    ok(pluginInfo.activated, "Plugin should be activated");
    ok(pluginInfo.hasRunningPlugin, "Plugin should be running");
    ok(pluginInfo.listed, "Flash should be listed in navigator.plugins");
    is(pluginInfo.flashClassification, "allowed",
       "Page's classification should be 'allowed'");

    yield BrowserTestUtils.removeTab(tab);
  }
});
