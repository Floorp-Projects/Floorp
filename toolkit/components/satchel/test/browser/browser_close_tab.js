/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {FormHistory} = Cu.import("resource://gre/modules/FormHistory.jsm", {});

add_task(async function test() {
  const url = `data:text/html,<input type="text" name="field1">`;

  // Open a dummy tab.
  await BrowserTestUtils.withNewTab({gBrowser, url}, async function(browser) {});

  await BrowserTestUtils.withNewTab({gBrowser, url}, async function(browser) {
    const {autoCompletePopup} = browser;
    const mockHistory = [
      {op: "add", fieldname: "field1", value: "value1"},
    ];

    await new Promise(resolve =>
      FormHistory.update([{op: "remove"}, ...mockHistory],
                         {handleCompletion: resolve})
    );
    await ContentTask.spawn(browser, {}, async function() {
      const input = content.document.querySelector("input");

      input.focus();
    });

    // show popup
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await TestUtils.waitForCondition(() => {
      return autoCompletePopup.popupOpen;
    });

    let listener;
    let errorLogPromise = new Promise(resolve => {
      listener = ({message}) => {
        const ERROR_MSG = "AutoCompletePopup: No messageManager for " +
                          "message \"FormAutoComplete:PopupClosed\"";
        if (message.includes(ERROR_MSG)) {
          Assert.ok(true, "Got the error message for inexistent messageManager.");
          Services.console.unregisterListener(listener);
          resolve();
        }
      };
    });
    Services.console.registerListener(listener);

    gBrowser.removeCurrentTab();

    await TestUtils.waitForCondition(() => {
      return !autoCompletePopup.popupOpen;
    });

    Assert.ok(!autoCompletePopup.popupOpen, "Ensure the popup is closed.");

    await errorLogPromise;
  });
});
