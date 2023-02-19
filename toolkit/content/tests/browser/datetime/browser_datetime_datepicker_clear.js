/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function testClear(key) {
  const inputValue = "2023-03-03";
  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );
  let browser = helper.tab.linkedBrowser;

  Assert.equal(helper.panel.state, "open", "Panel should be opened");

  let closed = helper.promisePickerClosed();

  // Clear the input fields
  if (key) {
    // Move focus from the selected date to the Clear button:
    EventUtils.synthesizeKey("KEY_Tab", {});

    Assert.ok(
      helper.getElement(BTN_CLEAR).matches(":focus"),
      "The Clear button can receive keyboard focus"
    );

    EventUtils.synthesizeKey(key, {});
  } else {
    helper.click(helper.getElement(BTN_CLEAR));
  }

  await closed;

  await SpecialPowers.spawn(browser, [], () => {
    is(
      content.document.querySelector("input").value,
      "",
      "The input value is reset after the Clear button is pressed"
    );
  });

  await helper.tearDown();
}

add_task(async function test_datepicker_clear_keyboard() {
  await testClear(" ");
});

add_task(async function test_datepicker_clear_keyboard_enter() {
  await testClear("KEY_Enter");
});

add_task(async function test_datepicker_clear_mouse() {
  await testClear();
});
