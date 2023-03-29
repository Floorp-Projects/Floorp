/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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
      DATE_FORMAT_LOCAL(date),
      "Date picker opens when a showPicker method is called"
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
 * Test that date picker opens with showPicker and the explicit value.
 */
add_task(async function test_datepicker_showPicker_value() {
  await helper.openPicker(
    "data:text/html, <input type='date' value='2012-10-15'>",
    false,
    "showPicker"
  );

  Assert.equal(
    helper.getElement(MONTH_YEAR).textContent,
    DATE_FORMAT_LOCAL(new Date("2012-10-12")),
    "Date picker opens when a showPicker method is called"
  );

  await helper.tearDown();
});
