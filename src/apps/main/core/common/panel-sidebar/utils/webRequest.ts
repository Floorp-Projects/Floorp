/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { BrowserWindowTracker } = ChromeUtils.importESModule(
  "resource:///modules/BrowserWindowTracker.sys.mjs",
);

// Mobile User-Agent string for Android
const MOBILE_UA =
  "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Mobile Safari/537.36 Edg/114.0.1823.79";

/**
 * Observer for http-on-modify-request
 * Modifies User-Agent header for all HTTP requests
 */
const httpRequestObserver = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  observe: (channel: any, topic: string) => {
    const topLevelWindow = getBrowserById(channel.browserId);
    console.log("topLevelWindow", topLevelWindow?.floorpBmsUserAgent);

    if (
      topic !== "http-on-modify-request" ||
      !(channel instanceof Ci.nsIHttpChannel) ||
      !topLevelWindow?.floorpBmsUserAgent
    ) {
      return;
    }

    try {
      channel.setRequestHeader("User-Agent", MOBILE_UA, false);
    } catch (error) {
      console.error("Failed to set User-Agent:", error);
    }
  },
};

function getBrowserById(browserId: string): Window | null {
  for (const win of BrowserWindowTracker.orderedWindows) {
    for (const tab of win.gBrowser.visibleTabs) {
      if (tab.linkedPanel) {
        if (tab.linkedBrowser && tab.linkedBrowser.browserId === browserId) {
          return tab.linkedBrowser.ownerGlobal;
        }
        return null;
      }
    }
  }
  return null;
}

// Register observer
Services.obs.addObserver(httpRequestObserver, "http-on-modify-request", false);
