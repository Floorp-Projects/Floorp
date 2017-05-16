/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "http://example.com/browser/toolkit/components/startup/tests/browser/beforeunload.html";

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

function test() {
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();

  // Create foreground window
  let win2 = window.openDialog(location, "", "chrome,all,dialog=no", "about:blank");
  win2.addEventListener("load", function() {
    // Create background test tab
    let browser = BrowserTestUtils.addTab(gBrowser, TEST_URL).linkedBrowser;

    whenBrowserLoaded(browser, function() {
      let seenDialog = false;

      // Cancel the prompt the first time.
      waitForOnBeforeUnloadDialog(browser, (btnLeave, btnStay) => {
        seenDialog = Services.focus.activeWindow == window &&
                     gBrowser.selectedBrowser == browser;
        btnStay.click();
      });

      let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                         getService(Ci.nsIAppStartup);
      appStartup.quit(Ci.nsIAppStartup.eAttemptQuit);
      ok(seenDialog, "Should have seen a prompt dialog");
      ok(!win2.closed, "Shouldn't have closed the additional window");
      win2.close();

      // Leave the page the second time.
      waitForOnBeforeUnloadDialog(browser, (btnLeave, btnStay) => {
        btnLeave.click();
      });

      gBrowser.removeTab(gBrowser.selectedTab);
      executeSoon(finish);
    });
  }, {once: true});
}
