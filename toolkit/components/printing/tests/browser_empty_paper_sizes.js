/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testSanityCheckPaperList() {
  const mockPrinterName = "Fake Printer";
  await PrintHelper.withTestPage(async helper => {
    let paperList = [
      PrintHelper.createMockPaper({
        id: "regular",
        name: "Regular Paper",
      }),
      PrintHelper.createMockPaper({
        id: "large",
        name: "Large Size",
        width: 720,
        height: 1224,
      }),
    ];
    helper.addMockPrinter({ name: mockPrinterName, paperList });
    await helper.startPrint();
    await helper.dispatchSettingsChange({ printerName: mockPrinterName });
    await helper.waitForSettingsEvent();

    is(
      helper.settings.printerName,
      mockPrinterName,
      "The Fake Printer is current printer"
    );
    is(
      Object.values(helper.win.PrintSettingsViewProxy.availablePaperSizes)
        .length,
      2,
      "There are 2 paper sizes"
    );
    ok(
      helper.win.PrintSettingsViewProxy.availablePaperSizes.regular,
      "'regular' paper size is available"
    );
    ok(
      helper.win.PrintSettingsViewProxy.availablePaperSizes.large,
      "'large' paper size is available"
    );
  });
});

add_task(async function testEmptyPaperListGetsFallbackPaperSizes() {
  const mockPrinterName = "Fake Printer";
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter(mockPrinterName);
    await helper.startPrint();

    is(
      Object.values(helper.win.PrintSettingsViewProxy.availablePrinters).length,
      2,
      "There are 2 available printers"
    );
    ok(
      helper.win.PrintSettingsViewProxy.availablePrinters[mockPrinterName],
      "The Fake Printer is one of our availablePrinters"
    );

    await helper.dispatchSettingsChange({ printerName: mockPrinterName });
    await helper.waitForSettingsEvent();

    is(
      helper.settings.printerName,
      mockPrinterName,
      "The Fake Printer is current printer"
    );
    is(
      helper.get("printer-picker").value,
      mockPrinterName,
      "The Fake Printer is selected"
    );

    let printerList = Cc["@mozilla.org/gfx/printerlist;1"].createInstance(
      Ci.nsIPrinterList
    );
    let fallbackPaperList = await printerList.fallbackPaperList;
    let paperPickerSizes = Array.from(
      helper.get("paper-size-picker").options
    ).map(o => o.value);
    for (let paper of fallbackPaperList) {
      ok(
        helper.win.PrintSettingsViewProxy.availablePaperSizes[paper.id],
        "Fallback paper size: " + paper.id + " is available"
      );
      ok(
        paperPickerSizes.includes(paper.id),
        "There is a paper size options for " + paper.id
      );
    }
    await helper.closeDialog();
  });
});
