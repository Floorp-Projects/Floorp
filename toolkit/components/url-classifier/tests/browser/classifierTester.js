/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var classifierTester = {
  URL_PATH: "/browser/toolkit/components/url-classifier/tests/browser/flash_block_frame.html",
  OBJECT_ID: "testObject",
  IFRAME_ID: "testFrame",
  FLASHBLOCK_ENABLE_PREF: "plugins.flashBlock.enabled",
  FLASH_PLUGIN_USER_SETTING_PREF: "plugin.state.flash",
  URLCLASSIFIER_DISALLOW_COMPLETIONS_PREF: "urlclassifier.disallow_completions",
  NEVER_ACTIVATE_PREF_VALUE: 0,
  ASK_TO_ACTIVATE_PREF_VALUE: 1,
  ALWAYS_ACTIVATE_PREF_VALUE: 2,
  ALLOW_CTA_PREF: "plugins.click_to_play",

  dbUrls: [
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
      pref: "urlclassifier.flashSubDocTable"
    },
    {
      url: "exception.subdocument.example.com/",
      db: "testexcept-flashsubdoc-simple",
      pref: "urlclassifier.flashSubDocExceptTable"
    }
  ],

  setPrefs: function ({setDBs = true, flashBlockEnable = true, flashSetting = classifierTester.ALWAYS_ACTIVATE_PREF_VALUE} = {}) {
    if (setDBs) {
      let DBs = [];

      for (let dbData of classifierTester.dbUrls) {
        Services.prefs.setCharPref(dbData.pref, dbData.db);
        DBs.push(dbData.db);
      }

      let completions = Services.prefs.getCharPref(classifierTester.URLCLASSIFIER_DISALLOW_COMPLETIONS_PREF);
      completions += "," + DBs.join(",");
      Services.prefs.setCharPref(classifierTester.URLCLASSIFIER_DISALLOW_COMPLETIONS_PREF, completions);
    }

    Services.prefs.setBoolPref(classifierTester.FLASHBLOCK_ENABLE_PREF,
                               flashBlockEnable);
    Services.prefs.setIntPref(classifierTester.FLASH_PLUGIN_USER_SETTING_PREF,
                              flashSetting);
    Services.prefs.setBoolPref(classifierTester.ALLOW_CTA_PREF, true);
  },

  unsetPrefs: function () {
    for (let dbData of classifierTester.dbUrls) {
      Services.prefs.clearUserPref(dbData.pref);
    }

    Services.prefs.clearUserPref(classifierTester.URLCLASSIFIER_DISALLOW_COMPLETIONS_PREF);
    Services.prefs.clearUserPref(classifierTester.FLASHBLOCK_ENABLE_PREF);
    Services.prefs.clearUserPref(classifierTester.FLASH_PLUGIN_USER_SETTING_PREF);
    Services.prefs.clearUserPref(classifierTester.ALLOW_CTA_PREF);
  },

  // The |domains| property describes the domains of the nested documents making
  // up the page. |domains[0]| represents the domain in the URL bar. The last
  // domain in the list is the domain of the most deeply nested iframe.
  // Only the plugin in the most deeply nested document will be checked.
  testCases: [
    {
      name: "Unknown domain",
      domains: ["http://example.com"],
      expectedFlashClassification: "unknown"
    },
    {
      name: "Nested unknown domains",
      domains: ["http://example.com", "http://example.org"],
      expectedFlashClassification: "unknown"
    },
    {
      name: "Allowed domain",
      domains: ["http://flashallow.example.com"],
      expectedFlashClassification: "allowed"
    },
    {
      name: "Allowed nested domain",
      domains: ["http://example.com", "http://flashallow.example.com"],
      expectedFlashClassification: "allowed"
    },
    {
      name: "Subdocument of allowed domain",
      domains: ["http://flashallow.example.com", "http://example.com"],
      expectedFlashClassification: "allowed"
    },
    {
      name: "Exception to allowed domain",
      domains: ["http://exception.flashallow.example.com"],
      expectedFlashClassification: "unknown"
    },
    {
      name: "Blocked domain",
      domains: ["http://flashblock.example.com"],
      expectedFlashClassification: "denied"
    },
    {
      name: "Nested blocked domain",
      domains: ["http://example.com", "http://flashblock.example.com"],
      expectedFlashClassification: "denied"
    },
    {
      name: "Subdocument of blocked subdocument",
      domains: ["http://example.com", "http://flashblock.example.com", "http://example.com"],
      expectedFlashClassification: "denied"
    },
    {
      name: "Blocked subdocument nested among in allowed documents",
      domains: ["http://flashallow.example.com", "http://flashblock.example.com", "http://flashallow.example.com"],
      expectedFlashClassification: "denied"
    },
    {
      name: "Exception to blocked domain",
      domains: ["http://exception.flashblock.example.com"],
      expectedFlashClassification: "unknown"
    },
    {
      name: "Sub-document blocked domain in top-level context",
      domains: ["http://subdocument.example.com"],
      expectedFlashClassification: "unknown"
    },
    {
      name: "Sub-document blocked domain",
      domains: ["http://example.com", "http://subdocument.example.com"],
      expectedFlashClassification: "denied"
    },
    {
      name: "Sub-document blocked domain in non-Third-Party context",
      domains: ["http://subdocument.example.com", "http://subdocument.example.com"],
      expectedFlashClassification: "unknown"
    },
    {
      name: "Sub-document blocked domain differing only by scheme",
      domains: ["http://subdocument.example.com", "https://subdocument.example.com"],
      expectedFlashClassification: "denied"
    },
    {
      name: "Sub-document blocked subdocument of an allowed domain",
      domains: ["http://flashallow.example.com", "http://subdocument.example.com"],
      expectedFlashClassification: "denied"
    },
    {
      name: "Subdocument of Sub-document blocked domain",
      domains: ["http://example.com", "http://subdocument.example.com", "http://example.com"],
      expectedFlashClassification: "denied"
    },
    {
      name: "Sub-document exception in top-level context",
      domains: ["http://exception.subdocument.example.com"],
      expectedFlashClassification: "unknown"
    },
    {
      name: "Sub-document blocked domain exception",
      domains: ["http://example.com", "http://exception.subdocument.example.com"],
      expectedFlashClassification: "unknown"
    }
  ],

  // Returns null if this value should not be verified given the combination
  // of inputs
  expectedPluginFallbackType: function (classification, flashSetting) {
    switch(classification) {
      case "unknown":
        if (flashSetting == classifierTester.ALWAYS_ACTIVATE_PREF_VALUE) {
          return null;
        } else if (flashSetting == classifierTester.ASK_TO_ACTIVATE_PREF_VALUE) {
          return Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY;
        } else if (flashSetting == classifierTester.NEVER_ACTIVATE_PREF_VALUE) {
          return Ci.nsIObjectLoadingContent.PLUGIN_DISABLED;
        }
        break;
      case "allowed":
        if (flashSetting == classifierTester.NEVER_ACTIVATE_PREF_VALUE) {
          return Ci.nsIObjectLoadingContent.PLUGIN_DISABLED;
        }
        return null;
      case "denied":
        return Ci.nsIObjectLoadingContent.PLUGIN_USER_DISABLED;
    }
    throw new Error("Invalid classification or flash setting");
  },

  // Returns null if this value should not be verified given the combination
  // of inputs
  expectedActivated: function (classification, flashSetting) {
    switch(classification) {
      case "unknown":
        return (flashSetting == classifierTester.ALWAYS_ACTIVATE_PREF_VALUE);
      case "allowed":
        return (flashSetting != classifierTester.NEVER_ACTIVATE_PREF_VALUE);
      case "denied":
        return false;
    }
    throw new Error("Invalid classification or flash setting");
  },

  // Returns null if this value should not be verified given the combination
  // of inputs
  expectedHasRunningPlugin: function (classification, flashSetting) {
    switch(classification) {
      case "unknown":
        return (flashSetting == classifierTester.ALWAYS_ACTIVATE_PREF_VALUE);
      case "allowed":
        return (flashSetting != classifierTester.NEVER_ACTIVATE_PREF_VALUE);
      case "denied":
        return false;
    }
    throw new Error("Invalid classification or flash setting");
  },

  // Returns null if this value should not be verified given the combination
  // of inputs
  expectedPluginListed: function (classification, flashSetting) {
    if (flashSetting == classifierTester.ASK_TO_ACTIVATE_PREF_VALUE &&
        Services.prefs.getCharPref('plugins.navigator.hidden_ctp_plugin') == "Shockwave Flash") {
      return false;
    }
    switch(classification) {
      case "unknown":
      case "allowed":
        return (flashSetting != classifierTester.NEVER_ACTIVATE_PREF_VALUE);
      case "denied":
        return false;
    }
    throw new Error("Invalid classification or flash setting");
  },

  buildTestCaseInNewTab: function (browser, testCase) {
    return (async function() {
      let iframeDomains = testCase.domains.slice();
      let pageDomain = iframeDomains.shift();
      let tab = await BrowserTestUtils.openNewForegroundTab(browser,
                                                            pageDomain + classifierTester.URL_PATH);

      let depth = 0;
      for (let domain of iframeDomains) {
        // Firefox does not like to load the same page in its own iframe. Put some
        // bogus query strings in the URL to make it happy.
        let url = domain + classifierTester.URL_PATH + "?date=" + Date.now() + "rand=" + Math.random();
        let domainLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, true, url);

        ContentTask.spawn(tab.linkedBrowser, {iframeId: classifierTester.IFRAME_ID, url: url, depth: depth},
                          async function({iframeId, url, depth}) {
          let doc = content.document;
          for (let i = 0; i < depth; ++i) {
            doc = doc.getElementById(iframeId).contentDocument;
          }
          doc.getElementById(iframeId).src = url;
        });

        await domainLoaded;
        ++depth;
      }
      return tab;
    })();
  },

  getPluginInfo: function (browser, depth) {
    return ContentTask.spawn(browser,
                             {iframeId: classifierTester.IFRAME_ID, depth: depth},
                             async function({iframeId, depth}) {
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
  },

  checkPluginInfo: function (pluginInfo, expectedClassification, flashSetting) {
    is(pluginInfo.flashClassification, expectedClassification,
       "Page's classification should match expected");

    let expectedPluginFallbackType =
      classifierTester.expectedPluginFallbackType(pluginInfo.flashClassification,
                                                  flashSetting);
    if (expectedPluginFallbackType != null) {
      is(pluginInfo.pluginFallbackType, expectedPluginFallbackType,
         "Plugin should have the correct fallback type");
    }

    let expectedActivated =
      classifierTester.expectedActivated(pluginInfo.flashClassification,
                                         flashSetting);
    if (expectedActivated != null) {
      is(pluginInfo.activated, expectedActivated,
         "Plugin should have the correct activation");
    }

    let expectedHasRunningPlugin =
      classifierTester.expectedHasRunningPlugin(pluginInfo.flashClassification,
                                                flashSetting);
    if (expectedHasRunningPlugin != null) {
      is(pluginInfo.hasRunningPlugin, expectedHasRunningPlugin,
         "Plugin should have the correct 'plugin running' state");
    }

    let expectedPluginListed =
      classifierTester.expectedPluginListed(pluginInfo.flashClassification,
                                            flashSetting);
    if (expectedPluginListed != null) {
      is(pluginInfo.listed, expectedPluginListed,
         "Plugin's existance in navigator.plugins should match expected");
    }
  }
};
registerCleanupFunction(classifierTester.unsetPrefs);
