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
  return helper.getChildren(DAYS_VIEW).map(child => child.textContent);
}

function getCalendarClassList() {
  return helper
    .getChildren(DAYS_VIEW)
    .map(child => Array.from(child.classList));
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
    ok(
      Math.abs(got - exp) < 1,
      msg + ": " + got + " should be equal(-ish) to " + exp
    );
  }
  is_close(helper.panel.screenX, inputRect.left, "datepicker x position");
  is_close(helper.panel.screenY, inputRect.bottom, "datepicker y position");
}

let helper = new DateTimeTestHelper();

registerCleanupFunction(() => {
  helper.cleanup();
});

/**
 * Test that date picker opens to today's date when input field is blank
 */
add_task(async function test_datepicker_today() {
  const date = new Date();

  await helper.openPicker("data:text/html, <input type='date'>");

  if (date.getMonth() === new Date().getMonth()) {
    Assert.equal(
      helper.getElement(MONTH_YEAR).textContent,
      DATE_FORMAT_LOCAL(date)
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
  const inputValue = "2016-12-15";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );

  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue))
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
    "2016-12"
  );
  Assert.deepEqual(
    getCalendarClassList(),
    calendarClasslist_201612,
    "2016-12 classNames"
  );

  await helper.tearDown();
});

/**
 * Ensure picker closes when focus moves to a different input.
 */
add_task(async function test_datepicker_focus_change() {
  await helper.openPicker(
    `data:text/html,<input id=date type=date><input id=other>`
  );
  let browser = helper.tab.linkedBrowser;
  await verifyPickerPosition(browser, "date");

  isnot(helper.panel.state, "closed", "Panel should be visible");

  let closed = helper.promisePickerClosed();

  await SpecialPowers.spawn(browser, [], () => {
    content.document.querySelector("#other").focus();
  });

  await closed;

  ok(true, "Panel should be closed now");

  await helper.tearDown();
});

/**
 * Ensure picker opens and closes with key bindings appropriately.
 */
add_task(async function test_datepicker_keyboard_open() {
  const inputValue = "2016-12-15";
  const prevMonth = "2016-11-01";
  await helper.openPicker(
    `data:text/html,<input id=date type=date value=${inputValue}>`
  );
  let browser = helper.tab.linkedBrowser;
  await verifyPickerPosition(browser, "date");

  let closed = helper.promisePickerClosed();

  BrowserTestUtils.synthesizeKey(" ", {}, browser);

  await closed;

  let ready = helper.waitForPickerReady();

  BrowserTestUtils.synthesizeKey(" ", {}, browser);

  await ready;

  // NOTE: After the click, the first input field (the month one) is focused,
  // so down arrow will change the selected month.
  //
  // This assumes en-US locale, which seems fine for testing purposes (as
  // DATE_FORMAT and other bits around do the same).
  BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);

  // It'd be good to use something else than waitForCondition for this but
  // there's no exposed event atm when the value changes from the child.
  await BrowserTestUtils.waitForCondition(() => {
    return (
      helper.getElement(MONTH_YEAR).textContent ==
      DATE_FORMAT(new Date(prevMonth))
    );
  }, "Should update date when updating months");

  await helper.tearDown();
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
  const inputValue = "2016-12-15";
  const firstDayOnCalendar = "2016-11-27";

  await helper.openPicker(
    `data:text/html, <input id="date" type="date" value="${inputValue}">`
  );

  let browser = helper.tab.linkedBrowser;
  await verifyPickerPosition(browser, "date");

  // Click the first item (top-left corner) of the calendar
  let promise = BrowserTestUtils.waitForContentEvent(
    helper.tab.linkedBrowser,
    "input"
  );
  helper.click(helper.getElement(DAYS_VIEW).children[0]);
  await promise;

  let value = await SpecialPowers.spawn(
    browser,
    [],
    () => content.document.querySelector("input").value
  );
  Assert.equal(value, firstDayOnCalendar);

  await helper.tearDown();
});

