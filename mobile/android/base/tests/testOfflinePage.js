// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

function ok(passed, text) {
  do_report_result(passed, text, Components.stack.caller, false);
}

function is(lhs, rhs, text) {
  do_report_result(lhs === rhs, text, Components.stack.caller, false);
}

// The chrome window
let chromeWin;

// Track the <browser> where the tests are happening
let browser;

// The proxy setting
let proxyPrefValue;

const kUniqueURI = Services.io.newURI("http://mochi.test:8888/tests/robocop/video_controls.html", null, null);

add_test(function setup_browser() {
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

  do_test_pending();

  // Add a new tab with a blank page so we can better control the real page load and the offline state
  browser = BrowserApp.addTab("about:blank", { selected: true, parentId: BrowserApp.selectedTab.id }).browser;

  // Go offline, expecting the error page.
  Services.io.offline = true;

  // Load our test web page
  browser.addEventListener("DOMContentLoaded", errorListener, true);
  browser.loadURI(kUniqueURI.spec, null, null)
});

//------------------------------------------------------------------------------
// listen to loading the neterror page. (offline mode)
function errorListener() {
  if (browser.contentWindow.location == "about:blank") {
    do_print("got about:blank, which is expected once, so return");
    return;
  }

  browser.removeEventListener("DOMContentLoaded", errorListener, true);
  ok(Services.io.offline, "Services.io.offline is true.");

  // This is an error page.
  is(browser.contentDocument.documentURI.substring(0, 27), "about:neterror?e=netOffline", "Document URI is the error page.");

  // But location bar should show the original request.
  is(browser.contentWindow.location.href, kUniqueURI.spec, "Docshell URI is the original URI.");

  Services.prefs.setIntPref("network.proxy.type", proxyPrefValue);

  // Now press the "Try Again" button, with offline mode off.
  Services.io.offline = false;

  browser.addEventListener("DOMContentLoaded", reloadListener, true);

  ok(browser.contentDocument.getElementById("errorTryAgain"), "The error page has got a #errorTryAgain element");
  browser.contentDocument.getElementById("errorTryAgain").click();
}


//------------------------------------------------------------------------------
// listen to reload of neterror.
function reloadListener() {
  browser.removeEventListener("DOMContentLoaded", reloadListener, true);

  ok(!Services.io.offline, "Services.io.offline is false.");

  // This is not an error page.
  is(browser.contentDocument.documentURI, kUniqueURI.spec, "Document URI is not the offline-error page, but the original URI.");

  do_test_finished();

  run_next_test();
}

run_next_test();
