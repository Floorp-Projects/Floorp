/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  assertFirefoxViewTab,
  closeFirefoxViewTab,
  init: FirefoxViewTestUtilsInit,
} = ChromeUtils.importESModule(
  "resource://testing-common/FirefoxViewTestUtils.sys.mjs"
);
FirefoxViewTestUtilsInit(this);

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.firefox-view", true]],
  });

  CustomizableUI.addWidgetToArea(
    "firefox-view-button",
    CustomizableUI.AREA_TABSTRIP,
    0
  );

  registerCleanupFunction(async () => {
    // If you're running mochitest with --keep-open=true, and need to
    // easily tell whether the button really appeared, comment out the below
    // line so that the button hangs around after the test finishes.
    CustomizableUI.removeWidgetFromArea("firefox-view-button");
    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function test_open_firefox_view() {
  // setup
  let newTabOpened = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );

  // execute
  await SMATestUtils.executeAndValidateAction({
    type: "OPEN_FIREFOX_VIEW",
  });

  // verify
  await newTabOpened;
  assertFirefoxViewTab(window);

  // cleanup
  closeFirefoxViewTab(window);
});
