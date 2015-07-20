// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

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

// Load a custom sjs script that echos our "User-Agent" header back at us
const TestURI = Services.io.newURI("http://mochi.test:8888/tests/robocop/desktopmode_user_agent.sjs", null, null);

add_task(function* test_desktopmode() {
  let chromeWin = Services.wm.getMostRecentWindow("navigator:browser");
  let BrowserApp = chromeWin.BrowserApp;

  // Add a new 'desktop mode' tab with our test page
  let desktopBrowser = BrowserApp.addTab(TestURI.spec, { selected: true, parentId: BrowserApp.selectedTab.id, desktopMode: true }).browser;
  yield promiseBrowserEvent(desktopBrowser, "load");

  // Some debugging
  do_print("desktop: " + desktopBrowser.contentWindow.navigator.userAgent);
  do_print("desktop: " + desktopBrowser.contentDocument.body.innerHTML);

  // Check the server UA and the navigator UA for 'desktop'
  ok(desktopBrowser.contentWindow.navigator.userAgent.indexOf("Linux x86_64") != -1, "window.navigator.userAgent has 'Linux' in it");
  ok(desktopBrowser.contentDocument.body.innerHTML.indexOf("Linux x86_64") != -1, "HTTP header 'User-Agent' has 'Linux' in it");

  // Add a new 'mobile mode' tab with our test page
  let mobileBrowser = BrowserApp.addTab(TestURI.spec, { selected: true, parentId: BrowserApp.selectedTab.id }).browser;
  yield promiseBrowserEvent(mobileBrowser, "load");

  // Some debugging
  do_print("mobile: " + mobileBrowser.contentWindow.navigator.userAgent);
  do_print("mobile: " + mobileBrowser.contentDocument.body.innerHTML);

  // Check the server UA and the navigator UA for 'mobile'
  // We only check for 'Android' because we don't know the version or if it's phone or tablet
  ok(mobileBrowser.contentWindow.navigator.userAgent.indexOf("Android") != -1, "window.navigator.userAgent has 'Android' in it");
  ok(mobileBrowser.contentDocument.body.innerHTML.indexOf("Android") != -1, "HTTP header 'User-Agent' has 'Android' in it");
});

run_next_test();
