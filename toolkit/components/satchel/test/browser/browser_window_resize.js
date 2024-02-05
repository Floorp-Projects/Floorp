/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function waitForAutoCompletePopup(aAutoCompletePopup, aOpened) {
  await BrowserTestUtils.waitForPopupEvent(
    aAutoCompletePopup,
    aOpened ? "shown" : "hidden"
  );
  Assert.equal(
    aAutoCompletePopup.popupOpen,
    aOpened,
    `Check autocomplete popup is ${aOpened ? "opened" : "closed"}`
  );
}

add_task(async function test_window_resized() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    `data:text/html,
      <input id="list-input" list="test-list" type="text"/>
      <datalist id="test-list">
        <option value="item 1"/>
        <option value="item 2"/>
        <option value="item 3"/>
        <option value="item 4"/>
      </datalist>`
  );

  const browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [], async function () {
    const input = content.document.querySelector("input");
    input.focus();
  });

  // Show popup
  const { autoCompletePopup } = browser;
  const shown = waitForAutoCompletePopup(autoCompletePopup, true);
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
  await shown;

  // Popup should be closed after window is resized
  const hidden = waitForAutoCompletePopup(autoCompletePopup, false);
  win.resizeBy(100, 100);
  await hidden;

  // Should be able to show popup again
  const shownAgain = waitForAutoCompletePopup(autoCompletePopup, true);
  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
  await shownAgain;

  // Close opened tab
  let tabClosed = BrowserTestUtils.waitForTabClosing(tab);
  await BrowserTestUtils.removeTab(tab);
  await tabClosed;

  // Close opened window
  await BrowserTestUtils.closeWindow(win);
});
