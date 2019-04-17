/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "NetUtil",
                               "resource://gre/modules/NetUtil.jsm");

function MainProcessSingleton() {}
MainProcessSingleton.prototype = {
  classID: Components.ID("{0636a680-45cb-11e4-916c-0800200c9a66}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),

  // Called when a webpage calls window.external.AddSearchProvider
  addSearchEngine({ target: browser, data: { pageURL, engineURL } }) {
    pageURL = NetUtil.newURI(pageURL);
    engineURL = NetUtil.newURI(engineURL, null, pageURL);

    let iconURL;
    if (browser.mIconURL && browser.mIconURL.startsWith("data:"))
      iconURL = NetUtil.newURI(browser.mIconURL);

    try {
      // Make sure the URLs are HTTP, HTTPS, or FTP.
      let isWeb = ["https", "http", "ftp"];

      if (!isWeb.includes(engineURL.scheme))
        throw new Error("Unsupported search engine URL: " + engineURL.spec);

      if (Services.policies &&
          !Services.policies.isAllowed("installSearchEngine")) {
        throw new Error("Search Engine installation blocked by the Enterprise Policy Manager.");
      }
    } catch (ex) {
      Cu.reportError("Invalid argument passed to window.external.AddSearchProvider: " + ex);

      var searchBundle = Services.strings.createBundle("chrome://global/locale/search/search.properties");
      var brandBundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");
      var brandName = brandBundle.GetStringFromName("brandShortName");
      var title = searchBundle.GetStringFromName("error_invalid_format_title");
      var msg = searchBundle.formatStringFromName("error_invalid_engine_msg2",
                                                  [brandName, engineURL.spec], 2);
      Services.ww.getNewPrompter(browser.ownerGlobal).alert(title, msg);
      return;
    }

    Services.search.addEngine(engineURL.spec, iconURL ? iconURL.spec : null, true)
      .catch(ex => Cu.reportError("Unable to add search engine to the search service: " + ex));
  },

  observe(subject, topic, data) {
    switch (topic) {
    case "app-startup": {
      ChromeUtils.import("resource://gre/modules/CustomElementsListener.jsm", null);
      Services.obs.addObserver(this, "xpcom-shutdown");

      // Load this script early so that console.* is initialized
      // before other frame scripts.
      Services.mm.loadFrameScript("chrome://global/content/browser-content.js", true, true);
      Services.ppmm.loadProcessScript("chrome://global/content/process-content.js", true, true);
      Services.mm.addMessageListener("Search:AddEngine", this.addSearchEngine);
      Services.ppmm.loadProcessScript("resource:///modules/ContentObservers.js", true);
      break;
    }

    case "xpcom-shutdown":
      Services.mm.removeMessageListener("Search:AddEngine", this.addSearchEngine);
      break;
    }
  },
};

var EXPORTED_SYMBOLS = ["MainProcessSingleton"];
