/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MONTH_YEAR = ".month-year",
  DAY_SELECTED = ".selection";
const DATE_FORMAT = new Intl.DateTimeFormat("en-US", {
  year: "numeric",
  month: "long",
  timeZone: "UTC",
}).format;

let helper = new DateTimeTestHelper();

registerCleanupFunction(() => {
  helper.cleanup();
});

/**
 * Ensure picker opens, closes, and updates its value with key bindings appropriately.
 */
add_task(async function test_datepicker_keyboard_nav() {
  info(
    "Ensure picker opens, closes, and updates its value with key bindings appropriately."
  );

  const inputValue = "2016-12-15";
  const prevMonth = "2016-11-01";
  await helper.openPicker(
    `data:text/html,<input id=date type=date value=${inputValue}>`
  );
  let browser = helper.tab.linkedBrowser;
  Assert.equal(helper.panel.state, "open", "Panel should be opened");

  let closed = helper.promisePickerClosed();

  // Close on Escape anywhere
  EventUtils.synthesizeKey("VK_ESCAPE", {});

  await closed;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed after Escape from anywhere on the window"
  );

  let ready = helper.waitForPickerReady();

  // Ensure focus is on the input field
  await SpecialPowers.spawn(browser, [], () => {
    content.document.querySelector("#date").focus();
  });

  info("Test that input updates with the keyboard update the picker");

  // NOTE: After a Tab, the first input field (the month one) is focused,
  // so down arrow will change the selected month.
  //
  // This assumes en-US locale, which seems fine for testing purposes (as
  // DATE_FORMAT and other bits around do the same).
  BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);

  // Toggle the picker on Space anywhere within the input
  BrowserTestUtils.synthesizeKey(" ", {}, browser);

  await ready;

  Assert.equal(
    helper.panel.state,
    "open",
    "Panel should be opened on Space from anywhere within the input field"
  );

  // It'd be good to use something else than waitForCondition for this but
  // there's no exposed event atm when the value changes from the child.
  await BrowserTestUtils.waitForCondition(() => {
    return (
      helper.getElement(MONTH_YEAR).textContent ==
      DATE_FORMAT(new Date(prevMonth))
    );
  }, `Should change to November 2016, instead got ${helper.getElement(MONTH_YEAR).textContent}`);

  Assert.ok(
    true,
    "The date on both the Calendar and Month-Year button was updated when updating months with Down arrow key"
  );

  closed = helper.promisePickerClosed();

  // Close on Escape and return the focus to the input field  (the month input in en-US locale)
  EventUtils.synthesizeKey("VK_ESCAPE", {}, window);

  await closed;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed on Escape"
  );

  // The focus should return to the input field.
  let isFocused = await SpecialPowers.spawn(browser, [], () => {
    return (
      content.document.querySelector("#date") === content.document.activeElement
    );
  });

  Assert.ok(isFocused, "<input> should again be focused");

  // Move focus to the second field (the day input in en-US locale)
  BrowserTestUtils.synthesizeKey("VK_RIGHT", {}, browser);

  // Change the day to 2016-12-16
  BrowserTestUtils.synthesizeKey("VK_UP", {}, browser);

  ready = helper.waitForPickerReady();

  // Open the picker on Space within the input to check the date update
  await BrowserTestUtils.synthesizeKey(" ", {}, browser);

  await ready;

  Assert.equal(helper.panel.state, "open", "Panel should be opened on Space");

  await BrowserTestUtils.waitForCondition(() => {
    return helper.getElement(DAY_SELECTED).textContent === "16";
  }, `Should change to the 16th, instead got ${helper.getElement(DAY_SELECTED).textContent}`);

  Assert.ok(
    true,
    "The date on the Calendar was updated when updating days with Up arrow key"
  );

  closed = helper.promisePickerClosed();

  // Close on Escape and return the focus to the input field  (the day input in en-US locale)
  EventUtils.synthesizeKey("VK_ESCAPE", {}, window);

  await closed;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed on Escape"
  );

  await helper.tearDown();
});
