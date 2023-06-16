/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * When the Previous Month button is pressed, calendar should display
 * the dates for the previous month.
 */
add_task(async function test_datepicker_prev_month_btn() {
  const inputValue = "2016-12-15";
  const prevMonth = "2016-11-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );

  // Move focus from the selected date to the Previous Month button:
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 2 });
  EventUtils.synthesizeKey(" ", {});

  // 2016-11-15:
  const focusableDay = getDayEl(15);

  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevMonth))
  );
  Assert.deepEqual(
    getCalendarText(),
    [
      "30",
      "31",
      "1",
      "2",
      "3",
      "4",
      "5",
      "6",
      "7",
      "8",
      "9",
      "10",
      "11",
      "12",
      "13",
      "14",
      "15",
      "16",
      "17",
      "18",
      "19",
      "20",
      "21",
      "22",
      "23",
      "24",
      "25",
      "26",
      "27",
      "28",
      "29",
      "30",
      "1",
      "2",
      "3",
      "4",
      "5",
      "6",
      "7",
      "8",
      "9",
      "10",
    ],
    "The calendar is updated to show the previous month (2016-11)"
  );
  Assert.ok(
    helper.getElement(BTN_PREV_MONTH).matches(":focus"),
    "Focus stays on a Previous Month button after it's pressed"
  );
  Assert.equal(
    focusableDay.textContent,
    "15",
    "The same day of the month is present within a calendar grid"
  );
  Assert.equal(
    focusableDay,
    helper.getElement(DAYS_VIEW).querySelector('[tabindex="0"]'),
    "The same day of the month is focusable within a calendar grid"
  );

  // Move focus from the Previous Month button to the same day of the month (2016-11-15):
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 3 });

  Assert.ok(
    focusableDay.matches(":focus"),
    "The same day of the previous month can be focused with a keyboard"
  );

  await helper.tearDown();
});

/**
 * When the Next Month button is clicked, calendar should display the dates for
 * the next month.
 */
add_task(async function test_datepicker_next_month_btn() {
  const inputValue = "2016-12-15";
  const nextMonth = "2017-01-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );

  // Move focus from the selected date to the Next Month button:
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 4 });
  EventUtils.synthesizeKey(" ", {});

  // 2017-01-15:
  const focusableDay = getDayEl(15);

  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextMonth))
  );
  Assert.deepEqual(
    getCalendarText(),
    [
      "25",
      "26",
      "27",
      "28",
      "29",
      "30",
      "31",
      "1",
      "2",
      "3",
      "4",
      "5",
      "6",
      "7",
      "8",
      "9",
      "10",
      "11",
      "12",
      "13",
      "14",
      "15",
      "16",
      "17",
      "18",
      "19",
      "20",
      "21",
      "22",
      "23",
      "24",
      "25",
      "26",
      "27",
      "28",
      "29",
      "30",
      "31",
      "1",
      "2",
      "3",
      "4",
    ],
    "The calendar is updated to show the next month (2017-01)."
  );
  Assert.ok(
    helper.getElement(BTN_NEXT_MONTH).matches(":focus"),
    "Focus stays on a Next Month button after it's pressed"
  );
  Assert.equal(
    focusableDay.textContent,
    "15",
    "The same day of the month is present within a calendar grid"
  );
  Assert.equal(
    focusableDay,
    helper.getElement(DAYS_VIEW).querySelector('[tabindex="0"]'),
    "The same day of the month is focusable within a calendar grid"
  );

  // Move focus from the Next Month button to the same day of the month (2017-01-15):
  EventUtils.synthesizeKey("KEY_Tab", {});

  Assert.ok(
    focusableDay.matches(":focus"),
    "The same day of the next month can be focused with a keyboard"
  );

  await helper.tearDown();
});

/**
 * When the Previous Month button is pressed, calendar should display
 * the dates for the previous month on RTL build (bug 1806823).
 */
