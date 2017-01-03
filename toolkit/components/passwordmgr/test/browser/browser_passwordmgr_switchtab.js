/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PROMPT_URL = "chrome://global/content/commonDialog.xul";
var { interfaces: Ci } = Components;

add_task(function* test() {
  yield new Promise(resolve => {

  let tab = gBrowser.addTab();
  isnot(tab, gBrowser.selectedTab, "New tab shouldn't be selected");

  let listener = {
    onOpenWindow(window) {
      var domwindow = window.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindow);
      waitForFocus(() => {
        is(domwindow.document.location.href, PROMPT_URL, "Should have seen a prompt window");
        is(domwindow.args.promptType, "promptUserAndPass", "Should be an authenticate prompt");

        is(gBrowser.selectedTab, tab, "Should have selected the new tab");

        domwindow.document.documentElement.cancelDialog();
      }, domwindow);
    },

    onCloseWindow() {
    }
  };

  Services.wm.addListener(listener);
  registerCleanupFunction(() => {
    Services.wm.removeListener(listener);
    gBrowser.removeTab(tab);
  });

  tab.linkedBrowser.addEventListener("load", () => {
    finish();
  }, true);
  tab.linkedBrowser.loadURI("http://example.com/browser/toolkit/components/passwordmgr/test/browser/authenticate.sjs");

  });
});
