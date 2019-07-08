/* Test that the charset menu is properly enabled when swapping browsers. */
add_task(async function test() {
  // NB: This test cheats and calls updateCharacterEncodingMenuState directly
  // instead of opening the "View" menu.
  function charsetMenuEnabled() {
    updateCharacterEncodingMenuState();
    return !document.getElementById("charsetMenu").hasAttribute("disabled");
  }

  const PAGE = "data:text/html,<!DOCTYPE html><body>hello";
  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: PAGE,
  });
  ok(charsetMenuEnabled(), "should have a charset menu here");

  let tab2 = await BrowserTestUtils.openNewForegroundTab({ gBrowser });
  ok(!charsetMenuEnabled(), "about:blank shouldn't have a charset menu");

  await BrowserTestUtils.switchTab(gBrowser, tab1);

  let swapped = BrowserTestUtils.waitForEvent(
    tab2.linkedBrowser,
    "SwapDocShells"
  );

  // NB: Closes tab1.
  gBrowser.swapBrowsersAndCloseOther(tab2, tab1);
  await swapped;

  ok(charsetMenuEnabled(), "should have a charset after the swap");

  BrowserTestUtils.removeTab(tab2);
});
