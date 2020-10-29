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
