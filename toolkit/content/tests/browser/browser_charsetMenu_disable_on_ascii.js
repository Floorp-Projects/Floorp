/* Test that the charset menu is properly enabled when swapping browsers. */
add_task(async function test() {
  function charsetMenuEnabled() {
    return !document
      .getElementById("repair-text-encoding")
      .hasAttribute("disabled");
  }

  const PAGE = "data:text/html,<!DOCTYPE html><body>ASCII-only";
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: PAGE,
    waitForStateStop: true,
  });
  ok(!charsetMenuEnabled(), "should have a charset menu here");

  BrowserTestUtils.removeTab(tab);
});
