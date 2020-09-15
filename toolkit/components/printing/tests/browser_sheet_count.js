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

    // TODO: Ideally, this test would set numCopies=4 for a "real" printer and
    // verify that the value is ignored when switching to the PDF printer. Since
    // we don't have any "real" printers set up for testing yet, fake the printer
    // and force the component to update.
    let { settings, viewSettings } = helper;
    is(viewSettings.numCopies, 1, "numCopies is 1 in viewSettings");
    settings.outputFormat = Ci.nsIPrintSettings.kOutputFormatNative;
    settings.printerName = "My real printer";
    is(viewSettings.numCopies, 4, "numCopies is 4 in viewSettings");

    // Manually update the components.
    helper.get("print").update(viewSettings);
    sheetCount.update(viewSettings);
    numCopies.update(viewSettings);

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
