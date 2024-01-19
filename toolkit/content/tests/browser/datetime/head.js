/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Helper class for testing datetime input picker widget
 */
class DateTimeTestHelper {
  constructor() {
    this.panel = null;
    this.tab = null;
    this.frame = null;
  }

  /**
   * Opens a new tab with the URL of the test page, and make sure the picker is
   * ready for testing.
   *
   * @param  {String} pageUrl
   * @param  {bool} inFrame true if input is in the first child frame
   * @param  {String} openMethod "click" or "showPicker"
   */
  async openPicker(pageUrl, inFrame, openMethod = "click") {
    this.tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);
    let bc = gBrowser.selectedBrowser;
    if (inFrame) {
      await SpecialPowers.spawn(bc, [], async function () {
        const iframe = content.document.querySelector("iframe");
        // Ensure the iframe's position is correct before doing any
        // other operations
        iframe.getBoundingClientRect();
      });
      bc = bc.browsingContext.children[0];
    }
    await SpecialPowers.spawn(bc, [], async function () {
      // Ensure that screen coordinates are ok.
      await SpecialPowers.contentTransformsReceived(content);
    });

    let shown = this.waitForPickerReady();

    if (openMethod === "click") {
      await SpecialPowers.spawn(bc, [], () => {
        const input = content.document.querySelector("input");
        const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
        shadowRoot.getElementById("calendar-button").click();
      });
    } else if (openMethod === "showPicker") {
      await SpecialPowers.spawn(bc, [], function () {
        content.document.notifyUserGestureActivation();
        content.document.querySelector("input").showPicker();
      });
    }
    this.panel = await shown;
    this.frame = this.panel.querySelector("#dateTimePopupFrame");
  }

  promisePickerClosed() {
    return new Promise(resolve => {
      this.panel.addEventListener("popuphidden", resolve, { once: true });
    });
  }

  promiseChange(selector = "input") {
    return SpecialPowers.spawn(
      this.tab.linkedBrowser,
      [selector],
      async selector => {
        let input = content.document.querySelector(selector);
        await ContentTaskUtils.waitForEvent(input, "change", false, e => {
          ok(
            content.window.windowUtils.isHandlingUserInput,
            "isHandlingUserInput should be true"
          );
          return true;
        });
      }
    );
  }

  waitForPickerReady() {
    return BrowserTestUtils.waitForDateTimePickerPanelShown(window);
  }

  /**
   * Find an element on the picker.
   *
   * @param  {String} selector
   * @return {DOMElement}
   */
  getElement(selector) {
    return this.frame.contentDocument.querySelector(selector);
  }

  /**
   * Find the children of an element on the picker.
   *
   * @param  {String} selector
   * @return {Array<DOMElement>}
   */
  getChildren(selector) {
    return Array.from(this.getElement(selector).children);
  }

  /**
   * Click on an element
   *
   * @param  {DOMElement} element
   */
  click(element) {
    EventUtils.synthesizeMouseAtCenter(element, {}, this.frame.contentWindow);
  }

  /**
   * Close the panel and the tab
   */
  async tearDown() {
    if (this.panel.state != "closed") {
      let pickerClosePromise = this.promisePickerClosed();
      this.panel.hidePopup();
      await pickerClosePromise;
    }
    BrowserTestUtils.removeTab(this.tab);
    this.tab = null;
  }

  /**
   * Clean up after tests. Remove the frame to prevent leak.
   */
  cleanup() {
    this.frame?.remove();
    this.frame = null;
    this.panel = null;
  }
}

let helper = new DateTimeTestHelper();

registerCleanupFunction(() => {
  helper.cleanup();
});

const BTN_MONTH_YEAR = "#month-year-label",
  BTN_NEXT_MONTH = ".next",
  BTN_PREV_MONTH = ".prev",
  BTN_CLEAR = "#clear-button",
  DAY_SELECTED = ".selection",
  DAY_TODAY = ".today",
  DAYS_VIEW = ".days-view",
  DIALOG_PICKER = "#date-picker",
  MONTH_YEAR = ".month-year",
  MONTH_YEAR_NAV = ".month-year-nav",
  MONTH_YEAR_VIEW = ".month-year-view",
  SPINNER_MONTH = "#spinner-month",
  SPINNER_YEAR = "#spinner-year",
  WEEK_HEADER = ".week-header";
