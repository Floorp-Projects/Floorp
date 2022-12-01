/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const MONTH_YEAR = ".month-year",
  DAYS_VIEW = ".days-view",
  BTN_PREV_MONTH = ".prev",
  BTN_NEXT_MONTH = ".next";
const DATE_FORMAT = new Intl.DateTimeFormat("en-US", {
  year: "numeric",
  month: "long",
  timeZone: "UTC",
}).format;

function getCalendarText() {
  let calendarCells = [];
  for (const tr of helper.getChildren(DAYS_VIEW)) {
    for (const td of tr.children) {
      calendarCells.push(td.textContent);
    }
  }
  return calendarCells;
}

let helper = new DateTimeTestHelper();

registerCleanupFunction(() => {
  helper.cleanup();
});

/**
 * When the prev month button is clicked, calendar should display the dates for
 * the previous month.
 */
add_task(async function test_datepicker_prev_month_btn() {
  const inputValue = "2016-12-15";
  const prevMonth = "2016-11-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );
  helper.click(helper.getElement(BTN_PREV_MONTH));

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
    "2016-11"
  );

  await helper.tearDown();
});

/**
 * When the next month button is clicked, calendar should display the dates for
 * the next month.
 */
add_task(async function test_datepicker_next_month_btn() {
  const inputValue = "2016-12-15";
  const nextMonth = "2017-01-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );
  helper.click(helper.getElement(BTN_NEXT_MONTH));

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
    "2017-01"
  );

  await helper.tearDown();
});

/**
 * When a date on the calendar is clicked, date picker should close and set
 * value to the input box.
 */
add_task(async function test_datepicker_clicked() {
  info("When a calendar day is clicked, the picker closes, the value is set");
  const inputValue = "2016-12-15";
  const firstDayOnCalendar = "2016-11-27";

  await helper.openPicker(
    `data:text/html, <input id="date" type="date" value="${inputValue}">`
  );

  let browser = helper.tab.linkedBrowser;
  Assert.equal(helper.panel.state, "open", "Panel should be opened");

  // Click the first item (top-left corner) of the calendar
  let promise = BrowserTestUtils.waitForContentEvent(browser, "input");
  helper.click(helper.getElement(DAYS_VIEW).firstChild.firstChild);
  await promise;

  let value = await SpecialPowers.spawn(browser, [], () => {
    return content.document.querySelector("input").value;
  });

  Assert.equal(value, firstDayOnCalendar);

  await helper.tearDown();
});
