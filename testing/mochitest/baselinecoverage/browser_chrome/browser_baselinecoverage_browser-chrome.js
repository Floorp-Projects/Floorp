/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

add_task(async function () {
  requestLongerTimeout(2);
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:blank",
    },
    async function () {
      ok(true, "Collecting baseline coverage for browser-chrome tests.");
      await new Promise(c => setTimeout(c, 30 * 1000));
    }
  );

  await BrowserTestUtils.closeWindow(newWin);
});
