/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const MONTH_YEAR = ".month-year",
  BTN_MONTH_YEAR = "#month-year-label",
  SPINNER_MONTH = "#spinner-month",
  SPINNER_YEAR = "#spinner-year";
const DATE_FORMAT = new Intl.DateTimeFormat("en-US", {
  year: "numeric",
  month: "long",
  timeZone: "UTC",
}).format;

/**
 * Helper function to check if a DOM element has a specific attribute
 *
 * @param {DOMElement} el: DOM Element to be tested
 * @param {String} attr: The name of the attribute to be tested
 */
function testAttribute(el, attr) {
  Assert.ok(
    el.hasAttribute(attr),
    `The "${el}" element has a "${attr}" attribute`
  );
}

/**
 * Helper function to check for localization attributes of a DOM element
 *
 * @param {DOMElement} el: DOM Element to be tested
 * @param {String} id: Value of the "data-l10n-id" attribute of the element
 * @param {Object} args: Args provided by the l10n object of the element
 */
function testLocalization(el, id, args = null) {
  const l10nAttrs = document.l10n.getAttributes(el);

  Assert.deepEqual(
    l10nAttrs,
    {
      id,
      args,
    },
    `The "${id}" element is localizable`
  );
}

/**
 * Helper function to check for l10n of an element's attribute
 *
 * @param {DOMElement} el: DOM Element to be tested
 * @param {String} attr: The name of the attribute to be tested
 * @param {String} id: Value of the "data-l10n-id" attribute of the element
 * @param {Object} args: Args provided by the l10n object of the element
 */
function testAttributeL10n(el, attr, id, args = null) {
  testAttribute(el, attr);
  testLocalization(el, id, args);
}

/**
 * Helper function to check if a CSS property respects reduced motion mode
 *
 * @param {DOMElement} el: DOM Element to be tested
 * @param {String} prop: The name of the CSS property to be tested
 * @param {Object} valueNotReduced: Default value of the tested CSS property
 *                 for "prefers-reduced-motion: no-preference"
 * @param {String} valueReduced: Value of the tested CSS property
 *                 for "prefers-reduced-motion: reduce"
 */
async function testReducedMotionProp(el, prop, valueNotReduced, valueReduced) {
  info(`Test the panel's CSS ${prop} value depending on a reduced motion mode`);

  // Set "prefers-reduced-motion" media to "no-preference"
  await SpecialPowers.pushPrefEnv({
    set: [["ui.prefersReducedMotion", 0]],
  });

  ok(
    matchMedia("(prefers-reduced-motion: no-preference)").matches,
    "The reduce motion mode is not active"
  );
  is(
    getComputedStyle(el).getPropertyValue(prop),
    valueNotReduced,
    `Default ${prop} will be provided, when a reduce motion mode is not active`
  );

  // Set "prefers-reduced-motion" media to "reduce"
  await SpecialPowers.pushPrefEnv({
    set: [["ui.prefersReducedMotion", 1]],
  });

  ok(
    matchMedia("(prefers-reduced-motion: reduce)").matches,
    "The reduce motion mode is active"
  );
  is(
    getComputedStyle(el).getPropertyValue(prop),
    valueReduced,
    `Reduced ${prop} will be provided, when a reduce motion mode is active`
  );
}

let helper = new DateTimeTestHelper();

registerCleanupFunction(() => {
  helper.cleanup();
});

/**
 * Test that the Month spinner opens with an accessible markup
 */
