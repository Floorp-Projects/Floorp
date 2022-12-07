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

/**
 * Ensure the month spinner follows Page Up/Down key bindings appropriately.
 */
add_task(async function test_spinner_month_keyboard_pageup_pagedown() {
  info(
    "Ensure the month spinner follows Page Up/Down key bindings appropriately."
  );

  const inputValue = "2022-12-10";
  const nextFifthMonth = "2022-05-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );
  // Move focus from the selection to the month-year toggle button:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey("KEY_Tab", {});
  // Open month-year selection panel with spinners:
  EventUtils.synthesizeKey(" ", {});

  const spinnerMonthBtn = helper.getElement(SPINNER_MONTH).children[1];
  const spinnerYearBtn = helper.getElement(SPINNER_YEAR).children[1];

  // Move focus from the month-year toggle button to the month spinner:
  EventUtils.synthesizeKey("KEY_Tab", {});

  // Change the month-year from December 2022 to May 2022:
  EventUtils.synthesizeKey("KEY_PageDown", {});
  // It'd be good to use something else than waitForCondition for this but
  // there's no exposed event atm when the value changes from the child.
  await BrowserTestUtils.waitForCondition(() => {
    return (
      helper.getElement(MONTH_YEAR).textContent ==
      DATE_FORMAT(new Date(nextFifthMonth))
    );
  }, `Should change to May 2022, instead got ${helper.getElement(MONTH_YEAR).textContent}`);

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "4",
    "Page Down selects the fifth later month"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "Page Down on a month spinner does not update the year"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextFifthMonth)),
    "Page Down updates the month-year button to the fifth later month"
  );

  // Change the month-year from May 2022 to December 2022:
  EventUtils.synthesizeKey("KEY_PageUp", {});
  // It'd be good to use something else than waitForCondition for this but
  // there's no exposed event atm when the value changes from the child.
  await BrowserTestUtils.waitForCondition(() => {
    return (
      helper.getElement(MONTH_YEAR).textContent ==
      DATE_FORMAT(new Date(inputValue))
    );
  }, `Should change to December 2022, instead got ${helper.getElement(MONTH_YEAR).textContent}`);

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "11",
    "Page Up selects the fifth earlier month"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "Page Up on a month spinner does not update the year"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue)),
    "Page Up updates the month-year button to the fifth earlier month"
  );

  await helper.tearDown();
});

/**
 * Ensure the month spinner follows Home/End key bindings appropriately.
 */
add_task(async function test_spinner_month_keyboard_home_end() {
  info("Ensure the month spinner follows Home/End key bindings appropriately.");

  const inputValue = "2022-12-11";
  const firstMonth = "2022-01-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );
  // Move focus from the selection to the month-year toggle button:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey("KEY_Tab", {});
  // Open month-year selection panel with spinners:
  EventUtils.synthesizeKey(" ", {});

  const spinnerMonthBtn = helper.getElement(SPINNER_MONTH).children[1];
  const spinnerYearBtn = helper.getElement(SPINNER_YEAR).children[1];

  // Move focus from the month-year toggle button to the month spinner:
  EventUtils.synthesizeKey("KEY_Tab", {});

  // Change the month-year from December 2022 to January 2022:
  EventUtils.synthesizeKey("KEY_Home", {});
  // It'd be good to use something else than waitForCondition for this but
  // there's no exposed event atm when the value changes from the child.
  await BrowserTestUtils.waitForCondition(() => {
    return (
      helper.getElement(MONTH_YEAR).textContent ==
      DATE_FORMAT(new Date(firstMonth))
    );
  }, `Should change to January 2022, instead got ${helper.getElement(MONTH_YEAR).textContent}`);

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "0",
    "Home key selects the first month of the year (min value)"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "Home key does not update the year value"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(firstMonth)),
    "Home key updates the month-year button to the first month of the same year (min value)"
  );

  // Change the month-year from January 2022 to December 2022:
  EventUtils.synthesizeKey("KEY_End", {});
  // It'd be good to use something else than waitForCondition for this but
  // there's no exposed event atm when the value changes from the child.
  await BrowserTestUtils.waitForCondition(() => {
    return (
      helper.getElement(MONTH_YEAR).textContent ==
      DATE_FORMAT(new Date(inputValue))
    );
  }, `Should change to December 2022, instead got ${helper.getElement(MONTH_YEAR).textContent}`);

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "11",
    "End key selects the last month of the year (max value)"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "End key does not update the year value"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue)),
    "End key updates the month-year button to the last month of the same year (max value)"
  );

  await helper.tearDown();
});

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

