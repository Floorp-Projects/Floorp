/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let badPrinterName = "Bad";
let otherPrinterName = "Fallback";

async function setupPrinters(helper) {
  let badPrinter = helper.addMockPrinter({
    name: badPrinterName,
  });

  let badPrinterInfo = await badPrinter.printerInfo;
  badPrinterInfo.defaultSettings.printerName = "";

  helper.addMockPrinter(otherPrinterName);

  await SpecialPowers.pushPrefEnv({
    set: [["print_printer", badPrinterName]],
  });
}

add_task(async function testBadPrinterSettings() {
  await PrintHelper.withTestPage(async helper => {
    await setupPrinters(helper);
    await helper.startPrint();

    let destinationPicker = helper.get("printer-picker");
    // Fallback can be any other printer, the fallback or save to pdf printer.
    isnot(
      destinationPicker.value,
      badPrinterName,
      "A fallback printer is selected"
    );

    await helper.closeDialog();
  });
});
