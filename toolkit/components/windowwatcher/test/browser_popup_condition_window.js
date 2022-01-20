"use strict";

/**
 * Opens windows from content using window.open with several features patterns.
 */
add_task(async function test_popup_conditions_window() {
  // Non-popup is opened in a new window.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 2]],
  });
  await testPopupPatterns("window");
  await SpecialPowers.popPrefEnv();
});
