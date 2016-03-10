// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Make the timer global so it doesn't get GC'd
var gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

function sleep(wait) {
  return new Promise((resolve, reject) => {
    do_print("sleep start");
    gTimer.initWithCallback({
      notify: function () {
        do_print("sleep end");
        resolve();
      },
    }, wait, gTimer.TYPE_ONE_SHOT);
  });
}

function promiseLoadEvent(browser, url, eventType="load") {
  return new Promise((resolve, reject) => {
    do_print("Wait browser event: " + eventType);

    function handle(event) {
      // Since we'll be redirecting, don't make assumptions about the given URL and the loaded URL
      if (event.target != browser.contentDocument || event.target.location.href == "about:blank") {
        do_print("Skipping spurious '" + eventType + "' event" + " for " + event.target.location.href);
        return;
      }

      browser.removeEventListener(eventType, handle, true);
      do_print("Browser event received: " + eventType);
      resolve(event);
    }

    browser.addEventListener(eventType, handle, true);
    if (url) {
      browser.loadURI(url);
    }
  });
}

// Wait 6 seconds for the pending visits to flush (which should happen in 3 seconds)
const PENDING_VISIT_WAIT = 6000;
// Longer wait required after first load
const PENDING_VISIT_WAIT_LONG = 20000;

// Manage the saved history visits so we can compare in the tests
var gVisitURLs = [];
function visitObserver(subject, topic, data) {
  let uri = subject.QueryInterface(Ci.nsIURI);
  do_print("Observer: " + uri.spec);
  gVisitURLs.push(uri.spec);
};

// Track the <browser> where the tests are happening
var gBrowser;

add_test(function setup_browser() {
  let chromeWin = Services.wm.getMostRecentWindow("navigator:browser");
  let BrowserApp = chromeWin.BrowserApp;

  do_register_cleanup(function cleanup() {
    Services.obs.removeObserver(visitObserver, "link-visited");
    BrowserApp.closeTab(BrowserApp.getTabForBrowser(gBrowser));
  });

  Services.obs.addObserver(visitObserver, "link-visited", false);

  // Load a blank page
  let url = "about:blank";
  gBrowser = BrowserApp.addTab(url, { selected: true, parentId: BrowserApp.selectedTab.id }).browser;
  gBrowser.addEventListener("load", function startTests(event) {
    gBrowser.removeEventListener("load", startTests, true);
    Services.tm.mainThread.dispatch(run_next_test, Ci.nsIThread.DISPATCH_NORMAL);
  }, true);
});

add_task(function* () {
  // Wait for any initial page loads to be saved to history
  yield sleep(PENDING_VISIT_WAIT);

  // Load a simple HTML page with no redirects
  gVisitURLs = [];
  yield promiseLoadEvent(gBrowser, "http://example.org/tests/robocop/robocop_blank_01.html");
  yield sleep(PENDING_VISIT_WAIT_LONG);

  do_print("visit counts: " + gVisitURLs.length);
  ok(gVisitURLs.length == 1, "Simple visit makes 1 history item");

  do_print("visit URL: " + gVisitURLs[0]);
  ok(gVisitURLs[0] == "http://example.org/tests/robocop/robocop_blank_01.html", "Simple visit makes final history item");

  // Load a simple HTML page via a 301 temporary redirect
  gVisitURLs = [];
  yield promiseLoadEvent(gBrowser, "http://example.org/tests/robocop/simple_redirect.sjs?http://example.org/tests/robocop/robocop_blank_02.html");
  yield sleep(PENDING_VISIT_WAIT);

  do_print("visit counts: " + gVisitURLs.length);
  ok(gVisitURLs.length == 1, "Simple 301 redirect makes 1 history item");

  do_print("visit URL: " + gVisitURLs[0]);
  ok(gVisitURLs[0] == "http://example.org/tests/robocop/robocop_blank_02.html", "Simple 301 redirect makes final history item");

  // Load a simple HTML page via a JavaScript redirect
  gVisitURLs = [];
  yield promiseLoadEvent(gBrowser, "http://example.org/tests/robocop/javascript_redirect.sjs?http://example.org/tests/robocop/robocop_blank_03.html");
  yield sleep(PENDING_VISIT_WAIT);

  do_print("visit counts: " + gVisitURLs.length);
  ok(gVisitURLs.length == 2, "JavaScript redirect makes 2 history items");

  do_print("visit URL 1: " + gVisitURLs[0]);
  ok(gVisitURLs[0] == "http://example.org/tests/robocop/javascript_redirect.sjs?http://example.org/tests/robocop/robocop_blank_03.html", "JavaScript redirect makes intermediate history item");

  do_print("visit URL 2: " + gVisitURLs[1]);
  ok(gVisitURLs[1] == "http://example.org/tests/robocop/robocop_blank_03.html", "JavaScript redirect makes final history item");
});

run_next_test();
