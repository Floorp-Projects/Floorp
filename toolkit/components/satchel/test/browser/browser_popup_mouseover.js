/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {FormHistory} = Cu.import("resource://gre/modules/FormHistory.jsm", {});


add_task(async function test() {
  const url = `data:text/html,<input type="text" name="field1">`;
  await BrowserTestUtils.withNewTab({gBrowser, url}, async function(browser) {
    const {autoCompletePopup, autoCompletePopup: {richlistbox: itemsBox}} = browser;
    const mockHistory = [
      {op: "add", fieldname: "field1", value: "value1"},
      {op: "add", fieldname: "field1", value: "value2"},
      {op: "add", fieldname: "field1", value: "value3"},
      {op: "add", fieldname: "field1", value: "value4"},
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
    await BrowserTestUtils.waitForCondition(() => {
      return autoCompletePopup.popupOpen;
    });
    const listItemElems = itemsBox.querySelectorAll(".autocomplete-richlistitem");
    is(listItemElems.length, mockHistory.length, "ensure result length");
    is(itemsBox.mousedOverIndex, -1, "mousedOverIndex should be -1");

    // navigate to the firt item
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    is(autoCompletePopup.selectedIndex, 0, "selectedIndex should be 0");

    // mouseover the second item
    EventUtils.synthesizeMouseAtCenter(listItemElems[1], {type: "mouseover"});
    await BrowserTestUtils.waitForCondition(() => {
      return itemsBox.mousedOverIndex = 1;
    });
    ok(true, "mousedOverIndex changed");
    is(autoCompletePopup.selectedIndex, 0, "selectedIndex should not be changed by mouseover");

    // close popup
    await ContentTask.spawn(browser, {}, async function() {
      const input = content.document.querySelector("input");

      input.blur();
    });
  });
});
