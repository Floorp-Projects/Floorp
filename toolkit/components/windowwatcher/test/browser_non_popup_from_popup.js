"use strict";

// Opening non-popup from a popup should open a new tab in the most recent
// non-popup window.
add_task(async function test_non_popup_from_popup() {
  const BLANK_PAGE = "data:text/html,";

  // A page opened in a new tab.
  const OPEN_PAGE = "data:text/plain,hello";

  // A page opened in a new popup.
  // This opens a new non-popup page with OPEN_PAGE,
  // tha should be opened in a new tab in most recent window.
  const NON_POPUP_OPENER = btoa(
    `data:text/html,<script>window.open('${OPEN_PAGE}', '', '')</script>`
  );

  // A page opened in a new tab.
  // This opens a popup with NON_POPUP_OPENER.
  const POPUP_OPENER = `data:text/html,<script>window.open(atob("${NON_POPUP_OPENER}"), "", "width=500");</script>`;

  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 3]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: BLANK_PAGE,
    },
    async function(browser) {
      // Wait for a popup opened by POPUP_OPENER.
      const newPopupPromise = BrowserTestUtils.waitForNewWindow();

      // Wait for a new tab opened by NON_POPUP_OPENER.
      const newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, OPEN_PAGE);

      // Open a new tab that opens a popup with NON_POPUP_OPENER.
      await BrowserTestUtils.loadURI(gBrowser, POPUP_OPENER);

      let win = await newPopupPromise;
      Assert.ok(true, "popup is opened");

      let tab = await newTabPromise;
      Assert.ok(true, "new tab is opened in the recent window");

      BrowserTestUtils.removeTab(tab);
      await BrowserTestUtils.closeWindow(win);
    }
  );

  await SpecialPowers.popPrefEnv();
});
