/* Test that the charset menu is properly enabled when swapping browsers. */
add_task(async function test() {
  function charsetMenuEnabled() {
    return !document
      .getElementById("repair-text-encoding")
      .hasAttribute("disabled");
  }

  const PAGE =
    "data:text/html;charset=windows-1252,<!DOCTYPE html><body>hello %e4";
  let tab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: PAGE,
  });
  await BrowserTestUtils.waitForMutationCondition(
    document.getElementById("repair-text-encoding"),
    { attributes: true },
    charsetMenuEnabled
  );
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
