/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_OPEN_ABOUT_PAGE() {
  const tabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:logins?foo=bar"
  );
  await SMATestUtils.executeAndValidateAction({
    type: "OPEN_ABOUT_PAGE",
    data: { args: "logins", entrypoint: "foo=bar", where: "tabshifted" },
  });

  const tab = await tabPromise;
  ok(tab, "should open about page with entrypoint in a new tab by default");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_OPEN_ABOUT_PAGE_NEW_WINDOW() {
  const newWindowPromise = BrowserTestUtils.waitForNewWindow(
    gBrowser,
    "about:robots?foo=bar"
  );
  await SMATestUtils.executeAndValidateAction({
    type: "OPEN_ABOUT_PAGE",
    data: { args: "robots", entrypoint: "foo=bar", where: "window" },
  });

  const win = await newWindowPromise;
  ok(win, "should open about page in a new window");
  BrowserTestUtils.closeWindow(win);
});
