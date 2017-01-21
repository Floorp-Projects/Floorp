"use strict";

/* global addMessageListener, sendAsyncMessage */

Components.utils.import("resource://gre/modules/Services.jsm");

function* iterBrowserWindows() {
  let enm = Services.wm.getEnumerator("navigator:browser");
  while (enm.hasMoreElements()) {
    let win = enm.getNext();
    if (!win.closed && win.gBrowser) {
      yield win;
    }
  }
}

let initialTabs = new Map();
for (let win of iterBrowserWindows()) {
  initialTabs.set(win, new Set(win.gBrowser.tabs));
}

addMessageListener("check-cleanup", extensionId => {
  let results = {
    extraWindows: [],
    extraTabs: [],
  };

  for (let win of iterBrowserWindows()) {
    if (initialTabs.has(win)) {
      let tabs = initialTabs.get(win);

      for (let tab of win.gBrowser.tabs) {
        if (!tabs.has(tab)) {
          results.extraTabs.push(tab.linkedBrowser.currentURI.spec);
        }
      }
    } else {
      results.extraWindows.push(
        Array.from(win.gBrowser.tabs,
                   tab => tab.linkedBrowser.currentURI.spec));
    }
  }

  initialTabs = null;

  sendAsyncMessage("cleanup-results", results);
});
