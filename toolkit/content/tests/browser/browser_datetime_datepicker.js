/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const MONTH_YEAR = ".month-year",
  DAYS_VIEW = ".days-view",
  BTN_NEXT_MONTH = ".next",
  DAY_TODAY = ".today",
  DAY_SELECTED = ".selection";
const DATE_FORMAT = new Intl.DateTimeFormat("en-US", {
  year: "numeric",
  month: "long",
  timeZone: "UTC",
}).format;
const DATE_FORMAT_LOCAL = new Intl.DateTimeFormat("en-US", {
  year: "numeric",
  month: "long",
}).format;

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

function getCalendarText() {
  let calendarCells = [];
  for (const tr of helper.getChildren(DAYS_VIEW)) {
    for (const td of tr.children) {
      calendarCells.push(td.textContent);
    }
  }
  return calendarCells;
}

function getCalendarClassList() {
  let calendarCellsClasses = [];
  for (const tr of helper.getChildren(DAYS_VIEW)) {
    for (const td of tr.children) {
      calendarCellsClasses.push(td.classList);
    }
  }
  return calendarCellsClasses;
}

function mergeArrays(a, b) {
  return a.map((classlist, index) => classlist.concat(b[index]));
}

async function verifyPickerPosition(browsingContext, inputId) {
  let inputRect = await SpecialPowers.spawn(
    browsingContext,
    [inputId],
    async function(inputIdChild) {
      let rect = content.document
        .getElementById(inputIdChild)
        .getBoundingClientRect();
      return {
        left: content.mozInnerScreenX + rect.left,
        bottom: content.mozInnerScreenY + rect.bottom,
      };
    }
  );

  function is_close(got, exp, msg) {
    // on some platforms we see differences of a fraction of a pixel - so
    // allow any difference of < 1 pixels as being OK.
    Assert.ok(
      Math.abs(got - exp) < 1,
      msg + ": " + got + " should be equal(-ish) to " + exp
    );
  }
  const marginLeft = parseFloat(getComputedStyle(helper.panel).marginLeft);
  const marginTop = parseFloat(getComputedStyle(helper.panel).marginTop);
  is_close(
    helper.panel.screenX - marginLeft,
    inputRect.left,
    "datepicker x position"
  );
  is_close(
    helper.panel.screenY - marginTop,
    inputRect.bottom,
    "datepicker y position"
  );
}

let helper = new DateTimeTestHelper();

registerCleanupFunction(() => {
  helper.cleanup();
});

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

  // Navigate to the next month but does not commit the change
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue))
  );

  helper.click(helper.getElement(BTN_NEXT_MONTH));

  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(nextMonth))
  );

  EventUtils.synthesizeKey("VK_ESCAPE", {}, window);

  Assert.equal(helper.panel.state, "closed", "Panel should be closed");

  // Ensures the picker opens to the month of the input value
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let input = content.document.querySelector("input");
    function getCalendarButton(input) {
      const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
      return shadowRoot.getElementById("calendar-button");
    }
    getCalendarButton(input).click();
  });

  await helper.waitForPickerReady();

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
