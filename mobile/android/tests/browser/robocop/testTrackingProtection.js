// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { EventDispatcher } = ChromeUtils.import(
  "resource://gre/modules/Messaging.jsm"
);

const DTSCBN_PREF = "dom.testing.sync-content-blocking-notifications";

function promiseLoadEvent(browser, url, eventType = "load", runBeforeLoad) {
  return new Promise((resolve, reject) => {
    do_print("Wait browser event: " + eventType);

    function handle(event) {
      if (
        event.target != browser.contentDocument ||
        event.target.location.href == "about:blank" ||
        (url && event.target.location.href != url)
      ) {
        do_print(
          "Skipping spurious '" +
            eventType +
            "' event" +
            " for " +
            event.target.location.href
        );
        return;
      }

      browser.removeEventListener(eventType, handle, true);
      do_print(
        "Browser event received: " +
          eventType +
          ". Will wait 500ms for the tracking event also."
      );
      do_timeout(500, () => {
        resolve(event);
      });
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
// tracking content is blocked (Bug 1063831 + Bug 1520520)

var TABLE = "urlclassifier.trackingTable";

var BrowserApp = Services.wm.getMostRecentWindow("navigator:browser")
  .BrowserApp;

// Tests the tracking protection UI in private browsing. By default, tracking protection is
// enabled in private browsing ("privacy.trackingprotection.pbmode.enabled").
add_task(async function test_tracking_pb() {
  Services.prefs.setBoolPref(DTSCBN_PREF, true);

  // Load a blank page
  let browser = BrowserApp.addTab("about:blank", {
    selected: true,
    parentId: BrowserApp.selectedTab.id,
    isPrivate: true,
  }).browser;
  await new Promise((resolve, reject) => {
    browser.addEventListener(
      "load",
      function(event) {
        Services.tm.dispatchToMainThread(resolve);
      },
      { capture: true, once: true }
    );
  });

  // Populate and use 'test-track-simple' for tracking protection lookups
  Services.prefs.setCharPref(TABLE, "moztest-track-simple");

  // Point tab to a test page NOT containing tracking elements
  await promiseLoadEvent(
    browser,
    "http://tracking.example.org/tests/robocop/tracking_good.html"
  );
  EventDispatcher.instance.sendRequest({
    type: "Test:Expected",
    expected: "unknown",
  });

  // Point tab to a test page containing tracking elements
  await promiseLoadEvent(
    browser,
    "http://tracking.example.org/tests/robocop/tracking_bad.html"
  );
  EventDispatcher.instance.sendRequest({
    type: "Test:Expected",
    expected: "tracking_content_blocked",
  });

  // Simulate a click on the "Disable protection" button in the site identity popup.
  // We need to wait for a "load" event because "Session:Reload" will cause a full page reload.
  await promiseLoadEvent(browser, undefined, undefined, () => {
    EventDispatcher.instance.dispatch("Session:Reload", {
      allowContent: true,
      contentType: "tracking",
    });
  });
  EventDispatcher.instance.sendRequest({
    type: "Test:Expected",
    expected: "tracking_content_loaded",
  });

  // Simulate a click on the "Enable protection" button in the site identity popup.
  await promiseLoadEvent(browser, undefined, undefined, () => {
    EventDispatcher.instance.dispatch("Session:Reload", {
      allowContent: false,
      contentType: "tracking",
    });
  });
  EventDispatcher.instance.sendRequest({
    type: "Test:Expected",
    expected: "tracking_content_blocked",
  });

  // Disable tracking protection to make sure we don't show the UI when the pref is disabled.
  Services.prefs.setBoolPref(
    "privacy.trackingprotection.pbmode.enabled",
    false
  );

  // Point tab to a test page containing tracking elements
  await promiseLoadEvent(
    browser,
    "http://tracking.example.org/tests/robocop/tracking_bad.html"
  );
  EventDispatcher.instance.sendRequest({
    type: "Test:Expected",
    expected: "unknown",
  });

  // Point tab to a test page NOT containing tracking elements
  await promiseLoadEvent(
    browser,
    "http://tracking.example.org/tests/robocop/tracking_good.html"
  );
  EventDispatcher.instance.sendRequest({
    type: "Test:Expected",
    expected: "unknown",
  });

  // Reset the pref before the next testcase
  Services.prefs.clearUserPref("privacy.trackingprotection.pbmode.enabled");
});

add_task(async function test_tracking_not_pb() {
  // Load a blank page
  let browser = BrowserApp.addTab("about:blank", { selected: true }).browser;
  await new Promise((resolve, reject) => {
    browser.addEventListener(
      "load",
      function(event) {
        Services.tm.dispatchToMainThread(resolve);
      },
      { capture: true, once: true }
    );
  });

  // Point tab to a test page NOT containing tracking elements
  await promiseLoadEvent(
    browser,
    "http://tracking.example.org/tests/robocop/tracking_good.html"
  );
  EventDispatcher.instance.sendRequest({
    type: "Test:Expected",
    expected: "unknown",
  });

  // Point tab to a test page containing tracking elements (tracking protection UI *should not* be shown)
  await promiseLoadEvent(
    browser,
    "http://tracking.example.org/tests/robocop/tracking_bad.html"
  );
  EventDispatcher.instance.sendRequest({
    type: "Test:Expected",
    expected: "unknown",
  });

  // Enable tracking protection in normal tabs
  Services.prefs.setBoolPref("privacy.trackingprotection.enabled", true);

  // Point tab to a test page containing tracking elements (tracking protection UI *should* be shown)
  await promiseLoadEvent(
    browser,
    "http://tracking.example.org/tests/robocop/tracking_bad.html"
  );
  EventDispatcher.instance.sendRequest({
    type: "Test:Expected",
    expected: "tracking_content_blocked",
  });
});

add_task(async function cleanup() {
  Services.prefs.clearUserPref(DTSCBN_PREF);
});

run_next_test();
