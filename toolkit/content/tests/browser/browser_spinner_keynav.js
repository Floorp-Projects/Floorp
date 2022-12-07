/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const BTN_MONTH_YEAR = "#month-year-label",
  SPINNER_MONTH = "#spinner-month",
  SPINNER_YEAR = "#spinner-year",
  MONTH_YEAR = ".month-year";
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
 * Ensure the month spinner follows arrow key bindings appropriately.
 */
add_task(async function test_spinner_month_keyboard_arrows() {
  info("Ensure the month spinner follows arrow key bindings appropriately.");

  const inputValue = "2022-12-10";
  const nextMonth = "2022-01-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );
  let browser = helper.tab.linkedBrowser;
  let pickerDoc = helper.panel.querySelector("#dateTimePopupFrame")
    .contentDocument;

  info("Testing general keyboard navigation");

  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).getAttribute("aria-expanded"),
    "false",
    "Month-year button is collapsed when a picker is opened (by default)"
  );

  // Move focus from the selection to the month-year toggle button:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey("KEY_Tab", {});
  // Open month-year selection panel with spinners:
  EventUtils.synthesizeKey(" ", {});

  const spinnerMonthBtn = helper.getElement(SPINNER_MONTH).children[1];
  const spinnerYearBtn = helper.getElement(SPINNER_YEAR).children[1];

  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).getAttribute("aria-expanded"),
    "true",
    "Month-year button is expanded when the spinners are shown"
  );
  // December 2022 is an example:
  Assert.equal(
    pickerDoc.activeElement.textContent,
    DATE_FORMAT(new Date(inputValue)),
    "Month-year toggle button is focused"
  );
  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "11",
    "Month Spinner control is ready"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "Year Spinner control is ready"
  );

  // Move focus from the month-year toggle button to the month spinner:
  EventUtils.synthesizeKey("KEY_Tab", {});

  Assert.equal(
    pickerDoc.activeElement.getAttribute("aria-valuenow"),
    "11",
    "Tab moves focus to the month spinner"
  );

  info("Testing Up/Down Arrow keys behavior of the Month Spinner");

  // Change the month-year from December 2022 to January 2022:
  EventUtils.synthesizeKey("VK_DOWN", {});
  await BrowserTestUtils.waitForContentEvent(browser, "input");

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "0",
    "Down Arrow selects the next month"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "Down Arrow on a month spinner does not update the year"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextMonth)),
    "Down Arrow updates the month-year button to the next month"
  );

  // Change the month-year from January 2022 to December 2022:
  await EventUtils.synthesizeKey("VK_UP", {});
  await BrowserTestUtils.waitForContentEvent(browser, "input");

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "11",
    "Up Arrow selects the previous month"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "Up Arrow on a month spinner does not update the year"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue)),
    "Up Arrow updates the month-year button to the previous month"
  );

  await helper.tearDown();
});

// PageUp/PageDown and Home/End Tests are to be added per bug 1803664

/**
 * Ensure the year spinner follows arrow key bindings appropriately.
 */
add_task(async function test_spinner_year_keyboard_arrows() {
  info("Ensure the year spinner follows arrow key bindings appropriately.");

  const inputValue = "2022-12-10";
  const nextYear = "2023-12-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );
  let browser = helper.tab.linkedBrowser;
  let pickerDoc = helper.panel.querySelector("#dateTimePopupFrame")
    .contentDocument;

  info("Testing general keyboard navigation");

  // Move focus from the selection to the month-year toggle button:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey("KEY_Tab", {});
  // Open month-year selection panel with spinners:
  EventUtils.synthesizeKey(" ", {});

  const spinnerMonthBtn = helper.getElement(SPINNER_MONTH).children[1];
  const spinnerYearBtn = helper.getElement(SPINNER_YEAR).children[1];

  // December 2022 is an example:
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "Year Spinner control is ready"
  );

  // Move focus from the month-year toggle button to the year spinner:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey("KEY_Tab", {});

  Assert.equal(
    pickerDoc.activeElement.getAttribute("aria-valuenow"),
    "2022",
    "Tab can move the focus to the year spinner"
  );

  info("Testing Up/Down Arrow keys behavior of the Year Spinner");

  // Change the month-year from December 2022 to December 2023:
  EventUtils.synthesizeKey("VK_DOWN", {});
  await BrowserTestUtils.waitForContentEvent(browser, "input");

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "11",
    "Down Arrow on the year spinner does not change the month"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2023",
    "Down Arrow updates the year to the next"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextYear)),
    "Down Arrow updates the month-year button to the next year"
  );

  // Change the month-year from December 2023 to December 2022:
  EventUtils.synthesizeKey("VK_UP", {});
  await BrowserTestUtils.waitForContentEvent(browser, "input");

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "11",
    "Up Arrow on the year spinner does not change the month"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "Up Arrow updates the year to the previous"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue)),
    "Up Arrow updates the month-year button to the previous year"
  );

  await helper.tearDown();
});

// PageUp/PageDown and Home/End Tests are to be added per bug 1803664
