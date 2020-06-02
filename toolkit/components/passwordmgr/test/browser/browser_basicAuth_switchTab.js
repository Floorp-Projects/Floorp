/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PROMPT_URL = "chrome://global/content/commonDialog.xhtml";

add_task(async function test() {
  await new Promise(resolve => {
    let tab = BrowserTestUtils.addTab(gBrowser);
    isnot(tab, gBrowser.selectedTab, "New tab shouldn't be selected");

    let listener = {
      onOpenWindow(xulWin) {
        var domwindow = xulWin.docShell.domWindow;
        waitForFocus(() => {
          is(
            domwindow.document.location.href,
            PROMPT_URL,
            "Should have seen a prompt window"
          );
          is(
            domwindow.args.promptType,
            "promptUserAndPass",
            "Should be an authenticate prompt"
          );

          is(gBrowser.selectedTab, tab, "Should have selected the new tab");

          domwindow.document.getElementById("commonDialog").cancelDialog();
        }, domwindow);
      },

      onCloseWindow() {},
    };

    Services.wm.addListener(listener);
    registerCleanupFunction(() => {
      Services.wm.removeListener(listener);
      gBrowser.removeTab(tab);
    });

    BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(() => finish());
    BrowserTestUtils.loadURI(
      tab.linkedBrowser,
      "http://example.com/browser/toolkit/components/passwordmgr/test/browser/authenticate.sjs"
    );
  });
});
