/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

// The actual uri we open doesn't really matter.
const OPENED_URI = PrintHelper.defaultTestPageUrlHTTPS;

// Test for bug 1669554:
//
// This opens a rel=noopener window in a content process, which causes us to
// create a browsing context with an id that likely overflows an int32_t, which
// caused us to fail to parse the initialBrowsingContextGroupId attribute
// (causing us to potentially clone in the wrong process, etc).
const OPEN_NOOPENER_WINDOW = `
  <a rel="noopener" target="_blank" href="${OPENED_URI}">Open the window</a>
`;

add_task(async function test_bc_id_overflow() {
  is(document.querySelector(".printPreviewBrowser"), null);

  await BrowserTestUtils.withNewTab(
    `data:text/html,` + encodeURIComponent(OPEN_NOOPENER_WINDOW),
    async function (browser) {
      let tabOpenedPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        OPENED_URI,
        /* waitForLoad = */ true
      );
      await BrowserTestUtils.synthesizeMouse("a", 0, 0, {}, browser);
      let tab = await tabOpenedPromise;
      let helper = new PrintHelper(tab.linkedBrowser);
      await helper.startPrint();
      helper.assertDialogOpen();

      let previewBrowser = document.querySelector(".printPreviewBrowser");
      is(typeof previewBrowser.browsingContext.group.id, "number", "Sanity");
      is(
        previewBrowser.browsingContext.group.id,
        tab.linkedBrowser.browsingContext.group.id,
        "Group ids should match: " + tab.linkedBrowser.browsingContext.group.id
      );
      is(
        previewBrowser.browsingContext.group,
        tab.linkedBrowser.browsingContext.group,
        "Groups should match"
      );
      await helper.closeDialog();
      await BrowserTestUtils.removeTab(tab);
    }
  );
});
