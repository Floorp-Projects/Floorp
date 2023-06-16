/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

  await testCalendarBtnAttribute("aria-expanded", "true");

  let closed = helper.promisePickerClosed();

  // Close on Escape anywhere
  EventUtils.synthesizeKey("KEY_Escape", {});

  await closed;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed after Escape from anywhere on the window"
  );

  await testCalendarBtnAttribute("aria-expanded", "false");

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
  BrowserTestUtils.synthesizeKey("KEY_ArrowDown", {}, browser);

  // Toggle the picker on Space anywhere within the input
  BrowserTestUtils.synthesizeKey(" ", {}, browser);

  await ready;

  await testCalendarBtnAttribute("aria-expanded", "true");

  Assert.equal(
    helper.panel.state,
    "open",
    "Panel should be opened on Space from anywhere within the input field"
  );

  Assert.equal(
    helper.panel.querySelector("#dateTimePopupFrame").contentDocument
      .activeElement.textContent,
    "15",
    "Picker is opened with a focus set to the currently selected date"
  );

  let monthYearEl = helper.getElement(MONTH_YEAR);
  await BrowserTestUtils.waitForMutationCondition(
    monthYearEl,
    { childList: true },
    () => {
      return monthYearEl.textContent == DATE_FORMAT(new Date(prevMonth));
    },
    `Should change to November 2016, instead got ${
      helper.getElement(MONTH_YEAR).textContent
    }`
  );

  Assert.ok(
    true,
    "The date on both the Calendar and Month-Year button was updated when updating months with Down arrow key"
  );

  closed = helper.promisePickerClosed();

  // Close on Escape and return the focus to the input field  (the month input in en-US locale)
  EventUtils.synthesizeKey("KEY_Escape", {}, window);

  await closed;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed on Escape"
  );

  // Check the focus is returned to the Month field
  await SpecialPowers.spawn(browser, [], async () => {
    const input = content.document.querySelector("input");
    const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
    // Separators "/" are odd children of the wrapper
    const monthField = shadowRoot.getElementById("edit-wrapper").children[0];
    // Testing the focus position within content:
    Assert.equal(
      input,
      content.document.activeElement,
      `The input field includes programmatic focus`
    );
    // Testing the focus indication within the shadow-root:
    Assert.ok(
      monthField.matches(":focus"),
      `The keyboard focus was returned to the Month field`
    );
  });

  // Move focus to the second field (the day input in en-US locale)
  BrowserTestUtils.synthesizeKey("KEY_ArrowRight", {}, browser);

  // Change the day to 2016-12-16
  BrowserTestUtils.synthesizeKey("KEY_ArrowUp", {}, browser);

  ready = helper.waitForPickerReady();

  // Open the picker on Space within the input to check the date update
  await BrowserTestUtils.synthesizeKey(" ", {}, browser);

  await ready;

  await testCalendarBtnAttribute("aria-expanded", "true");

  Assert.equal(helper.panel.state, "open", "Panel should be opened on Space");

  let selectedDayEl = helper.getElement(DAY_SELECTED);
  await BrowserTestUtils.waitForMutationCondition(
    selectedDayEl,
    { childList: true },
    () => {
      return selectedDayEl.textContent === "16";
    },
    `Should change to the 16th, instead got ${
      helper.getElement(DAY_SELECTED).textContent
    }`
  );

  Assert.ok(
    true,
    "The date on the Calendar was updated when updating days with Up arrow key"
  );

  closed = helper.promisePickerClosed();

  // Close on Escape and return the focus to the input field  (the day input in en-US locale)
  EventUtils.synthesizeKey("KEY_Escape", {}, window);

  await closed;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed on Escape"
  );

  await testCalendarBtnAttribute("aria-expanded", "false");

  // Check the focus is returned to the Day field
  await SpecialPowers.spawn(browser, [], async () => {
    const input = content.document.querySelector("input");
    const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
    // Separators "/" are odd children of the wrapper
    const dayField = shadowRoot.getElementById("edit-wrapper").children[2];
    // Testing the focus position within content:
    Assert.equal(
      input,
      content.document.activeElement,
      `The input field includes programmatic focus`
    );
    // Testing the focus indication within the shadow-root:
    Assert.ok(
      dayField.matches(":focus"),
      `The keyboard focus was returned to the Day field`
    );
  });

  info("Test the Calendar button can toggle the picker with Enter/Space");

  // Move focus to the Calendar button
  BrowserTestUtils.synthesizeKey("KEY_Tab", {}, browser);
  BrowserTestUtils.synthesizeKey("KEY_Tab", {}, browser);

  // Toggle the picker on Enter on Calendar button
  await BrowserTestUtils.synthesizeKey("KEY_Enter", {}, browser);

  await helper.waitForPickerReady();

  Assert.equal(
    helper.panel.state,
    "open",
    "Panel should be opened on Enter from the Calendar button"
  );

  await testCalendarBtnAttribute("aria-expanded", "true");

  // Move focus from 2016-11-16 to 2016-11-17
  EventUtils.synthesizeKey("KEY_ArrowRight", {});

  // Make a selection by pressing Space on date gridcell
  await EventUtils.synthesizeKey(" ", {});

  await helper.promisePickerClosed();

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed on Space from the date gridcell"
  );
  await testCalendarBtnAttribute("aria-expanded", "false");

  // Check the focus is returned to the Calendar button
  await SpecialPowers.spawn(browser, [], async () => {
    const input = content.document.querySelector("input");
    const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
    const calendarBtn = shadowRoot.getElementById("calendar-button");
    // Testing the focus position within content:
    Assert.equal(
      input,
      content.document.activeElement,
      `The input field includes programmatic focus`
    );
    // Testing the focus indication within the shadow-root:
    Assert.ok(
      calendarBtn.matches(":focus"),
      `The keyboard focus was returned to the Calendar button`
    );
  });

  // Check the Backspace on Calendar button is not doing anything
  await EventUtils.synthesizeKey("KEY_Backspace", {});

  // The Calendar button is on its place and the input value is not changed
  // (bug 1804669)
  await SpecialPowers.spawn(browser, [], () => {
    const input = content.document.querySelector("input");
    const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
    const calendarBtn = shadowRoot.getElementById("calendar-button");
    Assert.equal(
      calendarBtn.children[0].tagName,
      "svg",
      `Calendar button has an <svg> child`
    );
    Assert.equal(input.value, "2016-11-17", `Input's value is not removed`);
  });

  // Toggle the picker on Space on Calendar button
  await EventUtils.synthesizeKey(" ", {});

  await helper.waitForPickerReady();

  Assert.equal(
    helper.panel.state,
    "open",
    "Panel should be opened on Space from the Calendar button"
  );

  await testCalendarBtnAttribute("aria-expanded", "true");

  await helper.tearDown();
});

