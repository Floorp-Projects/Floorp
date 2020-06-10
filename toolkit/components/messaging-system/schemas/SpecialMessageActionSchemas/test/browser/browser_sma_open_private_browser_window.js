/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_OPEN_PRIVATE_BROWSER_WINDOW() {
  const newWindowPromise = BrowserTestUtils.waitForNewWindow();
  await SMATestUtils.executeAndValidateAction({
    type: "OPEN_PRIVATE_BROWSER_WINDOW",
  });
  const win = await newWindowPromise;
  ok(
    PrivateBrowsingUtils.isWindowPrivate(win),
    "should open a private browsing window"
  );
  await BrowserTestUtils.closeWindow(win);
});
