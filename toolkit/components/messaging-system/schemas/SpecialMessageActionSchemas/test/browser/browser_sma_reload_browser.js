/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_disable_doh() {
  const TEST_URL = "https://example.com/";
  const action = {
    type: "RELOAD_BROWSER",
  };
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let reloadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser
  ).then(() => ok(true, "browser reloaded as expected"));

  await SpecialMessageActions.handleAction(action, gBrowser);
  await reloadedPromise;

  BrowserTestUtils.removeTab(tab);
});
