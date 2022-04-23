/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const MONTH_YEAR = ".month-year",
  DAYS_VIEW = ".days-view",
  BTN_PREV_MONTH = ".prev",
  BTN_NEXT_MONTH = ".next";
const DATE_FORMAT_LOCAL = new Intl.DateTimeFormat("en-US", {
  year: "numeric",
  month: "long",
}).format;

let helper = new DateTimeTestHelper();

registerCleanupFunction(() => {
  helper.cleanup();
});

/**
 * Test that date picker opens with showPicker.
 */
add_task(async function test_datepicker_showPicker() {
  const date = new Date();

  await helper.openPicker(
    "data:text/html, <input type='date'>",
    false,
    "showPicker"
  );

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
