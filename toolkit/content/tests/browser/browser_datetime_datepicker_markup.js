/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const MONTH_YEAR = ".month-year",
  WEEK_HEADER = ".week-header",
  DAYS_VIEW = ".days-view",
  DAY_TODAY = ".today",
  DAY_SELECTED = ".selection",
  BTN_PREV_MONTH = ".prev",
  BTN_NEXT_MONTH = ".next",
  DIALOG_PICKER = "#date-picker",
  MONTH_YEAR_NAV = ".month-year-nav",
  MONTH_YEAR_VIEW = ".month-year-view",
  SPINNER_MONTH = "#spinner-month",
  SPINNER_YEAR = "#spinner-year";

let helper = new DateTimeTestHelper();

registerCleanupFunction(() => {
  helper.cleanup();
});

/**
 * Test that date picker opens with accessible markup
 */
add_task(async function test_datepicker_markup() {
  info("Test that the date picker opens with accessible markup");

  await helper.openPicker("data:text/html, <input type='date'>");

  Assert.equal(
    helper.getElement(DIALOG_PICKER).getAttribute("role"),
    "dialog",
    "Datepicker dialog has an appropriate ARIA role"
  );
  Assert.ok(
    helper.getElement(DIALOG_PICKER).getAttribute("aria-modal"),
    "Datepicker dialog is a modal"
  );
  Assert.equal(
    helper.getElement(BTN_PREV_MONTH).tagName,
    "button",
    "Previous Month control is a button"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).tagName,
    "button",
    "Month picker view toggle is a button"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).getAttribute("aria-expanded"),
    "false",
    "Month picker view toggle is collapsed when the dialog is hidden"
  );
  Assert.equal(
    helper.getElement(MONTH_YEAR).getAttribute("aria-live"),
    "polite",
    "Month picker view toggle is a live region when it's not expanded"
  );
  Assert.ok(
    BrowserTestUtils.is_hidden(helper.getElement(MONTH_YEAR_VIEW)),
    "Month-year selection spinner is not visible"
  );
  Assert.ok(
    BrowserTestUtils.is_hidden(helper.getElement(MONTH_YEAR_VIEW)),
    "Month-year selection spinner is programmatically hidden"
  );
  Assert.equal(
    helper.getElement(BTN_NEXT_MONTH).tagName,
    "button",
    "Next Month control is a button"
  );
  Assert.equal(
    helper.getElement(DAYS_VIEW).parentNode.tagName,
    "table",
    "Calendar view is marked up as a table"
  );
  Assert.equal(
    helper.getElement(DAYS_VIEW).parentNode.getAttribute("role"),
    "grid",
    "Calendar view is a grid"
  );
  Assert.ok(
    helper.getElement(
      `#${helper
        .getElement(DAYS_VIEW)
        .parentNode.getAttribute("aria-labelledby")}`
    ),
    "Calendar view has a valid accessible name"
  );
  Assert.equal(
    helper.getElement(WEEK_HEADER).firstChild.tagName,
    "tr",
    "Week headers within the Calendar view are marked up as table rows"
  );
  Assert.equal(
    helper.getElement(WEEK_HEADER).firstChild.firstChild.tagName,
    "th",
    "Weekdays within the Calendar view are marked up as header cells"
  );
  Assert.equal(
    helper.getElement(WEEK_HEADER).firstChild.firstChild.getAttribute("role"),
    "columnheader",
    "Weekdays within the Calendar view are grid column headers"
  );
  Assert.equal(
    helper.getElement(DAYS_VIEW).firstChild.tagName,
    "tr",
    "Weeks within the Calendar view are marked up as table rows"
  );
  Assert.equal(
    helper.getElement(DAYS_VIEW).firstChild.firstChild.tagName,
    "td",
    "Days within the Calendar view are marked up as table cells"
  );
  Assert.equal(
    helper.getElement(DAYS_VIEW).firstChild.firstChild.getAttribute("role"),
    "gridcell",
    "Days within the Calendar view are gridcells"
  );

  await helper.tearDown();
});

/**
 * Test that date picker has localizable labels
 */
add_task(async function test_datepicker_l10n() {
  info("Test that the date picker has localizable labels");

  await helper.openPicker("data:text/html, <input type='date'>");

  const testcases = [
    {
      selector: DIALOG_PICKER,
      id: "date-picker-label",
      args: null,
    },
    {
      selector: MONTH_YEAR_NAV,
      id: "date-spinner-label",
      args: null,
    },
    {
      selector: BTN_PREV_MONTH,
      id: "date-picker-previous",
      args: null,
    },
    {
      selector: BTN_NEXT_MONTH,
      id: "date-picker-next",
      args: null,
    },
  ];

  // Check "aria-label" attributes
  for (let { selector, id, args } of testcases) {
    const el = helper.getElement(selector);
    const l10nAttrs = document.l10n.getAttributes(el);

    Assert.ok(
      helper.getElement(selector).hasAttribute("aria-label"),
      `Datepicker "${selector}" element has accessible name`
    );
    Assert.deepEqual(
      l10nAttrs,
      {
        id,
        args,
      },
      `Datepicker "${selector}" element's accessible name is localizable`
    );
  }

  await helper.tearDown();
});