add_task(async function test_spinner_month_markup() {
  info("Test that the Month spinner opens with an accessible markup");

  const inputValue = "2022-09-09";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}">`
  );
  helper.click(helper.getElement(MONTH_YEAR));

  const spinnerMonth = helper.getElement(SPINNER_MONTH);
  const spinnerMonthPrev = spinnerMonth.children[0];
  const spinnerMonthBtn = spinnerMonth.children[1];
  const spinnerMonthNext = spinnerMonth.children[2];

  Assert.equal(
    spinnerMonthPrev.tagName,
    "button",
    "Spinner's Previous Month control is a button"
  );
  Assert.equal(
    spinnerMonthBtn.getAttribute("role"),
    "spinbutton",
    "Spinner control is a spinbutton"
  );
  Assert.equal(
    spinnerMonthBtn.getAttribute("tabindex"),
    "0",
    "Spinner control is included in the focus order"
  );
  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuemin"),
    "0",
    "Spinner control has a min value set"
  );
  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuemax"),
    "11",
    "Spinner control has a max value set"
  );
  // September 2022 as an example
  Assert.equal(
    spinnerMonthBtn.getAttribute("aria-valuenow"),
    "8",
    "Spinner control has a current value set"
  );
  Assert.equal(
    spinnerMonthNext.tagName,
    "button",
    "Spinner's Next Month control is a button"
  );

  testAttribute(spinnerMonthBtn, "aria-valuetext");

  let visibleEls = spinnerMonthBtn.querySelectorAll(
    ":scope > :not([aria-hidden])"
  );
  Assert.equal(
    visibleEls.length,
    0,
    "There should be no children of the spinner without aria-hidden"
  );

  info("Test that the month spinner has localizable labels");

  testAttributeL10n(
    spinnerMonthPrev,
    "aria-label",
    "date-spinner-month-previous"
  );
  testAttributeL10n(spinnerMonthBtn, "aria-label", "date-spinner-month");
  testAttributeL10n(spinnerMonthNext, "aria-label", "date-spinner-month-next");

  await testReducedMotionProp(
    spinnerMonthBtn,
    "scroll-behavior",
    "smooth",
    "auto"
  );

  await helper.tearDown();
});

/**
 * Test that the Year spinner opens with an accessible markup
 */
add_task(async function test_spinner_year_markup() {
  info("Test that the year spinner opens with an accessible markup");

  const inputValue = "2022-06-06";
  const inputMin = "2020-06-01";
  const inputMax = "2030-12-31";

  await helper.openPicker(
    `data:text/html, <input type="date" value="${inputValue}" min="${inputMin}" max="${inputMax}">`
  );
  helper.click(helper.getElement(MONTH_YEAR));

  const spinnerYear = helper.getElement(SPINNER_YEAR);
  const spinnerYearPrev = spinnerYear.children[0];
  const spinnerYearBtn = spinnerYear.children[1];
  const spinnerYearNext = spinnerYear.children[2];

  Assert.equal(
    spinnerYearPrev.tagName,
    "button",
    "Spinner's Previous Year control is a button"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("role"),
    "spinbutton",
    "Spinner control is a spinbutton"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("tabindex"),
    "0",
    "Spinner control is included in the focus order"
  );
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuemin"),
    "2020",
    "Spinner control has a min value set, when the range is provided"
  );
  // 2020-2030 range is an example
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuemax"),
    "2030",
    "Spinner control has a max value set, when the range is provided"
  );
  // June 2022 is an example
  Assert.equal(
    spinnerYearBtn.getAttribute("aria-valuenow"),
    "2022",
    "Spinner control has a current value set"
  );
  Assert.equal(
    spinnerYearNext.tagName,
    "button",
    "Spinner's Next Year control is a button"
  );

  testAttribute(spinnerYearBtn, "aria-valuetext");

  let visibleEls = spinnerYearBtn.querySelectorAll(
    ":scope > :not([aria-hidden])"
  );
  Assert.equal(
    visibleEls.length,
    0,
    "There should be no children of the spinner without aria-hidden"
  );

  info("Test that the year spinner has localizable labels");

  testAttributeL10n(
    spinnerYearPrev,
    "aria-label",
    "date-spinner-year-previous"
  );
  testAttributeL10n(spinnerYearBtn, "aria-label", "date-spinner-year");
  testAttributeL10n(spinnerYearNext, "aria-label", "date-spinner-year-next");

  await testReducedMotionProp(
    spinnerYearBtn,
    "scroll-behavior",
    "smooth",
    "auto"
  );

  await helper.tearDown();
});