add_task(async function test_datepicker_prev_month_btn_rtl() {
  const inputValue = "2016-12-15";
  const prevMonth = "2016-11-01";

  await SpecialPowers.pushPrefEnv({ set: [["intl.l10n.pseudo", "bidi"]] });

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );

  // Move focus from the selected date to the Previous Month button:
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 2 });
  EventUtils.synthesizeKey(" ", {});

  // 2016-11-15:
  const focusableDay = getDayEl(15);

  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevMonth)),
    "The calendar is updated to show the previous month (2016-11)"
  );
  Assert.ok(
    helper.getElement(BTN_PREV_MONTH).matches(":focus"),
    "Focus stays on a Previous Month button after it's pressed"
  );
  Assert.equal(
    focusableDay.textContent,
    "15",
    "The same day of the month is present within a calendar grid"
  );
  Assert.equal(
    focusableDay,
    helper.getElement(DAYS_VIEW).querySelector('[tabindex="0"]'),
    "The same day of the month is focusable within a calendar grid"
  );

  // Move focus from the Previous Month button to the same day of the month (2016-11-15):
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 3 });

  Assert.ok(
    focusableDay.matches(":focus"),
    "The same day of the previous month can be focused with a keyboard"
  );

  await helper.tearDown();
  await SpecialPowers.popPrefEnv();
});

/**
 * When the Next Month button is clicked, calendar should display the dates for
 * the next month on RTL build (bug 1806823).
 */
add_task(async function test_datepicker_next_month_btn_rtl() {
  const inputValue = "2016-12-15";
  const nextMonth = "2017-01-01";

  await SpecialPowers.pushPrefEnv({ set: [["intl.l10n.pseudo", "bidi"]] });

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );

  // Move focus from the selected date to the Next Month button:
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 4 });
  EventUtils.synthesizeKey(" ", {});

  // 2017-01-15:
  const focusableDay = getDayEl(15);

  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextMonth)),
    "The calendar is updated to show the next month (2017-01)."
  );
  Assert.ok(
    helper.getElement(BTN_NEXT_MONTH).matches(":focus"),
    "Focus stays on a Next Month button after it's pressed"
  );
  Assert.equal(
    focusableDay.textContent,
    "15",
    "The same day of the month is present within a calendar grid"
  );
  Assert.equal(
    focusableDay,
    helper.getElement(DAYS_VIEW).querySelector('[tabindex="0"]'),
    "The same day of the month is focusable within a calendar grid"
  );

  // Move focus from the Next Month button to the same day of the month (2017-01-15):
  EventUtils.synthesizeKey("KEY_Tab", {});

  Assert.ok(
    focusableDay.matches(":focus"),
    "The same day of the next month can be focused with a keyboard"
  );

  await helper.tearDown();
  await SpecialPowers.popPrefEnv();
});

/**
 * When Previous/Next Month buttons or arrow keys are used to change a month view
 * when a time value is incomplete for datetime-local inputs,
 * calendar should update the month (bug 1817785).
 */
