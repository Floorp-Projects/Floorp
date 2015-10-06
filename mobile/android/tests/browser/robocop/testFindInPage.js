// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cu = Components.utils;

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const TEST_URL = "http://mochi.test:8888/tests/robocop/robocop_text_page.html";

function promiseBrowserEvent(browser, eventType) {
  return new Promise((resolve) => {
    function handle(event) {
      do_print("Received event " + eventType + " from browser");
      browser.removeEventListener(eventType, handle, true);
      resolve(event);
    }

    browser.addEventListener(eventType, handle, true);
    do_print("Now waiting for " + eventType + " event from browser");
  });
}

function openTabWithUrl(url) {
  do_print("Going to open " + url);
  let browserApp = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
  let browser = browserApp.addTab(url, { selected: true, parentId: browserApp.selectedTab.id }).browser;

  return promiseBrowserEvent(browser, "load")
    .then(() => { return browser; });
}

function findInPage(browser, text, nrOfMatches) {
  let repaintPromise = promiseBrowserEvent(browser, "MozAfterPaint");
  do_print("Send findInPageMessage: " + text + " nth: " + nrOfMatches);
  Messaging.sendRequest({ type: "Test:FindInPage", text: text, nrOfMatches: nrOfMatches });
  return repaintPromise;
}

function closeFindInPage(browser) {
  let repaintPromise = promiseBrowserEvent(browser, "MozAfterPaint");
  do_print("Send closeFindInPageMessage");
  Messaging.sendRequest({ type: "Test:CloseFindInPage" });
  return repaintPromise;
}

function assertSelection(document, expectedSelection = false, expectedAnchorText = false) {
  let sel = document.getSelection();
  if (!expectedSelection) {
    do_print("Assert empty selection");
    do_check_eq(sel.toString(), "");
  } else {
    do_print("Assert selection to be " + expectedSelection);
    do_check_eq(sel.toString(), expectedSelection);
  }
  if (expectedAnchorText) {
    do_print("Assert anchor text to be " + expectedAnchorText);
    do_check_eq(sel.anchorNode.textContent, expectedAnchorText);
  }
}

add_task(function* testFindInPage() {
  let browser = yield openTabWithUrl(TEST_URL);
  let document = browser.contentDocument;

  yield findInPage(browser, "Robocoop", 1);
  assertSelection(document);

  yield closeFindInPage(browser);
  assertSelection(document);

  yield findInPage(browser, "Robocop", 1);
  assertSelection(document, "Robocop", " Robocop 1 ");

  yield closeFindInPage(browser);
  assertSelection(document);

  yield findInPage(browser, "Robocop", 3);
  assertSelection(document, "Robocop", " Robocop 3 ");

  yield closeFindInPage(browser);
  assertSelection(document);
});

run_next_test();
