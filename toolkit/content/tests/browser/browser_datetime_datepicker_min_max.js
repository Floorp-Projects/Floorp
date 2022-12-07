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

  Assert.ok(
    helper
      .getElement(DAYS_VIEW)
      .firstChild.firstChild.getAttribute("aria-disabled"),
    "An out-of-range date is programmatically disabled"
  );

  Assert.ok(
    !helper.getElement(DAY_SELECTED).hasAttribute("aria-disabled"),
    "An in-range date is not programmatically disabled"
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

// Bug 1726546
add_task(async function test_datetime_local_min() {
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
  const inputValue = "2016-12-15T05:00";
  const inputMin = "2016-12-05T12:22";
  const inputMax = "2016-12-25T12:22";

  await helper.openPicker(
    `data:text/html,<input type="datetime-local" value="${inputValue}" min="${inputMin}" max="${inputMax}">`
  );

  let changePromise = helper.promiseChange();

  // Select the minimum day (the 5th, which is the 2nd child of 2nd row).
  // The date becomes invalid (we select 2016-12-05T05:00).
  helper.click(helper.getElement(DAYS_VIEW).children[1].children[1]);

  await changePromise;

  let [value, invalid] = await SpecialPowers.spawn(
    helper.tab.linkedBrowser,
    [],
    async () => {
      let input = content.document.querySelector("input");
      return [input.value, input.matches(":invalid")];
    }
  );

  Assert.equal(value, "2016-12-05T05:00", "Value should've changed");
  Assert.ok(invalid, "input should be now invalid");

  await helper.tearDown();
});

/**
 * Test that date picker opens to the minium valid date when the value property is lower than the min property
 */
add_task(async function test_datepicker_value_lower_than_min() {
  const date = new Date();
  const inputValue = "2001-02-03";
  const minValue = "2004-05-06";
  const maxValue = "2007-08-09";

  await helper.openPicker(
    `data:text/html, <input type='date' value="${inputValue}" min="${minValue}" max="${maxValue}">`
  );

  if (date.getMonth() === new Date().getMonth()) {
    Assert.equal(
      helper.getElement(MONTH_YEAR).textContent,
      DATE_FORMAT(new Date(minValue))
    );
  } else {
    Assert.ok(
      true,
      "Skipping datepicker value lower than min test if month changes when opening picker."
    );
  }

  await helper.tearDown();
});

/**
 * Test that date picker opens to the maximum valid date when the value property is higher than the max property
 */
add_task(async function test_datepicker_value_higher_than_max() {
  const date = new Date();
  const minValue = "2001-02-03";
  const maxValue = "2004-05-06";
  const inputValue = "2007-08-09";

  await helper.openPicker(
    `data:text/html, <input type='date' value="${inputValue}" min="${minValue}" max="${maxValue}">`
  );

  if (date.getMonth() === new Date().getMonth()) {
    Assert.equal(
      helper.getElement(MONTH_YEAR).textContent,
      DATE_FORMAT(new Date(maxValue))
    );
  } else {
    Assert.ok(
      true,
      "Skipping datepicker value higher than max test if month changes when opening picker."
    );
  }

  await helper.tearDown();
});
