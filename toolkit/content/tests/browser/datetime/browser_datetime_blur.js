/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_CONTENT = `data:text/html,
  <body onload='gBlurEvents = 0; gDateFocusEvents = 0; gTextFocusEvents = 0'>
    <input type='date' id='date' onfocus='gDateFocusEvents++' onblur='gBlurEvents++'>
    <input type='text' id='text' onfocus='gTextFocusEvents++'>
  </body>`;

function getBlurEvents() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.gBlurEvents;
  });
}

function getDateFocusEvents() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.gDateFocusEvents;
  });
}

function getTextFocusEvents() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.gTextFocusEvents;
  });
}

/**
 * Test that when a picker panel is opened by an input
 * the input is not blurred
 */
add_task(async function test_parent_blur() {
  info(
    "Test that when a picker panel is opened by an input the parent is not blurred"
  );

  // Set "prefers-reduced-motion" media to "reduce"
  // to avoid intermittent scroll failures (1803612, 1803687)
  await SpecialPowers.pushPrefEnv({
    set: [["ui.prefersReducedMotion", 1]],
  });
  Assert.ok(
    matchMedia("(prefers-reduced-motion: reduce)").matches,
    "The reduce motion mode is active"
  );

  await helper.openPicker(PAGE_CONTENT, false, "showPicker");

  Assert.equal(
    await getDateFocusEvents(),
    0,
    "Date input field is not calling a focus event when the '.showPicker()' method is called"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const input = content.document.querySelector("#date");

    Assert.ok(
      !input.matches(":focus"),
      `The keyboard focus is not placed on the date input after showPicker is called`
    );
  });

  let closedOnEsc = helper.promisePickerClosed();

  // Close a date picker
  EventUtils.synthesizeKey("KEY_Escape", {});

  await closedOnEsc;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed on Escape"
  );
  Assert.equal(
    await getDateFocusEvents(),
    0,
    "Date input field is not focused when its picker is dismissed with Escape key"
  );
  Assert.equal(
    await getBlurEvents(),
    0,
    "Date input field is not blurred when the picker is closed with Escape key"
  );

  // Ensure focus is on the input field
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const input = content.document.querySelector("#date");

    input.focus();

    Assert.ok(
      input.matches(":focus"),
      `The keyboard focus is placed on the date input field`
    );
  });
  Assert.equal(
    await getDateFocusEvents(),
    1,
    "A focus event was fired on the Date input field"
  );

  let readyOnKey = helper.waitForPickerReady();

  // Open a date picker
  EventUtils.synthesizeKey(" ", {});

  await readyOnKey;

  Assert.equal(
    helper.panel.state,
    "open",
    "Date picker panel should be opened"
  );
  Assert.equal(
    helper.panel
      .querySelector("#dateTimePopupFrame")
      .contentDocument.activeElement.getAttribute("role"),
    "gridcell",
    "The picker is opened and a calendar day is focused"
  );

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const input = content.document.querySelector("#date");

    Assert.ok(
      input.matches(":focus"),
      `The keyboard focus is retained on the date input field`
    );
    Assert.equal(
      input,
      content.document.activeElement,
      "Input field does not loose focus when its picker is opened and focused"
    );
  });

  Assert.equal(
    await getBlurEvents(),
    0,
    "Date input field is not blurred when its picker is opened and focused"
  );
  Assert.equal(
    await getDateFocusEvents(),
    1,
    "No new focus events were fired on the Date input while its picker is opened"
  );

  info(
    `Test that the date input field is not blurred after interacting
    with a month-year panel`
  );

  // Move focus from the today's date to the month-year toggle button:
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 3 });

  Assert.ok(
    helper.getElement(BTN_MONTH_YEAR).matches(":focus"),
    "The month-year toggle button is focused"
  );

  // Open the month-year selection panel:
  EventUtils.synthesizeKey(" ", {});

  Assert.equal(
    helper.getElement(BTN_MONTH_YEAR).getAttribute("aria-expanded"),
    "true",
    "Month-year button is expanded when the spinners are shown"
  );
  Assert.ok(
    BrowserTestUtils.isVisible(helper.getElement(MONTH_YEAR_VIEW)),
    "Month-year selection panel is visible"
  );

  // Move focus from the month-year toggle button to the year spinner:
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 2 });

  // Change the year spinner value from February 2023 to March 2023:
  EventUtils.synthesizeKey("KEY_ArrowDown", {});

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const input = content.document.querySelector("#date");

    Assert.ok(
      input.matches(":focus"),
      `The keyboard focus is retained on the date input field`
    );
    Assert.equal(
      input,
      content.document.activeElement,
      "Input field does not loose focus when the month-year picker is opened and interacted with"
    );
  });

  Assert.equal(
    await getBlurEvents(),
    0,
    "Date input field is not blurred after interacting with a month-year panel"
  );

  info(`Test that when a picker panel is opened and then it is closed
  with a click on the other field, the focus is updated`);

  let closedOnClick = helper.promisePickerClosed();

  // Close a picker by clicking on another input
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#text",
    {},
    gBrowser.selectedBrowser
  );

  await closedOnClick;

  Assert.equal(
    helper.panel.state,
    "closed",
    "Panel should be closed when another element is clicked"
  );

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async () => {
    const inputText = content.document.querySelector("#text");
    const inputDate = content.document.querySelector("#date");

    Assert.ok(
      inputText.matches(":focus"),
      `The keyboard focus is moved to the text input field`
    );
    Assert.equal(
      inputText,
      content.document.activeElement,
      "Text input field gains a focus when clicked"
    );
    Assert.ok(
      !inputDate.matches(":focus"),
      `The keyboard focus is moved from the date input field`
    );
    Assert.notEqual(
      inputDate,
      content.document.activeElement,
      "Date input field is not focused anymore"
    );
  });

  Assert.equal(
    await getBlurEvents(),
    1,
    "Date input field is blurred when focus is moved to the text input field"
  );
  Assert.equal(
    await getTextFocusEvents(),
    1,
    "Text input field is focused when it is clicked"
  );
  Assert.equal(
    await getDateFocusEvents(),
    1,
    "No new focus events were fired on the Date input after its picker was closed"
  );

  await helper.tearDown();
  // Clear the prefers-reduced-motion pref from the test profile:
  await SpecialPowers.popPrefEnv();
});
