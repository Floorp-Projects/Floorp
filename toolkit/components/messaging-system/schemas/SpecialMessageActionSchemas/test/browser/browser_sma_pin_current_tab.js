/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_PIN_CURRENT_TAB() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    await SMATestUtils.executeAndValidateAction({ type: "PIN_CURRENT_TAB" });

    ok(gBrowser.selectedTab.pinned, "should pin current tab");

    gBrowser.unpinTab(gBrowser.selectedTab);
  });
});
