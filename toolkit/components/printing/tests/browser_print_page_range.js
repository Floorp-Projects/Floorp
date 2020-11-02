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

add_task(async function testRangeResetAfterScale() {
  const mockPrinterName = "Fake Printer";
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter(mockPrinterName);
    await helper.startPrint();
    await helper.setupMockPrint();

    helper.mockFilePicker("changeRangeFromScale.pdf");
    this.changeAllToCustom(helper);

    await helper.openMoreSettings();
    let scaleRadio = helper.get("percent-scale-choice");
    await helper.waitForPreview(() => helper.click(scaleRadio));
    let percentScale = helper.get("percent-scale");
    await helper.waitForPreview(() => helper.text(percentScale, "200"));

    await helper.waitForPreview(() => {
      helper.text(helper.get("custom-range-start"), "3");
      helper.text(helper.get("custom-range-end"), "3");
    });

    await helper.withClosingFn(async () => {
      await helper.text(percentScale, "10");
      EventUtils.sendKey("return", helper.win);
      helper.resolvePrint();
    });
    helper.assertPrintedWithSettings({
      startPageRange: 1,
      endPageRange: 1,
      scaling: 0.1,
    });
  });
});

add_task(async function testInvalidRangeResetAfterDestinationChange() {
  const mockPrinterName = "Fake Printer";
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter(mockPrinterName);
    await helper.startPrint();

    let destinationPicker = helper.get("printer-picker");
    let startPageRange = helper.get("custom-range-start");

    await helper.assertSettingsChanged(
      { printRange: 0 },
      { printRange: 1 },
      async () => {
        await helper.waitForPreview(() => changeAllToCustom(helper));
      }
    );

    let rangeError = helper.get("error-invalid-start-range-overflow");

    await helper.assertSettingsNotChanged({ startPageRange: 1 }, async () => {
      ok(rangeError.hidden, "Range error is hidden");
      await helper.text(startPageRange, "9");
      await BrowserTestUtils.waitForAttributeRemoval("hidden", rangeError);
      ok(!rangeError.hidden, "Range error is showing");
    });

    is(destinationPicker.disabled, false, "Destination picker is enabled");

    // Select a new printer
    await helper.dispatchSettingsChange({ printerName: mockPrinterName });
    await BrowserTestUtils.waitForCondition(
      () => rangeError.hidden,
      "Wait for range error to be hidden"
    );
    is(startPageRange.value, "1", "Start page range has reset to 1");

    await helper.closeDialog();
  });
});
