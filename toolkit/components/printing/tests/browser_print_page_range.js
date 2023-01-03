/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function changeRangeTo(helper, destination) {
  let rangeSelect = helper.get("range-picker");
  let options = getRangeOptions(helper);
  let numberMove =
    options.indexOf(destination) - options.indexOf(rangeSelect.value);
  let direction = numberMove > 0 ? "down" : "up";
  if (!numberMove) {
    return;
  }

  let input = BrowserTestUtils.waitForEvent(rangeSelect, "input");

  let popupOpen = BrowserTestUtils.waitForSelectPopupShown(window);

  rangeSelect.focus();
  rangeSelect.scrollIntoView({ block: "center" });
  EventUtils.sendKey("space", helper.win);

  await popupOpen;
  for (let i = Math.abs(numberMove); i > 0; i--) {
    EventUtils.sendKey(direction, window);
  }
  EventUtils.sendKey("return", window);

  await input;
}

function getRangeOptions(helper) {
  let rangeSelect = helper.get("range-picker");
  let options = [];
  for (let el of rangeSelect.options) {
    if (!el.disabled) {
      options.push(el.value);
    }
  }
  return options;
}

function getSheetCount(helper) {
  return helper.doc.l10n.getAttributes(helper.get("sheet-count")).args
    .sheetCount;
}

add_task(async function testRangeResetAfterScale() {
  const mockPrinterName = "Fake Printer";
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter(mockPrinterName);
    await helper.startPrint();
    await helper.setupMockPrint();

    helper.mockFilePicker("changeRangeFromScale.pdf");
    await changeRangeTo(helper, "custom");

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
    await changeRangeTo(helper, "custom");
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
      await changeRangeTo(helper, "custom");
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

    await changeRangeTo(helper, "custom");
    await BrowserTestUtils.waitForAttributeRemoval("hidden", customRange);

    ok(!customRange.hidden, "Custom range is showing");
    is(helper.doc.activeElement, customRange, "Custom range field is focused");

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
      "-1,1-": [1, 50],
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
      ok(
        expected.length == pageRanges.length &&
          expected.every((page, index) => page === pageRanges[index]),
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

add_task(async function testPageRangeSelect() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    let pageRangeInput = helper.get("page-range-input");

    await changeRangeTo(helper, "all");
    let pageRanges = pageRangeInput.formatPageRange();
    ok(!pageRanges.length, "Page range for all should be []");

    await changeRangeTo(helper, "current");
    pageRanges = pageRangeInput.formatPageRange();
    ok(
      pageRanges.length == 2 &&
        [1, 1].every((page, index) => page === pageRanges[index]),
      "The first page should be the current page"
    );

    pageRangeInput._validateRangeInput("9", 50);
    pageRanges = pageRangeInput.formatPageRange();
    ok(
      pageRanges.length == 2 &&
        [9, 9].every((page, index) => page === pageRanges[index]),
      `Expected page range for "${pageRanges}" matches [9, 9]"`
    );

    await changeRangeTo(helper, "odd");
    pageRanges = pageRangeInput.formatPageRange();
    ok(
      pageRanges.length == 4 &&
        [1, 1, 3, 3].every((page, index) => page === pageRanges[index]),
      "Page range for odd should be [1, 1, 3, 3]"
    );

    await changeRangeTo(helper, "even");
    pageRanges = pageRangeInput.formatPageRange();
    ok(
      pageRanges.length == 2 &&
        [2, 2].every((page, index) => page === pageRanges[index]),
      "Page range for even should be [2, 2]"
    );
  }, "longerArticle.html");
});

add_task(async function testRangeError() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    await changeRangeTo(helper, "custom");

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

    await changeRangeTo(helper, "custom");

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

    await changeRangeTo(helper, "custom");

    let customRange = helper.get("custom-range");
    let rangeError = helper.get("error-invalid-range");
    ok(rangeError.hidden, "Generic error message is hidden");

    helper.text(customRange, "3");

    await BrowserTestUtils.waitForAttributeRemoval("hidden", rangeError);
    ok(!rangeError.hidden, "Generic error message is showing");

    await changeRangeTo(helper, "all");

    await BrowserTestUtils.waitForCondition(
      () => rangeError.hidden,
      "Wait for range error to be hidden"
    );
    ok(customRange.hidden, "Custom range is hidden");
    is(
      helper.doc.activeElement,
      helper.get("range-picker"),
      "Range picker remains focused"
    );
    await helper.closeDialog();
  });
});

