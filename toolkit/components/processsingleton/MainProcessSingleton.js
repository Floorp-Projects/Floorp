/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu, interfaces: Ci, classes: Cc, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

function MainProcessSingleton() {}
MainProcessSingleton.prototype = {
  classID: Components.ID("{0636a680-45cb-11e4-916c-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  logConsoleMessage(message) {
    let logMsg = message.data;
    logMsg.wrappedJSObject = logMsg;
    Services.obs.notifyObservers(logMsg, "console-api-log-event", null);
  },

  // Called when a webpage calls window.external.AddSearchProvider
  addSearchEngine({ target: browser, data: { pageURL, engineURL } }) {
    pageURL = NetUtil.newURI(pageURL);
    engineURL = NetUtil.newURI(engineURL, null, pageURL);

    let iconURL;
    let tabbrowser = browser.getTabBrowser();
    if (browser.mIconURL && (!tabbrowser || tabbrowser.shouldLoadFavIcon(pageURL)))
      iconURL = NetUtil.newURI(browser.mIconURL);

    try {
      // Make sure the URLs are HTTP, HTTPS, or FTP.
      let isWeb = ["https", "http", "ftp"];

      if (isWeb.indexOf(engineURL.scheme) < 0)
        throw "Unsupported search engine URL: " + engineURL;

      if (iconURL && isWeb.indexOf(iconURL.scheme) < 0)
        throw "Unsupported search icon URL: " + iconURL;
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

    Services.search.init(function(status) {
      if (status != Cr.NS_OK)
        return;

      Services.search.addEngine(engineURL.spec, null, iconURL ? iconURL.spec : null, true);
    })
  },

  observe(subject, topic, data) {
    switch (topic) {
    case "app-startup": {
      Services.obs.addObserver(this, "xpcom-shutdown", false);

      // Load this script early so that console.* is initialized
      // before other frame scripts.
      Services.mm.loadFrameScript("chrome://global/content/browser-content.js", true);
      Services.ppmm.loadProcessScript("chrome://global/content/process-content.js", true);
      Services.ppmm.addMessageListener("Console:Log", this.logConsoleMessage);
      Services.mm.addMessageListener("Search:AddEngine", this.addSearchEngine);
      Services.ppmm.loadProcessScript("resource:///modules/ContentObservers.js", true);
      break;
    }

    case "xpcom-shutdown":
      Services.ppmm.removeMessageListener("Console:Log", this.logConsoleMessage);
      Services.mm.removeMessageListener("Search:AddEngine", this.addSearchEngine);
      break;
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([MainProcessSingleton]);
