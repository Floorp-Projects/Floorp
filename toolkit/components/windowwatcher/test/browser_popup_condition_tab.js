"use strict";

/**
 * Opens windows from content using window.open with several features patterns.
 */
add_task(async function test_popup_conditions_tab() {
  // Non-popup is opened in a new tab (default behavior).
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 3]],
  });
  await testPopupPatterns("tab");
  await SpecialPowers.popPrefEnv();
});