/**
 * Ensure calendar follows Arrow key bindings appropriately.
 */
add_task(async function test_datepicker_keyboard_arrows() {
  info("Ensure calendar follows Arrow key bindings appropriately.");

  const inputValue = "2016-12-10";
  const prevMonth = "2016-11-01";
  await helper.openPicker(
    `data:text/html,<input id=date type=date value=${inputValue}>`
  );
  let pickerDoc = helper.panel.querySelector(
    "#dateTimePopupFrame"
  ).contentDocument;
  Assert.equal(helper.panel.state, "open", "Panel should be opened");

  // Move focus from 2016-12-10 to 2016-12-11:
  EventUtils.synthesizeKey("KEY_ArrowRight", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "11",
    "Arrow Right moves focus to the next day"
  );

  // Move focus from 2016-12-11 to 2016-12-04:
  EventUtils.synthesizeKey("KEY_ArrowUp", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "4",
    "Arrow Up moves focus to the same weekday of the previous week"
  );

  // Move focus from 2016-12-04 to 2016-12-03:
  EventUtils.synthesizeKey("KEY_ArrowLeft", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "3",
    "Arrow Left moves focus to the previous day"
  );

  // Move focus from 2016-12-03 to 2016-11-26:
  EventUtils.synthesizeKey("KEY_ArrowUp", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "26",
    "Arrow Up updates the view to be on the previous month, if needed"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevMonth)),
    "Arrow Up updates the spinner to show the previous month, if needed"
  );

  // Move focus from 2016-11-26 to 2016-12-03:
  EventUtils.synthesizeKey("KEY_ArrowDown", {});
  Assert.equal(
    pickerDoc.activeElement.textContent,
    "3",
    "Arrow Down updates the view to be on the next month, if needed"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue)),
    "Arrow Down updates the spinner to show the next month, if needed"
  );

  // Move focus from 2016-12-03 to 2016-12-10:
  EventUtils.synthesizeKey("KEY_ArrowDown", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "10",
    "Arrow Down moves focus to the same day of the next week"
  );

  await helper.tearDown();
});

/**
 * Ensure calendar follows Home/End key bindings appropriately.
 */
