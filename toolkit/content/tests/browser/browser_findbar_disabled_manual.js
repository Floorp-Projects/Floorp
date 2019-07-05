const TEST_PAGE_URI = "data:text/html;charset=utf-8,The letter s.";

// Disable manual (FAYT) findbar hotkeys.
add_task(async function setup_test_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [["accessibility.typeaheadfind.manual", false]],
  });
});

// Makes sure that the findbar hotkeys (' and /) have no effect.
add_task(async function test_hotkey_disabled() {
  // Opening new tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PAGE_URI
  );
  let browser = gBrowser.getBrowserForTab(tab);
  let findbar = await gBrowser.getFindBar();

  // Pressing these keys open the findbar normally.
  const HOTKEYS = ["/", "'"];

  // Make sure no findbar appears when pressed.
  for (let key of HOTKEYS) {
    is(findbar.hidden, true, "Findbar is hidden now.");
    gBrowser.selectedTab = tab;
    await SimpleTest.promiseFocus(gBrowser.selectedBrowser);
    await BrowserTestUtils.sendChar(key, browser);
    is(findbar.hidden, true, "Findbar should still be hidden.");
  }

  gBrowser.removeTab(tab);
});
