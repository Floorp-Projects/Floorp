/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FormHistory } = ChromeUtils.import(
  "resource://gre/modules/FormHistory.jsm"
);

add_task(async function test() {
  const url = `data:text/html,<input type="text" name="field1">`;

  // Open a dummy tab.
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function(
    browser
  ) {});

  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function(browser) {
    const { autoCompletePopup } = browser;
    const mockHistory = [{ op: "add", fieldname: "field1", value: "value1" }];

    await new Promise(resolve =>
      FormHistory.update([{ op: "remove" }, ...mockHistory], {
        handleCompletion: resolve,
      })
    );
    await SpecialPowers.spawn(browser, [], async function() {
      const input = content.document.querySelector("input");

      input.focus();
    });

    // show popup
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await TestUtils.waitForCondition(() => {
      return autoCompletePopup.popupOpen;
    });

    gBrowser.removeCurrentTab();

    await TestUtils.waitForCondition(() => {
      return !autoCompletePopup.popupOpen;
    });

    Assert.ok(!autoCompletePopup.popupOpen, "Ensure the popup is closed.");
  });
});
