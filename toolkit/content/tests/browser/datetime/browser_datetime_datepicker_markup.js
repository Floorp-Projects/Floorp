/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
    BrowserTestUtils.isHidden(helper.getElement(MONTH_YEAR_VIEW)),
    "Month-year selection spinner is not visible"
  );
  Assert.ok(
    BrowserTestUtils.isHidden(helper.getElement(MONTH_YEAR_VIEW)),
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
  Assert.equal(
    helper.getElement(BTN_CLEAR).tagName,
    "button",
    "Clear control is a button"
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
    {
      selector: BTN_CLEAR,
      id: "date-picker-clear-button",
      args: null,
    },
  ];

  // Check "aria-label" attributes
  for (let { selector, id, args } of testcases) {
    const el = helper.getElement(selector);
    const l10nAttrs = document.l10n.getAttributes(el);

    Assert.ok(
      el.hasAttribute("aria-label") || el.textContent,
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

    // Change month view from December 2016 to January 2017
    // to check an updated markup
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
    // 2016-12-05 was focused before the change, thus the same day of the month
    // is expected to be focused now (2017-01-05):
    Assert.equal(
      secondRowJan[4].getAttribute("tabindex"),
      "0",
      "The same day of the month is made focusable"
    );
    Assert.ok(
      !secondRowJan[0].hasAttribute("tabindex"),
      "The first day of the month is not focusable"
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

/**
 * Test that date input field has a Calendar button with an accessible markup
 */
add_task(async function test_calendar_button_markup_date() {
  info(
    "Test that type=date input field has a Calendar button with an accessible markup"
  );

  await helper.openPicker("data:text/html, <input type='date'>");
  let browser = helper.tab.linkedBrowser;

  Assert.equal(helper.panel.state, "open", "Panel is visible");

  let closed = helper.promisePickerClosed();

  await testCalendarBtnAttribute("aria-expanded", "true");
  await testCalendarBtnAttribute("aria-label", null, true);
  await testCalendarBtnAttribute("data-l10n-id", "datetime-calendar");

  await SpecialPowers.spawn(browser, [], () => {
    const input = content.document.querySelector("input");
    const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
    const calendarBtn = shadowRoot.getElementById("calendar-button");

    Assert.equal(calendarBtn.tagName, "BUTTON", "Calendar control is a button");
    Assert.ok(
      ContentTaskUtils.isVisible(calendarBtn),
      "The Calendar button is visible"
    );

    calendarBtn.click();
  });

  await closed;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed on click on the Calendar button"
  );

  await testCalendarBtnAttribute("aria-expanded", "false");

  await helper.tearDown();
});

/**
 * Test that datetime-local input field has a Calendar button
 * with an accessible markup
 */
add_task(async function test_calendar_button_markup_datetime() {
  info(
    "Test that type=datetime-local input field has a Calendar button with an accessible markup"
  );

  await helper.openPicker("data:text/html, <input type='datetime-local'>");
  let browser = helper.tab.linkedBrowser;

  Assert.equal(helper.panel.state, "open", "Panel is visible");

  let closed = helper.promisePickerClosed();

  await testCalendarBtnAttribute("aria-expanded", "true");
  await testCalendarBtnAttribute("aria-label", null, true);
  await testCalendarBtnAttribute("data-l10n-id", "datetime-calendar");

  await SpecialPowers.spawn(browser, [], () => {
    const input = content.document.querySelector("input");
    const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
    const calendarBtn = shadowRoot.getElementById("calendar-button");

    Assert.equal(calendarBtn.tagName, "BUTTON", "Calendar control is a button");
    Assert.ok(
      ContentTaskUtils.isVisible(calendarBtn),
      "The Calendar button is visible"
    );

    calendarBtn.click();
  });

  await closed;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed on click on the Calendar button"
  );

  await testCalendarBtnAttribute("aria-expanded", "false");

  await helper.tearDown();
});

/**
 * Test that time input field does not include a Calendar button,
 * but opens a time picker panel on click within the field (with a pref)
 */
add_task(async function test_calendar_button_markup_time() {
  info("Test that type=time input field does not include a Calendar button");

  // Toggle a pref to allow a time picker to be shown
  await SpecialPowers.pushPrefEnv({
    set: [["dom.forms.datetime.timepicker", true]],
  });

  let testTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html, <input type='time'>"
  );

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const input = content.document.querySelector("input");
    const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
    const calendarBtn = shadowRoot.getElementById("calendar-button");

    Assert.ok(
      ContentTaskUtils.isHidden(calendarBtn),
      "The Calendar control within a type=time input field is programmatically hidden"
    );
  });

  let ready = helper.waitForPickerReady();

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "input",
    {},
    gBrowser.selectedBrowser
  );

  await ready;

  Assert.equal(
    helper.panel.state,
    "open",
    "Time picker panel should be opened on click from anywhere within the time input field"
  );

  let closed = helper.promisePickerClosed();

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "input",
    {},
    gBrowser.selectedBrowser
  );

  await closed;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Time picker panel should be closed on click from anywhere within the time input field"
  );

  BrowserTestUtils.removeTab(testTab);
  await SpecialPowers.popPrefEnv();
});
