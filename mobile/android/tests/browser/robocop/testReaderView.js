// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

function promiseNotification(topic) {
  return new Promise((resolve, reject) => {
    function observe(subject, topic, data) {
      Services.obs.removeObserver(observe, topic);
      resolve();
    }
    Services.obs.addObserver(observe, topic, false);
  });
}

add_task(function* test_reader_view_visibility() {
  let gWin = Services.wm.getMostRecentWindow("navigator:browser");
  let BrowserApp = gWin.BrowserApp;

  let url = "http://mochi.test:8888/tests/robocop/reader_mode_pages/basic_article.html";
  let browser = BrowserApp.addTab("about:reader?url=" + url).browser;

  yield promiseBrowserEvent(browser, "load");

  do_register_cleanup(function cleanup() {
    BrowserApp.closeTab(BrowserApp.getTabForBrowser(browser));
  });

  let doc = browser.contentDocument;
  let title = doc.getElementById("reader-title");

  // We need to wait for reader content to appear because AboutReader.jsm
  // asynchronously fetches the content after about:reader loads.
  yield promiseNotification("AboutReader:Ready");
  do_check_eq(title.textContent, "Article title");
});

run_next_test();
