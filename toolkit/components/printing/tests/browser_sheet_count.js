/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testSheetCount() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    let sheetCountElement = helper.get("sheet-count");
    let { id } = helper.doc.l10n.getAttributes(sheetCountElement);
    is(id, "printui-sheets-count", "The l10n id is correct");
    let initialSheetCount = helper.sheetCount;
    ok(initialSheetCount >= 1, "There is an initial sheet count");

    await helper.openMoreSettings();

    let scaleRadio = helper.get("percent-scale-choice");
    await helper.waitForPreview(() => helper.click(scaleRadio));

    let percentScale = helper.get("percent-scale");
    await helper.waitForPreview(() => helper.text(percentScale, "200"));

    let zoomedSheetCount = helper.sheetCount;
    ok(zoomedSheetCount > initialSheetCount, "The sheet count increased");

    // Since we're using the Save to PDF printer, the numCopies element should
    // be hidden and its value ignored.
    let numCopies = helper.get("copies-count");
    ok(BrowserTestUtils.isHidden(numCopies), "numCopies element is hidden");
    helper.dispatchSettingsChange({
      numCopies: 4,
    });
    is(
      helper.sheetCount,
      zoomedSheetCount,
      "numCopies is ignored for Save to PDF printer"
    );

    is(helper.viewSettings.numCopies, 1, "numCopies is 1 in viewSettings");

    // We don't have any "real" printers set up for testing yet, so insert a modified
    // copy of the PDF printer which pretends to be real, and switch to that
    // to triggers the component to update.
    let realPrinterName = "My real printer";
    let pdfPrinterInfo =
      helper.win.PrintSettingsViewProxy.availablePrinters[
        PrintUtils.SAVE_TO_PDF_PRINTER
      ];
    let mockPrinterInfo = Object.assign({}, pdfPrinterInfo, {});
    mockPrinterInfo.settings = pdfPrinterInfo.settings.clone();
    mockPrinterInfo.settings.outputFormat =
      Ci.nsIPrintSettings.kOutputFormatNative;
    mockPrinterInfo.settings.printerName = realPrinterName;

    helper.win.PrintSettingsViewProxy.availablePrinters[realPrinterName] =
      mockPrinterInfo;
    await helper.dispatchSettingsChange({
      printerName: realPrinterName,
    });
    await helper.awaitAnimationFrame();

    let { settings, viewSettings } = helper;

    is(
      settings.printerName,
      realPrinterName,
      "Sanity check the current settings have the new printerName"
    );
    is(
      settings.outputFormat,
      Ci.nsIPrintSettings.kOutputFormatNative,
      "The new printer has the correct outputFormat"
    );
    is(viewSettings.numCopies, 4, "numCopies is 4 in viewSettings");

    // numCopies is now visible and sheetCount is multiplied by numCopies.
    ok(BrowserTestUtils.isVisible(numCopies), "numCopies element is visible");
    is(numCopies.value, "4", "numCopies displays the correct value");
    is(
      helper.sheetCount,
      zoomedSheetCount * 4,
      "numCopies is used when using a non-PDF printer"
    );

    await helper.closeDialog();
  });
});

add_task(async function testSheetCountPageRange() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.waitForPreview(() =>
      helper.dispatchSettingsChange({
        shrinkToFit: false,
        scaling: 2,
      })
    );

    await BrowserTestUtils.waitForCondition(
      () => helper.sheetCount != 1,
      "Wait for sheet count to update"
    );
    let sheets = helper.sheetCount;
    ok(sheets >= 3, "There are at least 3 pages");

    // Set page range to 2-3, sheet count should be 2.
    await helper.waitForPreview(() =>
      helper.dispatchSettingsChange({
        pageRanges: [2, 3],
      })
    );

    sheets = helper.sheetCount;
    is(sheets, 2, "There are now only 2 pages shown");
  });
});