add_task(async function testPageCountChangeNoRangeNoRerender() {
  await PrintHelper.withTestPage(async helper => {
    let customPrinter = "A printer";
    helper.addMockPrinter(customPrinter);

    await helper.startPrint();

    await helper.assertSettingsChanged(
      { printerName: "Mozilla Save to PDF" },
      { printerName: customPrinter },
      async () => {
        let destinationPicker = helper.get("printer-picker");
        destinationPicker.focus();
        await Promise.all([
          helper.waitForPreview(() =>
            helper.dispatchSettingsChange({ printerName: customPrinter })
          ),
          helper.waitForSettingsEvent(),
        ]);
      }
    );

    // Change a setting that will change the number of pages. Since pageRanges
    // is set to "all" then there shouldn't be a re-render because of it.
    let previewUpdateCount = 0;
    ok(!helper.hasPendingPreview, "No preview is pending");
    helper.doc.addEventListener("preview-updated", () => previewUpdateCount++);

    // Ensure the sheet count will change.
    let initialSheetCount = getSheetCount(helper);

    await helper.assertSettingsChanged(
      { marginLeft: 0.5, marginRight: 0.5 },
      { marginLeft: 3, marginRight: 3 },
      async () => {
        await Promise.all([
          helper.waitForPreview(() =>
            helper.dispatchSettingsChange({ marginLeft: 3, marginRight: 3 })
          ),
          BrowserTestUtils.waitForEvent(helper.doc, "page-count"),
          helper.waitForSettingsEvent(),
        ]);
      }
    );

    let newSheetCount = getSheetCount(helper);
    ok(
      initialSheetCount < newSheetCount,
      `There are more sheets now ${initialSheetCount} < ${newSheetCount}`
    );

    ok(!helper.hasPendingPreview, "No preview is pending");
    is(previewUpdateCount, 1, "Only one preview update fired");

    await helper.closeDialog();
  });
});

add_task(async function testPageCountChangeRangeNoRerender() {
  await PrintHelper.withTestPage(async helper => {
    let customPrinter = "A printer";
    helper.addMockPrinter(customPrinter);

    await helper.startPrint();

    await helper.assertSettingsChanged(
      { printerName: "Mozilla Save to PDF", pageRanges: [] },
      { printerName: customPrinter, pageRanges: [1, 1] },
      async () => {
        let destinationPicker = helper.get("printer-picker");
        destinationPicker.focus();
        await Promise.all([
          helper.waitForPreview(() =>
            helper.dispatchSettingsChange({ printerName: customPrinter })
          ),
          helper.waitForSettingsEvent(),
        ]);

        await helper.waitForPreview(async () => {
          await changeRangeTo(helper, "custom");
          helper.text(helper.get("custom-range"), "1");
        });
      }
    );

    // Change a setting that will change the number of pages. Since pageRanges
    // is set to a page that is in the new range, there shouldn't be a re-render.
    let previewUpdateCount = 0;
    ok(!helper.hasPendingPreview, "No preview is pending");
    helper.doc.addEventListener("preview-updated", () => previewUpdateCount++);

    await helper.assertSettingsChanged(
      { marginLeft: 0.5, marginRight: 0.5 },
      { marginLeft: 3, marginRight: 3 },
      async () => {
        await Promise.all([
          helper.waitForPreview(() =>
            helper.dispatchSettingsChange({ marginLeft: 3, marginRight: 3 })
          ),
          BrowserTestUtils.waitForEvent(helper.doc, "page-count"),
          helper.waitForSettingsEvent(),
        ]);
      }
    );

    let newSheetCount = getSheetCount(helper);
    is(newSheetCount, 1, "There's still only one sheet");

    ok(!helper.hasPendingPreview, "No preview is pending");
    is(previewUpdateCount, 1, "Only one preview update fired");

    await helper.closeDialog();
  });
});

add_task(async function testPageCountChangeRangeRerender() {
  await PrintHelper.withTestPage(async helper => {
    let customPrinter = "A printer";
    helper.addMockPrinter(customPrinter);

    await helper.startPrint();

    await helper.assertSettingsChanged(
      { printerName: "Mozilla Save to PDF", pageRanges: [] },
      { printerName: customPrinter, pageRanges: [1, 1] },
      async () => {
        let destinationPicker = helper.get("printer-picker");
        destinationPicker.focus();
        await Promise.all([
          helper.waitForPreview(() =>
            helper.dispatchSettingsChange({ printerName: customPrinter })
          ),
          helper.waitForSettingsEvent(),
        ]);

        await helper.waitForPreview(async () => {
          await changeRangeTo(helper, "custom");
          helper.text(helper.get("custom-range"), "1-");
        });
      }
    );

    // Change a setting that will change the number of pages. Since pageRanges
    // is from 1-N the calculated page range will need to be updated.
    let previewUpdateCount = 0;
    ok(!helper.hasPendingPreview, "No preview is pending");
    helper.doc.addEventListener("preview-updated", () => previewUpdateCount++);
    let renderedTwice = BrowserTestUtils.waitForCondition(
      () => previewUpdateCount == 2
    );

    // Ensure the sheet count will change.
    let initialSheetCount = getSheetCount(helper);

    await helper.assertSettingsChanged(
      { marginLeft: 0.5, marginRight: 0.5 },
      { marginLeft: 3, marginRight: 3 },
      async () => {
        await Promise.all([
          helper.waitForPreview(() =>
            helper.dispatchSettingsChange({ marginLeft: 3, marginRight: 3 })
          ),
          BrowserTestUtils.waitForEvent(helper.doc, "page-count"),
          helper.waitForSettingsEvent(),
        ]);
        await renderedTwice;
      }
    );

    let newSheetCount = getSheetCount(helper);
    ok(
      initialSheetCount < newSheetCount,
      `There are more sheets now ${initialSheetCount} < ${newSheetCount}`
    );
    Assert.deepEqual(
      helper.viewSettings.pageRanges,
      [1, newSheetCount],
      "The new range is the updated full page range"
    );

    ok(!helper.hasPendingPreview, "No preview is pending");
    is(previewUpdateCount, 2, "Preview updated again to show new page range");

    await helper.closeDialog();
  });
});
