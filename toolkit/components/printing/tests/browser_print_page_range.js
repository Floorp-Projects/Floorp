/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function changeAllToCustom(helper) {
  let rangeSelect = helper.get("range-picker");
  rangeSelect.focus();
  rangeSelect.scrollIntoView({ block: "center" });
  EventUtils.sendKey("space", helper.win);
  EventUtils.sendKey("down", helper.win);
  EventUtils.sendKey("return", helper.win);
}

function changeCustomToAll(helper) {
  let rangeSelect = helper.get("range-picker");
  rangeSelect.focus();
  rangeSelect.scrollIntoView({ block: "center" });
  EventUtils.sendKey("space", helper.win);
  EventUtils.sendKey("up", helper.win);
  EventUtils.sendKey("return", helper.win);
}

add_task(async function testRangeResetAfterScale() {
  const mockPrinterName = "Fake Printer";
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter(mockPrinterName);
    await helper.startPrint();
    await helper.setupMockPrint();

    helper.mockFilePicker("changeRangeFromScale.pdf");
    changeAllToCustom(helper);

    await helper.openMoreSettings();
    let scaleRadio = helper.get("percent-scale-choice");
    await helper.waitForPreview(() => helper.click(scaleRadio));
    let percentScale = helper.get("percent-scale");
    await helper.waitForPreview(() => helper.text(percentScale, "200"));

    let customRange = helper.get("custom-range");
    let rangeError = helper.get("error-invalid-range");
    await helper.waitForPreview(() => {
      helper.text(customRange, "3");
    });

    ok(rangeError.hidden, "Range error is hidden");

    await helper.text(percentScale, "10");
    EventUtils.sendKey("return", helper.win);

    await BrowserTestUtils.waitForAttributeRemoval("hidden", rangeError);
    ok(!rangeError.hidden, "Range error is showing");
    await helper.closeDialog();
  });
});

add_task(async function testRangeResetAfterPaperSize() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.waitForPreview(() =>
      helper.dispatchSettingsChange({ paperId: "iso_a5" })
    );
    await helper.setupMockPrint();

    await helper.openMoreSettings();
    let scaleRadio = helper.get("percent-scale-choice");
    await helper.waitForPreview(() => helper.click(scaleRadio));
    let percentScale = helper.get("percent-scale");
    await helper.waitForPreview(() => helper.text(percentScale, "200"));

    let customRange = helper.get("custom-range");
    changeAllToCustom(helper);
    await BrowserTestUtils.waitForAttributeRemoval("hidden", customRange);

    let rangeError = helper.get("error-invalid-range");
    await helper.waitForPreview(() => {
      helper.text(customRange, "6");
    });

    ok(rangeError.hidden, "Range error is hidden");

    helper.dispatchSettingsChange({ paperId: "iso_a3" });
    await BrowserTestUtils.waitForCondition(
      () => helper.get("paper-size-picker").value == "iso_a3",
      "Wait for paper size select to update"
    );
    EventUtils.sendKey("return", helper.win);

    await BrowserTestUtils.waitForAttributeRemoval("hidden", rangeError);
    ok(!rangeError.hidden, "Range error is showing");
    await helper.closeDialog();
  });
});

add_task(async function testInvalidRangeResetAfterDestinationChange() {
  const mockPrinterName = "Fake Printer";
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter(mockPrinterName);
    await helper.startPrint();

    let destinationPicker = helper.get("printer-picker");
    let customPageRange = helper.get("custom-range");

    await helper.assertSettingsNotChanged({ pageRanges: [] }, async () => {
      changeAllToCustom(helper);
    });
    let rangeError = helper.get("error-invalid-range");

    await helper.assertSettingsNotChanged({ pageRanges: [] }, async () => {
      ok(rangeError.hidden, "Range error is hidden");
      await helper.text(customPageRange, "9");
      await BrowserTestUtils.waitForAttributeRemoval("hidden", rangeError);
      ok(!rangeError.hidden, "Range error is showing");
    });

    is(destinationPicker.disabled, false, "Destination picker is enabled");

    // Select a new printer
    helper.dispatchSettingsChange({ printerName: mockPrinterName });
    await BrowserTestUtils.waitForCondition(
      () => rangeError.hidden,
      "Wait for range error to be hidden"
    );
    is(customPageRange.value, "", "Page range has reset");
    await helper.closeDialog();
  });
});