// Test that enabling duplex printing updates the sheet count accordingly.
add_task(async function testSheetCountDuplex() {
  await PrintHelper.withTestPage(async helper => {
    const mockPrinterName = "DuplexCapablePrinter";
    const printer = helper.addMockPrinter(mockPrinterName);
    printer.supportsDuplex = Promise.resolve(true);

    await helper.startPrint();
    await helper.dispatchSettingsChange({ printerName: mockPrinterName });

    // Set scale and shinkToFit to make the document
    // bigger so that it spans multiple pages.
    await helper.waitForPreview(() =>
      helper.dispatchSettingsChange({
        shrinkToFit: false,
        scaling: 4,
        duplex: Ci.nsIPrintSettings.kDuplexNone,
      })
    );
    await BrowserTestUtils.waitForCondition(
      () => helper.sheetCount != 1,
      "Wait for sheet count to update"
    );
    let singleSidedSheets = helper.sheetCount;
    ok(singleSidedSheets >= 2, "There are at least 2 pages");

    // Turn on long-edge duplex printing and ensure the sheet count is halved.
    await helper.waitForSettingsEvent(() =>
      helper.dispatchSettingsChange({
        duplex: Ci.nsIPrintSettings.kDuplexFlipOnLongEdge,
      })
    );
    await BrowserTestUtils.waitForCondition(
      () => helper.sheetCount != singleSidedSheets,
      "Wait for sheet count to update"
    );
    let duplexLongEdgeSheets = helper.sheetCount;
    is(
      duplexLongEdgeSheets,
      Math.ceil(singleSidedSheets / 2),
      "Long-edge duplex printing halved the sheet count"
    );

    // Turn off duplex printing
    await helper.waitForSettingsEvent(() =>
      helper.dispatchSettingsChange({
        duplex: Ci.nsIPrintSettings.kDuplexNone,
      })
    );
    await BrowserTestUtils.waitForCondition(
      () => helper.sheetCount == singleSidedSheets,
      "Wait for sheet count to update"
    );

    // Turn on short-edge duplex printing and ensure the
    // sheet count matches the long-edge duplex sheet count.
    await helper.waitForSettingsEvent(() =>
      helper.dispatchSettingsChange({
        duplex: Ci.nsIPrintSettings.kDuplexFlipOnShortEdge,
      })
    );
    await BrowserTestUtils.waitForCondition(
      () => helper.sheetCount != singleSidedSheets,
      "Wait for sheet count to update"
    );
    let duplexShortEdgeSheets = helper.sheetCount;
    is(
      duplexShortEdgeSheets,
      duplexLongEdgeSheets,
      "Short-edge duplex printing halved the sheet count"
    );
  });
});

// Test that enabling duplex printing with multiple copies updates the
// sheet count accordingly.
add_task(async function testSheetCountDuplexWithCopies() {
  // Use different scale values to exercise printing of different page counts
  for (let scale of [2, 3, 4, 5]) {
    await TestDuplexNumCopiesAtScale(scale);
  }
});

