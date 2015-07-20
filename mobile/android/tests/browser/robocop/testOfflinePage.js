// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

function promiseBrowserEvent(browser, eventType) {
  return new Promise((resolve) => {
    function handle(event) {
      // Since we'll be redirecting, don't make assumptions about the given URL and the loaded URL
      if (event.target != browser.contentDocument || event.target.location.href == "about:blank") {
        do_print("Skipping spurious '" + eventType + "' event" + " for " + event.target.location.href);
        return;
      }
      do_print("Received event " + eventType + " from browser");
      browser.removeEventListener(eventType, handle, true);
      resolve(event);
    }

    browser.addEventListener(eventType, handle, true);
    do_print("Now waiting for " + eventType + " event from browser");
  });
}

// Provide a helper to yield until we are sure the offline state has changed
function promiseOffline(isOffline) {
  return new Promise((resolve, reject) => {
    function observe(subject, topic, data) {
      do_print("Received topic: " + topic);
      Services.obs.removeObserver(observe, "network:offline-status-changed");
      resolve();
    }
    Services.obs.addObserver(observe, "network:offline-status-changed", false);
    Services.io.offline = isOffline;
  });
}

// The chrome window
let chromeWin;

// Track the <browser> where the tests are happening
let browser;

// The proxy setting
let proxyPrefValue;

const kUniqueURI = Services.io.newURI("http://mochi.test:8888/tests/robocop/video_controls.html", null, null);

add_task(function* test_offline() {
  // Tests always connect to localhost, and per bug 87717, localhost is now
  // reachable in offline mode.  To avoid this, disable any proxy.
  proxyPrefValue = Services.prefs.getIntPref("network.proxy.type");
  Services.prefs.setIntPref("network.proxy.type", 0);

  // Clear network cache.
  Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(Ci.nsICacheStorageService).clear();

  chromeWin = Services.wm.getMostRecentWindow("navigator:browser");
  let BrowserApp = chromeWin.BrowserApp;

  do_register_cleanup(function cleanup() {
    BrowserApp.closeTab(BrowserApp.getTabForBrowser(browser));
    Services.prefs.setIntPref("network.proxy.type", proxyPrefValue);
    Services.io.offline = false;
  });

  // Add a new tab with a blank page so we can better control the real page load and the offline state
  browser = BrowserApp.addTab("about:blank", { selected: true, parentId: BrowserApp.selectedTab.id }).browser;

  // Go offline, expecting the error page.
  yield promiseOffline(true);

  // Load our test web page
  browser.loadURI(kUniqueURI.spec, null, null)
  yield promiseBrowserEvent(browser, "DOMContentLoaded");

  // This is an error page.
  is(browser.contentDocument.documentURI.substring(0, 27), "about:neterror?e=netOffline", "Document URI is the error page.");

  // But location bar should show the original request.
  is(browser.contentWindow.location.href, kUniqueURI.spec, "Docshell URI is the original URI.");

  Services.prefs.setIntPref("network.proxy.type", proxyPrefValue);

  // Go online and try to load the page again
  yield promiseOffline(false);

  ok(browser.contentDocument.getElementById("errorTryAgain"), "The error page has got a #errorTryAgain element");

  // Click "Try Again" button to start the page load
  browser.contentDocument.getElementById("errorTryAgain").click();
  yield promiseBrowserEvent(browser, "DOMContentLoaded");

  // This is not an error page.
  is(browser.contentDocument.documentURI, kUniqueURI.spec, "Document URI is not the offline-error page, but the original URI.");
});

run_next_test();
