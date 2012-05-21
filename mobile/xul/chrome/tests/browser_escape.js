/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const url1 = baseURI + "browser_blank_01.html";
const url2 = baseURI + "browser_blank_02.html";
const url3 = baseURI + "browser_blank_03.html";
let tab1, tab2;

function test() {
  waitForExplicitFinish();
  testGoBack();

  registerCleanupFunction(closeTabs);
}

function testGoBack() {
  tab1 = Browser.addTab("about:blank", true);
  tab2 = Browser.addTab("about:blank", true, tab1);
  let browser = tab2.browser;

  // Load each of these URLs, then use escape to step backward through them.
  let urls = [url1, url2, url3];
  let i = 0, step = 1;
  let expectedURI;

  function loadNextPage() {
    expectedURI = urls[i];
    if (step == 1) {
      // go forward
      Browser.loadURI(expectedURI);
    } else {
      // go back by pressing the escape key
      Browser.selectedBrowser.focus();
      EventUtils.synthesizeKey("VK_ESCAPE", {type: "keypress"}, window);
    }
  }

  browser.messageManager.addMessageListener("pageshow", function listener(aMessage) {
    let uri = browser.currentURI.spec;
    if (uri == "about:blank") {
      loadNextPage();
      return;
    }
    is(uri, expectedURI, "Page " + i + " loaded");

    if (i == urls.length - 1)
      step = -1; // start going back when we get to the end
    i += step;

    if (i >= 0) {
      loadNextPage();
      //setTimeout(loadNextPage, 1000);
    } else {
      // All done. Go to the next test.
      browser.messageManager.removeMessageListener("pageshow", listener);
      closeTabs();
      testReturnToOwner();
    }
  });
}

function testReturnToOwner() {
  tab1 = Browser.addTab("about:blank", true);
  tab2 = Browser.addTab("about:blank", true, tab1);
  is(Browser.selectedTab, tab2, "tab2 is selected");
  EventUtils.sendKey("ESCAPE");
  is(Browser.selectedTab, tab1, "tab1 is selected");
  closeTabs();
  testContextMenu();
}

function testContextMenu() {
  ContextHelper.showPopup({
    json: {
      types: ['link']
    },
    target: Browser.selectedBrowser
  });
  ok(ContextHelper.popupState, "Context menu is shown");
  Browser.selectedBrowser.focus();
  EventUtils.synthesizeKey("VK_ESCAPE", {type: "keypress"}, window);
  ok(!ContextHelper.popupState, "Context menu is dismissed");
  finish();
}

function closeTabs() {
  try {
    Browser.closeTab(tab1);
    Browser.closeTab(tab2);
  } finally {
    tab1 = tab2 = null;
  }
}
