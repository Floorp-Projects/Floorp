/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that a Picture-in-Picture window opened by a Private browsing
 * window has the "private" feature set on its window (which is important
 * for some things, eg: taskbar grouping on Windows).
 */
add_task(async () => {
  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    let privateWin = await BrowserTestUtils.openNewBrowserWindow({
      private: true,
    });
    let pipTab = await BrowserTestUtils.openNewForegroundTab(
      privateWin.gBrowser,
      TEST_PAGE
    );
    let browser = pipTab.linkedBrowser;

    let pipWin = await triggerPictureInPicture(browser, videoID);
    ok(pipWin, "Got Picture-in-Picture window.");

    Assert.equal(
      pipWin.docShell.treeOwner
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIAppWindow).chromeFlags &
        Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
      Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW,
      "Picture-in-Picture window should be marked as private"
    );

    await BrowserTestUtils.closeWindow(privateWin);
  }
});