/**
 * Ensure the year spinner follows Page Up/Down key bindings appropriately.
 */
add_task(async function test_spinner_year_keyboard_pageup_pagedown() {
  info(
    "Ensure the year spinner follows Page Up/Down key bindings appropriately."
  );

  const inputValue = "2022-12-10";
  const nextFifthYear = "2027-12-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );
  // Move focus from the selection to the month-year toggle button:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey("KEY_Tab", {});
  // Open month-year selection panel with spinners:
  EventUtils.synthesizeKey(" ", {});

  const spinnerMonthBtn = helper.getElement(SPINNER_MONTH).children[1];
  const spinnerYearBtn = helper.getElement(SPINNER_YEAR).children[1];

  // Move focus from the month-year toggle button to the year spinner:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey("KEY_Tab", {});

  // Change the month-year from December 2022 to December 2027:
  EventUtils.synthesizeKey("KEY_PageDown", {});
  // It'd be good to use something else than waitForCondition for this but
  // there's no exposed event atm when the value changes from the child.
  await BrowserTestUtils.waitForCondition(() => {
    return (
      helper.getElement(MONTH_YEAR).textContent ==
      DATE_FORMAT(new Date(nextFifthYear))
    );
  }, `Should change to December 2027, instead got ${helper.getElement(MONTH_YEAR).textContent}`);

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "11",
    "Page Down on the year spinner does not change the month"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2027",
    "Page Down selects the fifth later year"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextFifthYear)),
    "Page Down updates the month-year button to the fifth later year"
  );

  // Change the month-year from December 2027 to December 2022:
  EventUtils.synthesizeKey("KEY_PageUp", {});
  // It'd be good to use something else than waitForCondition for this but
  // there's no exposed event atm when the value changes from the child.
  await BrowserTestUtils.waitForCondition(() => {
    return (
      helper.getElement(MONTH_YEAR).textContent ==
      DATE_FORMAT(new Date(inputValue))
    );
  }, `Should change to December 2022, instead got ${helper.getElement(MONTH_YEAR).textContent}`);

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "11",
    "Page Up on the year spinner does not change the month"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "Page Up selects the fifth earlier year"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue)),
    "Page Up updates the month-year button to the fifth earlier year"
  );

  await helper.tearDown();
});

/**
 * Ensure the year spinner follows Home/End key bindings appropriately.
 */
add_task(async function test_spinner_year_keyboard_home_end() {
  info("Ensure the year spinner follows Home/End key bindings appropriately.");

  const inputValue = "2022-12-10";
  const minValue = "2020-10-10";
  const maxValue = "2030-12-31";
  const minYear = "2020-12-01";
  const maxYear = "2030-12-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}"  min="${minValue}" max="${maxValue}">`
  );
  // Move focus from the selection to the month-year toggle button:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey("KEY_Tab", {});
  // Open month-year selection panel with spinners:
  EventUtils.synthesizeKey(" ", {});

  const spinnerMonthBtn = helper.getElement(SPINNER_MONTH).children[1];
  const spinnerYearBtn = helper.getElement(SPINNER_YEAR).children[1];

  // Move focus from the month-year toggle button to the year spinner:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey("KEY_Tab", {});

  // Change the month-year from December 2022 to December 2020:
  EventUtils.synthesizeKey("KEY_Home", {});
  // It'd be good to use something else than waitForCondition for this but
  // there's no exposed event atm when the value changes from the child.
  await BrowserTestUtils.waitForCondition(() => {
    return (
      helper.getElement(MONTH_YEAR).textContent ==
      DATE_FORMAT(new Date(minYear))
    );
  }, `Should change to December 2020, instead got ${helper.getElement(MONTH_YEAR).textContent}`);

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "11",
    "Home key on the year spinner does not change the month"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2020",
    "Home key selects the min year value"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(minYear)),
    "Home key updates the month-year button to the min year value"
  );

  // Change the month-year from December 2022 to December 2030:
  EventUtils.synthesizeKey("KEY_End", {});

  // It'd be good to use something else than waitForCondition for this but
  // there's no exposed event atm when the value changes from the child.
  await BrowserTestUtils.waitForCondition(() => {
    return (
      helper.getElement(MONTH_YEAR).textContent ==
      DATE_FORMAT(new Date(maxYear))
    );
  }, `Should change to December 2030, instead got ${helper.getElement(MONTH_YEAR).textContent}`);

  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "11",
    "End key on the year spinner does not change the month"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2030",
    "End key selects the max year value"
  );
  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(maxYear)),
    "End key updates the month-year button to the max year value"
  );

  await helper.tearDown();
});