const DATE_FORMAT = new Intl.DateTimeFormat("en-US", {
  year: "numeric",
  month: "long",
  timeZone: "UTC",
}).format;
const DATE_FORMAT_LOCAL = new Intl.DateTimeFormat("en-US", {
  year: "numeric",
  month: "long",
}).format;

/**
 * Helper function to find and return a gridcell element
 * for a specific day of the month
 *
 * @return {Array[String]} TextContent of each gridcell within a calendar grid
 */
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

/**
 * Helper function to find and return a gridcell element
 * for a specific day of the month
 *
 * @param {Number} day: A day of the month to find in the month grid
 *
 * @return {HTMLElement} A gridcell that represents the needed day of the month
 */
function getDayEl(dayNum) {
  const dayEls = Array.from(
    helper.getElement(DAYS_VIEW).querySelectorAll("td")
  );
  return dayEls.find(el => el.textContent === dayNum.toString());
}

function mergeArrays(a, b) {
  return a.map((classlist, index) => classlist.concat(b[index]));
}

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
 * Helper function to check the value of a Calendar button's specific attribute
 *
 * @param {String} attr: The name of the attribute to be tested
 * @param {String} val: Value that is expected to be assigned to the attribute.
 * @param {Boolean} presenceOnly: If "true", test only the presence of the attribute
 */
async function testCalendarBtnAttribute(attr, val, presenceOnly = false) {
  let browser = helper.tab.linkedBrowser;

  await SpecialPowers.spawn(
    browser,
    [attr, val, presenceOnly],
    (attr, val, presenceOnly) => {
      const input = content.document.querySelector("input");
      const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;
      const calendarBtn = shadowRoot.getElementById("calendar-button");

      if (presenceOnly) {
        Assert.ok(
          calendarBtn.hasAttribute(attr),
          `Calendar button has ${attr} attribute`
        );
      } else {
        Assert.equal(
          calendarBtn.getAttribute(attr),
          val,
          `Calendar button has ${attr} attribute set to ${val}`
        );
      }
    }
  );
}

/**
 * Helper function to test if a submission/dismissal keyboard shortcut works
 * on a month or a year selection spinner
 *
 * @param {String} key: A keyboard Event.key that will be synthesized
 * @param {Object} document: Reference to the content document
 *                 of the #dateTimePopupFrame
 * @param {Number} tabs: How many times "Tab" key should be pressed
 *                 to move a keyboard focus to a needed spinner
 *                 (1 for month/default and 2 for year)
 *
 * @description Starts with the month-year toggle button being focused
 *              on the date/datetime-local input's datepicker panel
 */
async function testKeyOnSpinners(key, document, tabs = 1) {
  info(`Testing "${key}" key behavior`);

  Assert.equal(
    document.activeElement,
    helper.getElement(BTN_MONTH_YEAR),
    "The month-year toggle button is focused"
  );

  // Open the month-year selection panel with spinners:
  await EventUtils.synthesizeKey(" ", {});

  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).getAttribute("aria-expanded"),
    "true",
    "Month-year button is expanded when the spinners are shown"
  );
  Assert.ok(
    BrowserTestUtils.isVisible(helper.getElement(MONTH_YEAR_VIEW)),
    "Month-year selection panel is visible"
  );

  // Move focus from the month-year toggle button to one of spinners:
  await EventUtils.synthesizeKey("KEY_Tab", { repeat: tabs });

  Assert.equal(
    document.activeElement.getAttribute("role"),
    "spinbutton",
    "The spinner is focused"
  );

  // Confirm the spinbutton choice and close the month-year selection panel:
  await EventUtils.synthesizeKey(key, {});

  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).getAttribute("aria-expanded"),
    "false",
    "Month-year button is collapsed when the spinners are hidden"
  );
  Assert.ok(
    BrowserTestUtils.isHidden(helper.getElement(MONTH_YEAR_VIEW)),
    "Month-year selection panel is not visible"
  );
  Assert.equal(
    document.activeElement,
    helper.getElement(DAYS_VIEW).querySelector('[tabindex="0"]'),
    "A focusable day within a calendar grid is focused"
  );

  // Return the focus to the month-year toggle button for future tests
  // (passing a Previous button along the way):
  await EventUtils.synthesizeKey("KEY_Tab", { repeat: 3 });
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

async function verifyPickerPosition(browsingContext, inputId) {
  let inputRect = await SpecialPowers.spawn(
    browsingContext,
    [inputId],
    async function (inputIdChild) {
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
