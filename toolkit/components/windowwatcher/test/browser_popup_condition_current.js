"use strict";

/**
 * Opens windows from content using window.open with several features patterns.
 */
add_task(async function test_popup_conditions_current() {
  // Non-popup is opened in a current tab.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 1]],
  });
  await testPopupPatterns("current");
  await SpecialPowers.popPrefEnv();
});
