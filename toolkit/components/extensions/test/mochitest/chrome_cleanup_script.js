"use strict";

/* global addMessageListener, sendAsyncMessage */

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

let listener = msg => {
  void (msg instanceof Ci.nsIConsoleMessage);
  dump(`Console message: ${msg}\n`);
};

Services.console.registerListener(listener);

let getBrowserApp, getTabBrowser;
if (AppConstants.MOZ_BUILD_APP === "mobile/android") {
  getBrowserApp = win => win.BrowserApp;
  getTabBrowser = tab => tab.browser;
} else {
  getBrowserApp = win => win.gBrowser;
  getTabBrowser = tab => tab.linkedBrowser;
}

function* iterBrowserWindows() {
  for (let win of Services.wm.getEnumerator("navigator:browser")) {
    if (!win.closed && getBrowserApp(win)) {
      yield win;
    }
  }
}

let initialTabs = new Map();
for (let win of iterBrowserWindows()) {
  initialTabs.set(win, new Set(getBrowserApp(win).tabs));
}

addMessageListener("check-cleanup", extensionId => {
  Services.console.unregisterListener(listener);

  let results = {
    extraWindows: [],
    extraTabs: [],
  };

  for (let win of iterBrowserWindows()) {
    if (initialTabs.has(win)) {
      let tabs = initialTabs.get(win);

      for (let tab of getBrowserApp(win).tabs) {
        if (!tabs.has(tab)) {
          results.extraTabs.push(getTabBrowser(tab).currentURI.spec);
        }
      }
    } else {
      results.extraWindows.push(
        Array.from(win.gBrowser.tabs, tab => getTabBrowser(tab).currentURI.spec)
      );
    }
  }

  initialTabs = null;

  sendAsyncMessage("cleanup-results", results);
});