add_task(async function test_datepicker_keyboard_home_end() {
  info("Ensure calendar follows Home/End key bindings appropriately.");

  const inputValue = "2016-12-15";
  const prevMonth = "2016-11-01";
  await helper.openPicker(
    `data:text/html,<input id=date type=date value=${inputValue}>`
  );
  let pickerDoc = helper.panel.querySelector(
    "#dateTimePopupFrame"
  ).contentDocument;
  Assert.equal(helper.panel.state, "open", "Panel should be opened");

  // Move focus from 2016-12-15 to 2016-12-11 (in the en-US locale):
  EventUtils.synthesizeKey("KEY_Home", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "11",
    "Home key moves focus to the first day/Sunday of the current week"
  );

  // Move focus from 2016-12-11 to 2016-12-17 (in the en-US locale):
  EventUtils.synthesizeKey("KEY_End", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "17",
    "End key moves focus to the last day/Saturday of the current week"
  );

  // Move focus from 2016-12-17 to 2016-12-31:
  EventUtils.synthesizeKey("KEY_End", { ctrlKey: true });

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "31",
    "Ctrl + End keys move focus to the last day of the current month"
  );

  // Move focus from 2016-12-31 to 2016-12-01:
  EventUtils.synthesizeKey("KEY_Home", { ctrlKey: true });

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "1",
    "Ctrl + Home keys move focus to the first day of the current month"
  );

  // Move focus from 2016-12-01 to 2016-11-27 (in the en-US locale):
  EventUtils.synthesizeKey("KEY_Home", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "27",
    "Home key updates the view to be on the previous month, if needed"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevMonth)),
    "Home key updates the spinner to show the previous month, if needed"
  );

  // Move focus from 2016-11-27 to 2016-12-03 (in the en-US locale):
  EventUtils.synthesizeKey("KEY_End", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "3",
    "End key updates the view to be on the next month, if needed"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue)),
    "End key updates the spinner to show the next month, if needed"
  );

  await helper.tearDown();
});

/**
 * Ensure calendar follows Page Up/Down key bindings appropriately.
 */
add_task(async function test_datepicker_keyboard_pgup_pgdown() {
  info("Ensure calendar follows Page Up/Down key bindings appropriately.");

  const inputValue = "2023-01-31";
  const prevMonth = "2022-12-31";
  const prevYear = "2021-12-01";
  const nextMonth = "2023-01-31";
  const nextShortMonth = "2023-03-03";
  await helper.openPicker(
    `data:text/html,<input id=date type=date value=${inputValue}>`
  );
  let pickerDoc = helper.panel.querySelector(
    "#dateTimePopupFrame"
  ).contentDocument;
  Assert.equal(helper.panel.state, "open", "Panel should be opened");

  // Move focus from 2023-01-31 to 2022-12-31:
  EventUtils.synthesizeKey("KEY_PageUp", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "31",
    "Page Up key moves focus to the same day of the previous month"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevMonth)),
    "Page Up key updates the month-year button to show the previous month"
  );

  // Move focus from 2022-12-31 to 2022-12-01
  // (because 2022-11-31 does not exist):
  EventUtils.synthesizeKey("KEY_PageUp", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "1",
    `When the same day does not exists in the previous month Page Up key moves
    focus to the same day of the same week of the current month`
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevMonth)),
    `When the same day does not exist in the previous month
    Page Up key does not update the month-year button and shows the current month`
  );

  // Move focus from 2022-12-01 to 2021-12-01:
  EventUtils.synthesizeKey("KEY_PageUp", { shiftKey: true });
  Assert.equal(
    pickerDoc.activeElement.textContent,
    "1",
    "Page Up with Shift key moves focus to the same day of the same month of the previous year"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevYear)),
    "Page Up with Shift key updates the month-year button to show the same month of the previous year"
  );

  // Move focus from 2021-12-01 to 2022-12-01 month by month (bug 1806645):
  EventUtils.synthesizeKey("KEY_PageDown", { repeat: 12 });
  Assert.equal(
    pickerDoc.activeElement.textContent,
    "1",
    "When repeated, Page Down key moves focus to the same day of the same month of the next year"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevMonth)),
    "When repeated, Page Down key updates the month-year button to show the same month of the next year"
  );

  // Move focus from 2022-12-01 to 2021-12-01 month by month (bug 1806645):
  EventUtils.synthesizeKey("KEY_PageUp", { repeat: 12 });
  Assert.equal(
    pickerDoc.activeElement.textContent,
    "1",
    "When repeated, Page Up moves focus to the same day of the same month of the previous year"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevYear)),
    "When repeated, Page Up key updates the month-year button to show the same month of the previous year"
  );

  // Move focus from 2021-12-01 to 2022-12-01:
  EventUtils.synthesizeKey("KEY_PageDown", { shiftKey: true });
  Assert.equal(
    pickerDoc.activeElement.textContent,
    "1",
    "Page Down with Shift key moves focus to the same day of the same month of the next year"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevMonth)),
    "Page Down with Shift key updates the month-year button to show the same month of the next year"
  );

  // Move focus from 2016-12-01 to 2016-12-31:
  EventUtils.synthesizeKey("KEY_End", { ctrlKey: true });
  // Move focus from 2022-12-31 to 2023-01-31:
  EventUtils.synthesizeKey("KEY_PageDown", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "31",
    "Page Down key moves focus to the same day of the next month"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextMonth)),
    "Page Down key updates the month-year button to show the next month"
  );

  // Move focus from 2023-01-31 to 2023-03-03:
  EventUtils.synthesizeKey("KEY_PageDown", {});

  Assert.equal(
    pickerDoc.activeElement.textContent,
    "3",
    `When the same day does not exists in the next month, Page Down key moves
    focus to the same day of the same week of the month after`
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextShortMonth)),
    "Page Down key updates the month-year button to show the month after"
  );

  await helper.tearDown();
});
