/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

SpecialPowers.pushPrefEnv({
  set: [["dom.require_user_interaction_for_beforeunload", false]],
});

const TEST_URL =
  "http://example.com/browser/toolkit/components/startup/tests/browser/beforeunload.html";

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  let browser = gBrowser.selectedBrowser;

  whenBrowserLoaded(browser, function() {
    let seenDialog = false;

    // Cancel the prompt the first time.
    waitForOnBeforeUnloadDialog(browser, (btnLeave, btnStay) => {
      seenDialog = true;
      btnStay.click();
    });

    Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
    ok(seenDialog, "Should have seen a prompt dialog");
    ok(!window.closed, "Shouldn't have closed the window");

    let win2 = window.openDialog(
      location,
      "",
      "chrome,all,dialog=no",
      "about:blank"
    );
    ok(win2 != null, "Should have been able to open a new window");
    win2.addEventListener(
      "load",
      () => {
        executeSoon(() => {
          win2.close();

          // Leave the page the second time.
          waitForOnBeforeUnloadDialog(browser, (btnLeave, btnStay) => {
            btnLeave.click();
          });

          gBrowser.removeTab(gBrowser.selectedTab);
          finish();
        });
      },
      { once: true }
    );
  });
}