/**
 * Test that date picker opens to today's date, with today's and selected days
 * marked up correctly, given a date value is set.
 */
add_task(async function test_datepicker_today_and_selected() {
  info("Test today's and selected days' markup when a date value is set");

  const date = new Date();
  let inputValue = new Date();
  // Both 2 and 10 dates are used as an example only to test that
  // the current date and selected dates are marked up differently.
  if (date.getDate() === 2) {
    inputValue.setDate(10);
  } else {
    inputValue.setDate(2);
  }
  inputValue = inputValue.toISOString().split("T")[0];

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">  `
  );

  if (date.getMonth() === new Date().getMonth()) {
    Assert.notEqual(
      helper.getElement(DAY_TODAY),
      helper.getElement(DAY_SELECTED),
      "Today and selected dates are different"
    );
    Assert.equal(
      helper.getElement(DAY_TODAY).getAttribute("aria-current"),
      "date",
      "Today's date is programmatically current"
    );
    Assert.equal(
      helper.getElement(DAY_SELECTED).getAttribute("aria-selected"),
      "true",
      "Chosen date is programmatically selected"
    );
    Assert.ok(
      !helper.getElement(DAY_TODAY).hasAttribute("tabindex"),
      "Today is not included in the focus order, when another day is selected"
    );
    Assert.equal(
      helper.getElement(DAY_SELECTED).getAttribute("tabindex"),
      "0",
      "Selected date is included in the focus order"
    );
  } else {
    Assert.ok(
      true,
      "Skipping datepicker today test if month changes when opening picker."
    );
  }

  await helper.tearDown();
});

/**
 * Test that date picker refreshes ARIA properties
 * after the other month was displayed.
 */
add_task(async function test_datepicker_markup_refresh() {
  const inputValue = "2016-12-05";
  const minValue = "2016-12-05";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}" min="${minValue}">`
  );

  const secondRowDec = helper.getChildren(DAYS_VIEW)[1].children;

  // 2016-12-05 Monday is selected (in en_US locale)
  if (secondRowDec[1] === helper.getElement(DAY_SELECTED)) {
    Assert.equal(
      secondRowDec[1].getAttribute("aria-selected"),
      "true",
      "Chosen date is programmatically selected"
    );
    Assert.ok(
      !secondRowDec[1].classList.contains("out-of-range"),
      "Chosen date is not styled as out-of-range"
    );
    Assert.ok(
      !secondRowDec[1].hasAttribute("aria-disabled"),
      "Chosen date is not programmatically disabled"
    );
    // I.e. 2016-12-04 Sunday is out-of-range (in en_US locale)
    Assert.ok(
      secondRowDec[0].classList.contains("out-of-range"),
      "Less than min date is styled as out-of-range"
    );
    Assert.equal(
      secondRowDec[0].getAttribute("aria-disabled"),
      "true",
      "Less than min date is programmatically disabled"
    );

    // Change month view to check an updated markup
    helper.click(helper.getElement(BTN_NEXT_MONTH));

    const secondRowJan = helper.getChildren(DAYS_VIEW)[1].children;

    // 2017-01-02 Monday is not selected and in-range (in en_US locale)
    Assert.equal(
      secondRowJan[1].getAttribute("aria-selected"),
      "false",
      "Day with the same position as selected is not programmatically selected"
    );
    Assert.ok(
      !secondRowJan[1].classList.contains("out-of-range"),
      "Day with the same position as selected is not styled as out-of-range"
    );
    Assert.ok(
      !secondRowJan[1].hasAttribute("aria-disabled"),
      "Day with the same position as selected is not programmatically disabled"
    );
    // I.e. 2017-01-01 Sunday is in-range (in en_US locale)
    Assert.ok(
      !secondRowJan[0].classList.contains("out-of-range"),
      "Day with the same as less than min date is not styled as out-of-range"
    );
    Assert.ok(
      !secondRowJan[0].hasAttribute("aria-disabled"),
      "Day with the same as less than min date is not programmatically disabled"
    );
    Assert.equal(
      secondRowJan[0].getAttribute("tabindex"),
      "0",
      "The first day of the month is made focusable"
    );
    Assert.ok(
      !secondRowJan[1].hasAttribute("tabindex"),
      "Day with the same position as selected is not focusable"
    );
    Assert.ok(!helper.getElement(DAY_TODAY), "No date is marked up as today");
    Assert.ok(
      !helper.getElement(DAY_SELECTED),
      "No date is marked up as selected"
    );
  } else {
    Assert.ok(
      true,
      "Skipping datepicker attributes flushing test if the week/locale is different from the en_US used for the test"
    );
  }

  await helper.tearDown();
});
