/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_open_firefoxview() {
  const tabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:firefoxview"
  );
  await SMATestUtils.executeAndValidateAction({
    type: "OPEN_FIREFOX_VIEW",
  });

  const tab = await tabPromise;

  ok(tab, "should open about:firefoxview in a new tab");

  BrowserTestUtils.removeTab(tab);
});
