/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Ensure navigating through Datepicker using keyboard after a date
 * has already been selected will keep the keyboard focus
 * when reaching a different month (bug 1804466).
 */
add_task(async function test_focus_after_selection() {
  info(
    `Ensure navigating through Datepicker using keyboard after a date has already been selected will not lose keyboard focus when reaching a different month.`
  );

  // Set "prefers-reduced-motion" media to "reduce"
  // to avoid intermittent scroll failures (1803612, 1803687)
  await SpecialPowers.pushPrefEnv({
    set: [["ui.prefersReducedMotion", 1]],
  });
  Assert.ok(
    matchMedia("(prefers-reduced-motion: reduce)").matches,
    "The reduce motion mode is active"
  );

  const inputValue = "2022-12-12";
  const prevMonth = "2022-10-01";
  const nextYear = "2023-11-01";
  const nextYearAfter = "2024-01-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value=${inputValue}>`
  );

  info("Test behavior when selection is done on the calendar grid");

  // Move focus from 2022-12-12 to 2022-10-24 by week
  // Changing 2 month views along the way:
  EventUtils.synthesizeKey("KEY_ArrowUp", { repeat: 7 });

  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(prevMonth)),
    "The calendar is updated to show the second previous month (2022-10)."
  );

  let closed = helper.promisePickerClosed();

  // Make a selection and close the picker
  EventUtils.synthesizeKey(" ");

  await closed;

  let ready = helper.waitForPickerReady();

  // Move the keyboard focus to the input field and reopen the picker
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey(" ");

  await ready;

  // Move focus from 2022-10-24 to 2022-12-12 by week
  // Changing 2 month views along the way:
  EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: 7 });

  // 2022-12-12:
  const focusedDay = getDayEl(12);

  let monthYearEl = helper.getElement(MONTH_YEAR);

  await BrowserTestUtils.waitForMutationCondition(
    monthYearEl,
    { childList: true },
    () => {
      return monthYearEl.textContent == DATE_FORMAT(new Date(inputValue));
    },
    `Should change to December 2022, instead got ${
      helper.getElement(MONTH_YEAR).textContent
    }`
  );
  Assert.equal(
    focusedDay,
    helper.getElement(DAYS_VIEW).querySelector('[tabindex="0"]'),
    "There is a focusable day within a calendar grid"
  );
  Assert.ok(
    focusedDay.matches(":focus"),
    "The focusable day within a calendar grid is focused"
  );

  info("Test behavior when selection is done on the month-year panel");

  // Move focus to the month-year toggle button and open it:
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 2 });
  EventUtils.synthesizeKey(" ");

  // Move focus to the month spin button and change its value
  // from December to November:
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_ArrowUp");

  // Move focus to the year spin button and change its value
  // from 2022 to 2023:
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_ArrowDown");

  await BrowserTestUtils.waitForMutationCondition(
    monthYearEl,
    { childList: true },
    () => {
      return monthYearEl.textContent == DATE_FORMAT(new Date(nextYear));
    },
    `Should change to November 2023, instead got ${
      helper.getElement(MONTH_YEAR).textContent
    }`
  );

  // Make a selection, close the month picker
  EventUtils.synthesizeKey(" ");

  Assert.ok(
    BrowserTestUtils.is_hidden(helper.getElement(MONTH_YEAR_VIEW)),
    "Month-year selection panel is not visible"
  );

  // Move focus from 2023-11-12 to 2024-01-07 by week
  // Changing 2 month views along the way:
  EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: 8 });

  // 2024-01-07:
  const newFocusedDay = getDayEl(7);

  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextYearAfter)),
    "The calendar is updated to show another month (2024-01)."
  );
  Assert.equal(
    newFocusedDay,
    helper.getElement(DAYS_VIEW).querySelector('[tabindex="0"]'),
    "There is a focusable day within a calendar grid"
  );
  Assert.ok(
    newFocusedDay.matches(":focus"),
    "The focusable day within a calendar grid is focused"
  );

  await helper.tearDown();
  await SpecialPowers.popPrefEnv();
});
