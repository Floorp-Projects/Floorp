/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_OPEN_URL() {
  const action = {
    type: "OPEN_URL",
    data: { args: EXAMPLE_URL, where: "current" },
  };
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    const loaded = BrowserTestUtils.browserLoaded(browser);
    await SMATestUtils.executeAndValidateAction(action);
    const url = await loaded;
    Assert.equal(
      url,
      "https://example.com/",
      "should open URL in the same tab"
    );
  });
});

add_task(async function test_OPEN_URL_new_tab() {
  const action = {
    type: "OPEN_URL",
    data: { args: EXAMPLE_URL, where: "tab" },
  };
  const tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, EXAMPLE_URL);
  await SpecialMessageActions.handleAction(action, gBrowser);
  const browser = await tabPromise;
  ok(browser, "should open URL in a new tab");
  BrowserTestUtils.removeTab(browser);
});
