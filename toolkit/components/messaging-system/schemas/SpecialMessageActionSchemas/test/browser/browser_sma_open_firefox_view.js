/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * The setup code and the utility funcitons here are cribbed from (mostly)
 * browser/components/firefoxview/test/browser/head.js
 *
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1784979 has been filed to move
 * these to some place publically accessible, after which we will be able to
 * a bunch of code from this file.
 */
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

function assertFirefoxViewTab(w = window) {
  ok(w.FirefoxViewHandler.tab, "Firefox View tab exists");
  ok(w.FirefoxViewHandler.tab?.hidden, "Firefox View tab is hidden");
  is(
    w.gBrowser.visibleTabs.indexOf(w.FirefoxViewHandler.tab),
    -1,
    "Firefox View tab is not in the list of visible tabs"
  );
}

function closeFirefoxViewTab(w = window) {
  w.gBrowser.removeTab(w.FirefoxViewHandler.tab);
  ok(
    !w.FirefoxViewHandler.tab,
    "Reference to Firefox View tab got removed when closing the tab"
  );
}

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
  assertFirefoxViewTab();

  // cleanup
  closeFirefoxViewTab();
});