/**
 * Ensure that the datepicker popop appears correctly positioned when
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

  // Ensures the picker opens to the month of the input value
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "input",
    {},
    gBrowser.selectedBrowser
  );
  await helper.waitForPickerReady();
  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT(new Date(inputValue))
  );

  await helper.tearDown();
});

/**
 * When min and max attributes are set, calendar should show some dates as
 * out-of-range.
 */
add_task(async function test_datepicker_min_max() {
  const inputValue = "2016-12-15";
  const inputMin = "2016-12-05";
  const inputMax = "2016-12-25";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}" min="${inputMin}" max="${inputMax}">`
  );

  Assert.deepEqual(
    getCalendarClassList(),
    mergeArrays(calendarClasslist_201612, [
      // R denotes out-of-range
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
    ]),
    "2016-12 with min & max"
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

add_task(async function test_datepicker_abs_min() {
  const inputValue = "0001-01-01";
  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );

  Assert.deepEqual(
    getCalendarText(),
    [
      "",
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
      "8",
      "9",
      "10",
    ],
    "0001-01"
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

add_task(async function test_datepicker_abs_max() {
  const inputValue = "275760-09-13";
  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );

  Assert.deepEqual(
    getCalendarText(),
    [
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
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
    ],
    "275760-09"
  );

  await helper.tearDown();
});

/**
 * Ensure datetime-local picker closes when focus moves to a time input.
 */
add_task(async function test_datetime_focus_to_input() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.forms.datetime-local", true],
      ["dom.forms.datetime-local.widget", true],
    ],
  });

  await helper.openPicker(
    `data:text/html,<input id=datetime type=datetime-local>`
  );
  let browser = helper.tab.linkedBrowser;
  await verifyPickerPosition(browser, "datetime");

  isnot(helper.panel.state, "closed", "Panel should be visible");

  let closed = helper.promisePickerClosed();

  // Move to the time section by pressing tab.
  for (let i = 0; i < 3; ++i) {
    await BrowserTestUtils.synthesizeKey("KEY_Tab", {}, browser);
  }

  await closed;

  ok(true, "Panel should be closed now");

  // The input should still be focused.
  let isFocused = await SpecialPowers.spawn(browser, [], () => {
    return content.document.querySelector("#datetime").matches(":focus");
  });

  ok(isFocused, "<input> should still be focused");

  await helper.tearDown();
});

// Bug 1726546
add_task(async function test_datetime_local_min() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.forms.datetime-local", true],
      ["dom.forms.datetime-local.widget", true],
    ],
  });

  const inputValue = "2016-12-15T04:00";
  const inputMin = "2016-12-05T12:22";
  const inputMax = "2016-12-25T12:22";

  await helper.openPicker(
    `data:text/html,<input type="datetime-local" value="${inputValue}" min="${inputMin}" max="${inputMax}">`
  );

  Assert.deepEqual(
    getCalendarClassList(),
    mergeArrays(calendarClasslist_201612, [
      // R denotes out-of-range
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
      [R],
    ]),
    "2016-12 with min & max"
  );

  await helper.tearDown();
});

// Bug 1726546
add_task(async function test_datetime_local_min_select_invalid() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.forms.datetime-local", true],
      ["dom.forms.datetime-local.widget", true],
    ],
  });

  const inputValue = "2016-12-15T05:00";
  const inputMin = "2016-12-05T12:22";
  const inputMax = "2016-12-25T12:22";

  await helper.openPicker(
    `data:text/html,<input type="datetime-local" value="${inputValue}" min="${inputMin}" max="${inputMax}">`
  );

  let changePromise = helper.promiseChange();

  // Select the minimum day (the 5th, which is the 9th child).
  // The date becomes invalid (we select 2016-12-05T05:00).
  helper.click(helper.getElement(DAYS_VIEW).children[8]);

  await changePromise;

  let [value, invalid] = await SpecialPowers.spawn(
    helper.tab.linkedBrowser,
    [],
    async () => {
      let input = content.document.querySelector("input");
      return [input.value, input.matches(":invalid")];
    }
  );

  is(value, "2016-12-05T05:00", "Value should've changed");
  ok(invalid, "input should be now invalid");

  await helper.tearDown();
});