// Enable duplex and numCopies=2 with the provided scale value and check
// that the sheet count is correct.
async function TestDuplexNumCopiesAtScale(scale) {
  await PrintHelper.withTestPage(async helper => {
    const mockPrinterName = "DuplexCapablePrinter";
    const printer = helper.addMockPrinter(mockPrinterName);
    printer.supportsDuplex = Promise.resolve(true);

    await helper.startPrint();
    await helper.dispatchSettingsChange({ printerName: mockPrinterName });

    // Set scale and shinkToFit to make the document
    // bigger so that it spans multiple pages.
    await helper.waitForPreview(() =>
      helper.dispatchSettingsChange({
        shrinkToFit: false,
        scaling: scale,
        duplex: Ci.nsIPrintSettings.kDuplexNone,
      })
    );
    await BrowserTestUtils.waitForCondition(
      () => helper.sheetCount != 1,
      "Wait for sheet count to update"
    );
    let singleSidedSheets = helper.sheetCount;

    // Chnage to two copies
    await helper.waitForSettingsEvent(() =>
      helper.dispatchSettingsChange({
        numCopies: 2,
      })
    );
    await BrowserTestUtils.waitForCondition(
      () => helper.sheetCount != singleSidedSheets,
      "Wait for sheet count to update"
    );
    let twoCopiesSheetCount = helper.sheetCount;

    // Turn on duplex printing.
    await helper.waitForSettingsEvent(() =>
      helper.dispatchSettingsChange({
        duplex: Ci.nsIPrintSettings.kDuplexFlipOnLongEdge,
      })
    );
    await BrowserTestUtils.waitForCondition(
      () => helper.sheetCount != twoCopiesSheetCount,
      "Wait for sheet count to update"
    );
    let duplexTwoCopiesSheetCount = helper.sheetCount;

    // Check sheet count accounts for duplex and numCopies.
    is(
      duplexTwoCopiesSheetCount,
      Math.ceil(singleSidedSheets / 2) * 2,
      "Duplex with 2 copies sheet count is correct"
    );

    await helper.closeDialog();
  });
}

add_task(async function testPagesPerSheetCount() {
  await PrintHelper.withTestPage(async helper => {
    let mockPrinterName = "A real printer!";
    helper.addMockPrinter(mockPrinterName);

    await SpecialPowers.pushPrefEnv({
      set: [["print_printer", mockPrinterName]],
    });

    await helper.startPrint();

    await helper.waitForPreview(() =>
      helper.dispatchSettingsChange({
        shrinkToFit: false,
        scaling: 2,
      })
    );

    await BrowserTestUtils.waitForCondition(
      () => helper.sheetCount != 1,
      "Wait for sheet count to update"
    );
    let sheets = helper.sheetCount;

    ok(sheets > 1, "There are multiple pages");

    await helper.openMoreSettings();
    let pagesPerSheet = helper.get("pages-per-sheet-picker");
    ok(BrowserTestUtils.isVisible(pagesPerSheet), "Pages per sheet is shown");
    pagesPerSheet.focus();

    let popupOpen = BrowserTestUtils.waitForSelectPopupShown(window);

    EventUtils.sendKey("space", helper.win);

    await popupOpen;

    let numberMove =
      [...pagesPerSheet.options].map(o => o.value).indexOf("16") -
      pagesPerSheet.selectedIndex;

    for (let i = 0; i < numberMove; i++) {
      EventUtils.sendKey("down", window);
      if (document.activeElement.value == 16) {
        break;
      }
    }

    await helper.waitForPreview(() => EventUtils.sendKey("return", window));

    sheets = helper.sheetCount;
    is(sheets, 1, "There's only one sheet now");

    await helper.waitForSettingsEvent(() =>
      helper.dispatchSettingsChange({ numCopies: 5 })
    );

    sheets = helper.sheetCount;
    is(sheets, 5, "Copies are handled with pages per sheet correctly");

    await helper.closeDialog();
  });
});

add_task(async function testUpdateCopiesNoPreviewUpdate() {
  const mockPrinterName = "Fake Printer";
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter(mockPrinterName);
    await helper.startPrint();

    await helper.waitForSettingsEvent(() =>
      helper.dispatchSettingsChange({ numCopies: 5 })
    );

    ok(
      !helper.win.PrintEventHandler._updatePrintPreviewTask.isArmed,
      "Preview Task is not armed"
    );

    await helper.waitForPreview(() =>
      helper.dispatchSettingsChange({ printerName: mockPrinterName })
    );

    await helper.waitForSettingsEvent(() =>
      helper.dispatchSettingsChange({ numCopies: 2 })
    );
    ok(
      !helper.win.PrintEventHandler._updatePrintPreviewTask.isArmed,
      "Preview Task is not armed"
    );

    await helper.closeDialog();
  });
});