add_task(async function testPageRangeSets() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    let customRange = helper.get("custom-range");
    let pageRangeInput = helper.get("page-range-input");
    let invalidError = helper.get("error-invalid-range");
    let invalidOverflowError = helper.get("error-invalid-start-range-overflow");

    ok(customRange.hidden, "Custom range input is hidden");

    changeAllToCustom(helper);
    await BrowserTestUtils.waitForAttributeRemoval("hidden", customRange);

    ok(!customRange.hidden, "Custom range is showing");

    // We need to set the input to something to ensure we do not return early
    // out of our validation function
    helper.text(helper.get("custom-range"), ",");

    let validStrings = {
      "1": [1, 1],
      "1,": [1, 1],
      "2": [2, 2],
      "1-2": [1, 2],
      "1,2": [1, 2],
      "1,2,": [1, 2],
      "2,1": [1, 2],
      "1,3": [1, 1, 3, 3],
      "1-1,3": [1, 1, 3, 3],
      "1,3-3": [1, 1, 3, 3],
      "10-33": [10, 33],
      "1-": [1, 50],
      "-": [],
      "-20": [1, 20],
      "-,1": [],
      "-1,1-": [],
      "-1,1-2": [1, 2],
      ",9": [9, 9],
      ",": [],
      "1,2,1,20,5": [1, 2, 5, 5, 20, 20],
      "1-17,4,12-19": [1, 19],
      "43-46,42,47-": [42, 50],
    };

    for (let [str, expected] of Object.entries(validStrings)) {
      pageRangeInput._validateRangeInput(str, 50);
      let pageRanges = pageRangeInput.formatPageRange();

      is(
        expected.every((page, index) => page === pageRanges[index]),
        true,
        `Expected page range for "${str}" matches "${expected}"`
      );

      ok(invalidError.hidden, "Generic error message is hidden");
      ok(invalidOverflowError.hidden, "Start overflow error message is hidden");
    }

    let invalidStrings = ["51", "1,51", "1-51", "4-1", "--", "0", "-90"];

    for (let str of invalidStrings) {
      pageRangeInput._validateRangeInput(str, 50);
      is(pageRangeInput._pagesSet.size, 0, `There are no pages in the set`);
      ok(!pageRangeInput.validity, "Input is invalid");
    }

    await helper.closeDialog();
  });
});

add_task(async function testRangeError() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    changeAllToCustom(helper);

    let invalidError = helper.get("error-invalid-range");
    let invalidOverflowError = helper.get("error-invalid-start-range-overflow");

    ok(invalidError.hidden, "Generic error message is hidden");
    ok(invalidOverflowError.hidden, "Start overflow error message is hidden");

    helper.text(helper.get("custom-range"), "4");

    await BrowserTestUtils.waitForAttributeRemoval("hidden", invalidError);

    ok(!invalidError.hidden, "Generic error message is showing");
    ok(invalidOverflowError.hidden, "Start overflow error message is hidden");

    await helper.closeDialog();
  });
});

add_task(async function testStartOverflowRangeError() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    changeAllToCustom(helper);

    await helper.openMoreSettings();
    let scaleRadio = helper.get("percent-scale-choice");
    await helper.waitForPreview(() => helper.click(scaleRadio));
    let percentScale = helper.get("percent-scale");
    await helper.waitForPreview(() => helper.text(percentScale, "200"));

    let invalidError = helper.get("error-invalid-range");
    let invalidOverflowError = helper.get("error-invalid-start-range-overflow");

    ok(invalidError.hidden, "Generic error message is hidden");
    ok(invalidOverflowError.hidden, "Start overflow error message is hidden");

    helper.text(helper.get("custom-range"), "2-1");

    await BrowserTestUtils.waitForAttributeRemoval(
      "hidden",
      invalidOverflowError
    );

    ok(invalidError.hidden, "Generic error message is hidden");
    ok(!invalidOverflowError.hidden, "Start overflow error message is showing");

    await helper.closeDialog();
  });
});

add_task(async function testErrorClearedAfterSwitchingToAll() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    changeAllToCustom(helper);

    let customRange = helper.get("custom-range");
    let rangeError = helper.get("error-invalid-range");
    ok(rangeError.hidden, "Generic error message is hidden");

    helper.text(customRange, "3");

    await BrowserTestUtils.waitForAttributeRemoval("hidden", rangeError);
    ok(!rangeError.hidden, "Generic error message is showing");

    changeCustomToAll(helper);

    await BrowserTestUtils.waitForCondition(
      () => rangeError.hidden,
      "Wait for range error to be hidden"
    );
    ok(customRange.hidden, "Custom range is hidden");
    await helper.closeDialog();
  });
});
