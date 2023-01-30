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
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey(" ");

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
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 3 });
  EventUtils.synthesizeKey(" ");

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
  EventUtils.synthesizeKey("KEY_Tab");

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
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey(" ");

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
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 3 });
  EventUtils.synthesizeKey(" ");

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
  EventUtils.synthesizeKey("KEY_Tab");

  Assert.ok(
    focusableDay.matches(":focus"),
    "The same day of the next month can be focused with a keyboard"
  );

  await helper.tearDown();
  await SpecialPowers.popPrefEnv();
});
