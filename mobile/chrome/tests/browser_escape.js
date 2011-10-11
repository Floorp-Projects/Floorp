/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  EventUtils.sendKey("ESCAPE", window);
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