add_task(async function test_datepicker_reopened_prev_next_month_btn() {
  info("Setup a datetime-local datepicker to its reopened state for testing");

  let inputValueDT = "2023-05-02T01:01";
  let prevMonth = new Date("2023-04-02");

  await helper.openPicker(
    `data:text/html, <input type="datetime-local" value="${inputValueDT}">`
  );

  let closed = helper.promisePickerClosed();
  EventUtils.synthesizeKey("KEY_Escape", {});
  await closed;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Date picker panel should be closed"
  );

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const input = content.document.querySelector("input");
    const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
    const editFields = shadowRoot.querySelectorAll(".datetime-edit-field");
    const amPm = editFields[5];
    amPm.focus();

    Assert.ok(
      amPm.matches(":focus"),
      "Period of the day within the input is focused"
    );
  });

  // Use Backspace key to clear the value of the AM/PM section of the input
  // and wait for input.value to change to null (bug 1833988):
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const input = content.document.querySelector("input");

    const EventUtils = ContentTaskUtils.getEventUtils(content);
    EventUtils.synthesizeKey("KEY_Backspace", {}, content);

    await ContentTaskUtils.waitForMutationCondition(
      input,
      { attributeFilter: ["value"] },
      () => input.value == ""
    );

    Assert.ok(
      !input.value,
      `Expected an input value to be changed to 'null' when a time value became incomplete, instead got ${input.value}`
    );
  });

  let ready = helper.waitForPickerReady();

  // Move focus to a day section of the input and open a picker:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey(" ", {});

  await ready;

  Assert.equal(
    helper.panel.querySelector("#dateTimePopupFrame").contentDocument
      .activeElement.textContent,
    "2",
    "Picker is opened with a focus set to the currently selected date"
  );

  info("Test the Previous Month button behavior");

  // Move focus from the selected date to the Previous Month button,
  // and activate it to move calendar from 2023-05-02 to 2023-04-02:
  EventUtils.synthesizeKey("KEY_Tab", {
    repeat: 2,
  });
  EventUtils.synthesizeKey(" ", {});

  // Same date of the previous month should be visible and focusable
  // (2023-04-02) but the focus should remain on the Previous Month button:
  const focusableDayPrevMonth = getDayEl(2);
  const monthYearEl = helper.getElement(MONTH_YEAR);
  await BrowserTestUtils.waitForMutationCondition(
    monthYearEl,
    {
      childList: true,
    },
    () => {
      return monthYearEl.textContent == DATE_FORMAT(new Date(prevMonth));
    },
    `Should change to the previous month (April 2023), instead got ${
      helper.getElement(MONTH_YEAR).textContent
    }`
  );
  Assert.ok(
    true,
    `The date on both the Calendar and Month-Year button was updated
    when Previous Month button was used`
  );
  Assert.ok(
    helper.getElement(BTN_PREV_MONTH).matches(":focus"),
    "Focus stays on a Previous Month button after it's pressed"
  );
  Assert.equal(
    focusableDayPrevMonth,
    helper.getElement(DAYS_VIEW).querySelector('[tabindex="0"]'),
    "The same day of the month is focusable within a calendar grid"
  );
  Assert.equal(
    focusableDayPrevMonth.textContent,
    "2",
    "The same day of the month is present within a calendar grid"
  );

  // Move focus from the Previous Month button to the same day of the month (2023-04-02):
  EventUtils.synthesizeKey("KEY_Tab", {
    repeat: 3,
  });

  Assert.ok(
    focusableDayPrevMonth.matches(":focus"),
    "The same day of the previous month can be focused with a keyboard"
  );

  info("Test the Next Month button behavior");

  // Move focus from the focused date to the Next Month button and activate it,
  // (from 2023-04-02 to 2023-05-02):
  EventUtils.synthesizeKey("KEY_Tab", {
    repeat: 4,
  });
  EventUtils.synthesizeKey(" ", {});

  // Same date of the next month should be visible and focusable
  // (2023-05-02) but the focus should remain on the Next Month button:
  const focusableDayNextMonth = getDayEl(2);
  await BrowserTestUtils.waitForMutationCondition(
    monthYearEl,
    {
      childList: true,
    },
    () => {
      return monthYearEl.textContent == DATE_FORMAT(new Date(inputValueDT));
    },
    `Should change to May 2023, instead got ${
      helper.getElement(MONTH_YEAR).textContent
    }`
  );
  Assert.ok(
    true,
    `The date on both the Calendar and Month-Year button was updated
    when Next Month button was used`
  );
  Assert.ok(
    helper.getElement(BTN_NEXT_MONTH).matches(":focus"),
    "Focus stays on a Next Month button after it's pressed"
  );
  Assert.equal(
    focusableDayNextMonth,
    helper.getElement(DAYS_VIEW).querySelector('[tabindex="0"]'),
    "The same day of the month is focusable within a calendar grid"
  );
  Assert.equal(
    focusableDayNextMonth.textContent,
    "2",
    "The same day of the month is present within a calendar grid"
  );

  // Move focus from the Next Month button to the focusable day of the month (2023-05-02):
  EventUtils.synthesizeKey("KEY_Tab", {});

  Assert.ok(
    focusableDayNextMonth.matches(":focus"),
    "The same day of the month can be focused with a keyboard"
  );

  info("Test the arrow navigation behavior");

  // Move focus from the focused date to the same weekday of the previous month,
  // (From 2023-05-02 to 2023-04-25):
  EventUtils.synthesizeKey("KEY_ArrowUp", {});

  await BrowserTestUtils.waitForMutationCondition(
    monthYearEl,
    {
      childList: true,
    },
    () => {
      return monthYearEl.textContent == DATE_FORMAT(new Date(prevMonth));
    },
    `Should change to the previous month, instead got ${
      helper.getElement(MONTH_YEAR).textContent
    }`
  );
  Assert.ok(
    true,
    `The date on both the Calendar and Month-Year button was updated
    when an Up Arrow key was used`
  );

  // Move focus from the focused date to the same weekday of the next month,
  // (from 2023-04-25 to 2023-05-02):
  EventUtils.synthesizeKey("KEY_ArrowDown", {});

  await BrowserTestUtils.waitForMutationCondition(
    monthYearEl,
    {
      childList: true,
    },
    () => {
      return monthYearEl.textContent == DATE_FORMAT(new Date(inputValueDT));
    },
    `Should change to the previous month, instead got ${
      helper.getElement(MONTH_YEAR).textContent
    }`
  );
  Assert.ok(
    true,
    `The date on both the Calendar and Month-Year button was updated
    when a Down Arrow key was used`
  );

  await helper.tearDown();
});
