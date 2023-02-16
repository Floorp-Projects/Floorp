/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Note: here we just naively check that the page has been reloaded.
// We might use this test later to refactor the ENABLE_CBH action
// back into a MULTI_ACTION which includes pref flips followed by a
// RELOAD_BROWSER action, as discussed in the review for bug 1816464.
add_task(async function test_enable_cbh() {
  const TEST_URL = "https://example.com/";
  const action = {
    type: "ENABLE_CBH",
  };
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let reloadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser
  ).then(() => ok(true, "browser reloaded as expected"));

  await SpecialMessageActions.handleAction(action, gBrowser);
  await reloadedPromise;

  BrowserTestUtils.removeTab(tab);
});
