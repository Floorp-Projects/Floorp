/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ColorwayClosetOpener } = ChromeUtils.import(
  "resource:///modules/ColorwayClosetOpener.jsm"
);

add_task(async function test_open_firefoxview_and_colorways_modal() {
  const sandbox = sinon.createSandbox();
  const spy = sandbox.spy(ColorwayClosetOpener, "openModal");

  const tabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:firefoxview"
  );
  await SMATestUtils.executeAndValidateAction({
    type: "OPEN_FIREFOX_VIEW_AND_COLORWAYS_MODAL",
  });

  const tab = await tabPromise;

  ok(tab, "should open about:firefoxview in a new tab");
  ok(spy.calledOnce, "ColorwayClosetOpener's openModal was called once");

  BrowserTestUtils.removeTab(tab);
  sandbox.restore();
});
