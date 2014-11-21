// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

function ok(passed, text) {
  do_report_result(passed, text, Components.stack.caller, false);
}

function promiseLoadEvent(browser, url, eventType="load", runBeforeLoad) {
  return new Promise((resolve, reject) => {
    do_print("Wait browser event: " + eventType);

    function handle(event) {
      if (event.target != browser.contentDocument || event.target.location.href == "about:blank" || (url && event.target.location.href != url)) {
        do_print("Skipping spurious '" + eventType + "' event" + " for " + event.target.location.href);
        return;
      }

      browser.removeEventListener(eventType, handle, true);
      do_print("Browser event received: " + eventType);
      resolve(event);
    }

    browser.addEventListener(eventType, handle, true);

    if (runBeforeLoad) {
      runBeforeLoad();
    }
    if (url) {
      browser.loadURI(url);
    }
  });
}

// Test that the Tracking Protection is active and has the correct state when
// tracking content is blocked (Bug 1063831)

// Code is mostly stolen from:
// http://mxr.mozilla.org/mozilla-central/source/browser/base/content/test/general/browser_trackingUI.js

var PREF = "privacy.trackingprotection.enabled";
var TABLE = "urlclassifier.trackingTable";

// Update tracking database
function doUpdate() {
  // Add some URLs to the tracking database (to be blocked)
  var testData = "tracking.example.com/";
  var testUpdate =
    "n:1000\ni:test-track-simple\nad:1\n" +
    "a:524:32:" + testData.length + "\n" +
    testData;

  let dbService = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(Ci.nsIUrlClassifierDBService);

  return new Promise((resolve, reject) => {
    let listener = {
      QueryInterface: function(iid) {
        if (iid.equals(Ci.nsISupports) || iid.equals(Ci.nsIUrlClassifierUpdateObserver))
          return this;

        throw Cr.NS_ERROR_NO_INTERFACE;
      },
      updateUrlRequested: function(url) { },
      streamFinished: function(status) { },
      updateError: function(errorCode) {
        ok(false, "Couldn't update classifier.");
        resolve();
      },
      updateSuccess: function(requestedTimeout) {
        resolve();
      }
    };

    dbService.beginUpdate(listener, "test-track-simple", "");
    dbService.beginStream("", "");
    dbService.updateStream(testUpdate);
    dbService.finishStream();
    dbService.finishUpdate();
  });
}

// Track the <browser> where the tests are happening
let browser;

add_test(function setup_browser() {
  let chromeWin = Services.wm.getMostRecentWindow("navigator:browser");
  let BrowserApp = chromeWin.BrowserApp;

  do_register_cleanup(function cleanup() {
    Services.prefs.clearUserPref(PREF);
    Services.prefs.clearUserPref(TABLE);
    BrowserApp.closeTab(BrowserApp.getTabForBrowser(browser));
  });

  // Load a blank page
  let url = "about:blank";
  browser = BrowserApp.addTab(url, { selected: true, parentId: BrowserApp.selectedTab.id }).browser;
  browser.addEventListener("load", function startTests(event) {
    browser.removeEventListener("load", startTests, true);
    Services.tm.mainThread.dispatch(run_next_test, Ci.nsIThread.DISPATCH_NORMAL);
  }, true);
});

add_task(function* () {
  // Populate and use 'test-track-simple' for tracking protection lookups
  Services.prefs.setCharPref(TABLE, "test-track-simple");
  yield doUpdate();

  // Enable Tracking Protection
  Services.prefs.setBoolPref(PREF, true);

  // Point tab to a test page NOT containing tracking elements
  yield promiseLoadEvent(browser, "http://tracking.example.org/tests/robocop/tracking_good.html");
  Messaging.sendRequest({ type: "Test:Expected", expected: "unknown" });

  // Point tab to a test page containing tracking elements
  yield promiseLoadEvent(browser, "http://tracking.example.org/tests/robocop/tracking_bad.html");
  Messaging.sendRequest({ type: "Test:Expected", expected: "tracking_content_blocked" });

  // Simulate a click on the "Disable protection" button in the site identity popup.
  // We need to wait for a "load" event because "Session:Reload" will cause a full page reload.
  yield promiseLoadEvent(browser, undefined, undefined, () => {
    Services.obs.notifyObservers(null, "Session:Reload", "{\"allowContent\":true,\"contentType\":\"tracking\"}");
  });
  Messaging.sendRequest({ type: "Test:Expected", expected: "tracking_content_loaded" });

  // Simulate a click on the "Enable protection" button in the site identity popup.
  yield promiseLoadEvent(browser, undefined, undefined, () => {
    Services.obs.notifyObservers(null, "Session:Reload", "{\"allowContent\":false,\"contentType\":\"tracking\"}");
  });
  Messaging.sendRequest({ type: "Test:Expected", expected: "tracking_content_blocked" });

  // Disable Tracking Protection
  Services.prefs.setBoolPref(PREF, false);

  // Point tab to a test page containing tracking elements
  yield promiseLoadEvent(browser, "http://tracking.example.org/tests/robocop/tracking_bad.html");
  Messaging.sendRequest({ type: "Test:Expected", expected: "unknown" });

  // Point tab to a test page NOT containing tracking elements
  yield promiseLoadEvent(browser, "http://tracking.example.org/tests/robocop/tracking_good.html");
  Messaging.sendRequest({ type: "Test:Expected", expected: "unknown" });
});

run_next_test();
