/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Create a list of abbreviations for calendar class names
const W = "weekend",
  O = "outside",
  S = "selection",
  R = "out-of-range",
  T = "today",
  P = "off-step";

// Calendar classlist for 2016-12. Used to verify the classNames are correct.
const calendarClasslist_201612 = [
  [W, O],
  [O],
  [O],
  [O],
  [],
  [],
  [W],
  [W],
  [],
  [],
  [],
  [],
  [],
  [W],
  [W],
  [],
  [],
  [],
  [S],
  [],
  [W],
  [W],
  [],
  [],
  [],
  [],
  [],
  [W],
  [W],
  [],
  [],
  [],
  [],
  [],
  [W],
  [W, O],
  [O],
  [O],
  [O],
  [O],
  [O],
  [W, O],
];

/**
 * Test that date picker opens to today's date when input field is blank
 */
add_task(async function test_datepicker_today() {
  info("Test that date picker opens to today's date when input field is blank");

  const date = new Date();

  await helper.openPicker("data:text/html, <input type='date'>");

  if (date.getMonth() === new Date().getMonth()) {
    Assert.equal(
      helper.getElement(MONTH_YEAR).textContent,
      DATE_FORMAT_LOCAL(date),
      "Today's date is opened"
    );
    Assert.equal(
      helper.getElement(DAY_TODAY).getAttribute("aria-current"),
      "date",
      "Today's date is programmatically current"
    );
    Assert.equal(
      helper.getElement(DAY_TODAY).getAttribute("tabindex"),
      "0",
      "Today's date is included in the focus order, when nothing is selected"
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
 * Test that date picker opens to the correct month, with calendar days
 * displayed correctly, given a date value is set.
 */
add_task(async function test_datepicker_open() {
  info("Test the date picker markup with a set input date value");

  const inputValue = "2016-12-15";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );

  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue)),
    "2016-12-15 date is opened"
  );

  Assert.deepEqual(
    getCalendarText(),
    [
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
      "5",
      "6",
      "7",
    ],
    "Calendar text for 2016-12 is correct"
  );
  Assert.deepEqual(
    getCalendarClassList(),
    calendarClasslist_201612,
    "2016-12 classNames of the picker are correct"
  );
  Assert.equal(
    helper.getElement(DAY_SELECTED).getAttribute("aria-selected"),
    "true",
    "Chosen date is programmatically selected"
  );
  Assert.equal(
    helper.getElement(DAY_SELECTED).getAttribute("tabindex"),
    "0",
    "Selected date is included in the focus order"
  );

  await helper.tearDown();
});

/**
 * Ensure that the datepicker popup appears correctly positioned when
 * the input field has been transformed.
 */
add_task(async function test_datepicker_transformed_position() {
  const inputValue = "2016-12-15";

  const style =
    "transform: translateX(7px) translateY(13px); border-top: 2px; border-left: 5px; margin: 30px;";
  const iframeContent = `<input id="date" type="date" value="${inputValue}" style="${style}">`;
  await helper.openPicker(
    "data:text/html,<iframe id='iframe' src='http://example.net/document-builder.sjs?html=" +
      encodeURI(iframeContent) +
      "'>",
    true
  );

  let bc = helper.tab.linkedBrowser.browsingContext.children[0];
  await verifyPickerPosition(bc, "date");

  await helper.tearDown();
});

/**
 * Make sure picker is in correct state when it is reopened.
 */
add_task(async function test_datepicker_reopen_state() {
  const inputValue = "2016-12-15";
  const nextMonth = "2017-01-01";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );

  // Navigate to the next month but do not commit the change
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue))
  );

  helper.click(helper.getElement(BTN_NEXT_MONTH));

  // January 2017
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextMonth))
  );

  let closed = helper.promisePickerClosed();

  EventUtils.synthesizeKey("KEY_Escape", {});

  await closed;

  Assert.equal(helper.panel.state, "closed", "Panel should be closed");

  // December 2016
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let input = content.document.querySelector("input");
    Assert.equal(
      input.value,
      "2016-12-15",
      "The input value remains unchanged after the picker is dismissed"
    );
  });

  let ready = helper.waitForPickerReady();

  // Move focus from the browser to an input field and open a picker:
  EventUtils.synthesizeKey("KEY_Tab", {});
  EventUtils.synthesizeKey(" ", {});

  await ready;

  Assert.equal(helper.panel.state, "open", "Panel should be opened");

  // December 2016
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue))
  );

  await helper.tearDown();
});

/**
 * When step attribute is set, calendar should show some dates as off-step.
 */
add_task(async function test_datepicker_step() {
  const inputValue = "2016-12-15";
  const inputStep = "5";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}" step="${inputStep}">`
  );

  Assert.deepEqual(
    getCalendarClassList(),
    mergeArrays(calendarClasslist_201612, [
      // P denotes off-step
      [P],
      [P],
      [P],
      [],
      [P],
      [P],
      [P],
      [P],
      [],
      [P],
      [P],
      [P],
      [P],
      [],
      [P],
      [P],
      [P],
      [P],
      [],
      [P],
      [P],
      [P],
      [P],
      [],
      [P],
      [P],
      [P],
      [P],
      [],
      [P],
      [P],
      [P],
      [P],
      [],
      [P],
      [P],
      [P],
      [P],
      [],
      [P],
      [P],
      [P],
    ]),
    "2016-12 with step"
  );

  await helper.tearDown();
});

// This test checks if the change event is considered as user input event.
add_task(async function test_datepicker_handling_user_input() {
  await helper.openPicker(`data:text/html, <input type="date">`);

  let changeEventPromise = helper.promiseChange();

  // Click the first item (top-left corner) of the calendar
  helper.click(helper.getElement(DAYS_VIEW).children[0]);
  await changeEventPromise;

  await helper.tearDown();
});

/**
 * Ensure datetime-local picker closes when selection is made.
 */
add_task(async function test_datetime_focus_to_input() {
  info("Ensure datetime-local picker closes when focus moves to a time input");

  await helper.openPicker(
    `data:text/html,<input id=datetime type=datetime-local>`
  );
  let browser = helper.tab.linkedBrowser;
  await verifyPickerPosition(browser, "datetime");

  Assert.equal(helper.panel.state, "open", "Panel should be visible");

  // Make selection to close the date dialog
  await EventUtils.synthesizeKey(" ", {});

  let closed = helper.promisePickerClosed();

  await closed;

  Assert.equal(helper.panel.state, "closed", "Panel should be closed now");

  await helper.tearDown();
});
