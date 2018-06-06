/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* This is a JavaScript module (JSM) to be imported via
   Components.utils.import() and acts as a singleton.
   Only the following listed symbols will exposed on import, and only when
   and where imported. */

const EXPORTED_SYMBOLS = ["BrowserTabs"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://services-sync/main.js");
ChromeUtils.import("resource:///modules/sessionstore/TabStateFlusher.jsm");
ChromeUtils.import("resource://tps/logger.jsm");

// Unfortunately, due to where TPS is run, we can't directly reuse the logic from
// BrowserTestUtils.jsm. Moreover, we can't resolve the URI it loads the content
// frame script from ("chrome://mochikit/content/tests/BrowserTestUtils/content-utils.js"),
// hence the hackiness here and in BrowserTabs.Add.
Services
.mm
.loadFrameScript("data:application/javascript;charset=utf-8," + encodeURIComponent(`
  Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
  addEventListener("load", function(event) {
    let subframe = event.target != content.document;
    sendAsyncMessage("tps:loadEvent", {subframe: subframe, url: event.target.documentURI});
  }, true)`), true);

var BrowserTabs = {
  /**
   * Add
   *
   * Opens a new tab in the current browser window for the
   * given uri. Rejects on error.
   *
   * @param uri The uri to load in the new tab
   * @return Promise
   */
  async Add(uri) {
    let mainWindow = Services.wm.getMostRecentWindow("navigator:browser");
    let browser = mainWindow.getBrowser();
    let newtab = browser.addTab(uri);

    // Wait for the tab to load.
    await new Promise(resolve => {
      let mm = browser.ownerGlobal.messageManager;
      mm.addMessageListener("tps:loadEvent", function onLoad(msg) {
        mm.removeMessageListener("tps:loadEvent", onLoad);
        resolve();
      });
    });

    browser.selectedTab = newtab;
    // We might sync before SessionStore is done recording information, so try
    // and force it to record everything. This is overkill, but effective.
    await TabStateFlusher.flushWindow(mainWindow);
  },

  /**
   * Find
   *
   * Finds the specified uri and title in Weave's list of remote tabs
   * for the specified profile.
   *
   * @param uri The uri of the tab to find
   * @param title The page title of the tab to find
   * @param profile The profile to search for tabs
   * @return true if the specified tab could be found, otherwise false
   */
  Find(uri, title, profile) {
    // Find the uri in Weave's list of tabs for the given profile.
    let engine = Weave.Service.engineManager.get("tabs");
    for (let [, client] of Object.entries(engine.getAllClients())) {
      if (!client.tabs) {
        continue;
      }
      for (let key in client.tabs) {
        let tab = client.tabs[key];
        let weaveTabUrl = tab.urlHistory[0];
        if (uri == weaveTabUrl && profile == client.clientName)
          if (title == undefined || title == tab.title)
            return true;
        }
        Logger.logInfo(`Dumping tabs for ${client.clientName}...\n` + JSON.stringify(client.tabs));
      }
    return false;
  },
};
