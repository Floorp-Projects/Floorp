/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function getSheetCount(el) {
  return el.ownerDocument.l10n.getAttributes(el).args.sheetCount;
}

add_task(async function testSheetCount() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();

    let sheetCount = helper.get("sheet-count");
    let { id } = helper.doc.l10n.getAttributes(sheetCount);
    is(id, "printui-sheets-count", "The l10n id is correct");
    let initialSheetCount = getSheetCount(sheetCount);
    ok(initialSheetCount >= 1, "There is an initial sheet count");

    await helper.openMoreSettings();

    let scaleRadio = helper.get("percent-scale-choice");
    await helper.waitForPreview(() => helper.click(scaleRadio));

    let percentScale = helper.get("percent-scale");
    await helper.waitForPreview(() => helper.text(percentScale, "200"));

    let zoomedSheetCount = getSheetCount(sheetCount);
    ok(zoomedSheetCount > initialSheetCount, "The sheet count increased");

    // Since we're using the Save to PDF printer, the numCopies element should
    // be hidden and its value ignored.
    let numCopies = helper.get("copies-count");
    ok(BrowserTestUtils.is_hidden(numCopies), "numCopies element is hidden");
    helper.dispatchSettingsChange({
      numCopies: 4,
    });
    is(
      getSheetCount(sheetCount),
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

    helper.win.PrintSettingsViewProxy.availablePrinters[
      realPrinterName
    ] = mockPrinterInfo;
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
    ok(BrowserTestUtils.is_visible(numCopies), "numCopies element is visible");
    is(numCopies.value, "4", "numCopies displays the correct value");
    is(
      getSheetCount(sheetCount),
      zoomedSheetCount * 4,
      "numCopies is used when using a non-PDF printer"
    );

    await helper.closeDialog();
  });
});
