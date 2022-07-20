/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let pdfPrinterName = "Mozilla Save to PDF";
let fastPrinterName = "Fast";
let slowPrinterName = "Slow";

async function setupPrinters(helper) {
  helper.addMockPrinter(fastPrinterName);

  let resolvePrinterInfo;
  helper.addMockPrinter({
    name: slowPrinterName,
    printerInfoPromise: new Promise(resolve => {
      resolvePrinterInfo = resolve;
    }),
  });

  await SpecialPowers.pushPrefEnv({
    set: [["print.printer_Slow.print_orientation", 1]],
  });

  return resolvePrinterInfo;
}

async function changeDestination(helper, dir) {
  let picker = helper.get("printer-picker");
  let changed = BrowserTestUtils.waitForEvent(picker, "change");

  let pickerOpened = BrowserTestUtils.waitForSelectPopupShown(window);
  picker.focus();
  EventUtils.sendKey("space", helper.win);
  await pickerOpened;
  EventUtils.sendKey(dir, window);
  EventUtils.sendKey("return", window);
  await changed;
}

function assertFormEnabled(form) {
  for (let element of form.elements) {
    if (element.hasAttribute("disallowed")) {
      ok(element.disabled, `${element.id} is disallowed`);
    } else {
      ok(!element.disabled, `${element.id} is enabled`);
    }
  }
}

function assertFormDisabled(form) {
  for (let element of form.elements) {
    if (element.id == "printer-picker" || element.id == "cancel-button") {
      ok(!element.disabled, `${element.id} is enabled`);
    } else {
      ok(element.disabled, `${element.id} is disabled`);
    }
  }
}

add_task(async function testSlowDestinationChange() {
  await PrintHelper.withTestPage(async helper => {
    let resolvePrinterInfo = await setupPrinters(helper);
    await helper.startPrint();

    let destinationPicker = helper.get("printer-picker");
    let printForm = helper.get("print");

    info("Changing to fast printer should change settings");
    await helper.assertSettingsChanged(
      { printerName: pdfPrinterName, orientation: 0 },
      { printerName: fastPrinterName, orientation: 0 },
      async () => {
        await changeDestination(helper, "down");
        is(destinationPicker.value, fastPrinterName, "Fast printer selected");
        // Wait one frame so the print settings promises resolve.
        await helper.awaitAnimationFrame();
        assertFormEnabled(printForm);
      }
    );

    info("Changing to slow printer should not change settings yet");
    await helper.assertSettingsNotChanged(
      { printerName: fastPrinterName, orientation: 0 },
      async () => {
        await changeDestination(helper, "down");
        is(destinationPicker.value, slowPrinterName, "Slow printer selected");
        // Wait one frame, since the settings are blocked on resolvePrinterInfo
        // the settings shouldn't change.
        await helper.awaitAnimationFrame();
        assertFormDisabled(printForm);
      }
    );

    await helper.assertSettingsChanged(
      { printerName: fastPrinterName, orientation: 0 },
      { printerName: slowPrinterName, orientation: 1 },
      async () => {
        resolvePrinterInfo();
        await helper.waitForSettingsEvent();
        assertFormEnabled(printForm);
      }
    );

    await helper.closeDialog();
  });
});

add_task(async function testSwitchAwayFromSlowDestination() {
  await PrintHelper.withTestPage(async helper => {
    let resolvePrinterInfo = await setupPrinters(helper);
    await helper.startPrint();

    let destinationPicker = helper.get("printer-picker");
    let printForm = helper.get("print");

    // Load the fast printer.
    await helper.waitForSettingsEvent(async () => {
      await changeDestination(helper, "down");
    });
    await helper.awaitAnimationFrame();
    assertFormEnabled(printForm);

    // "Load" the slow printer.
    await changeDestination(helper, "down");
    is(destinationPicker.value, slowPrinterName, "Slow printer selected");
    // Wait an animation frame, since there's no settings event.
    await helper.awaitAnimationFrame();
    assertFormDisabled(printForm);

    // Switch back to the fast printer.
    await helper.waitForSettingsEvent(async () => {
      await changeDestination(helper, "up");
    });
    helper.assertSettingsMatch({
      printerName: fastPrinterName,
      orientation: 0,
    });

    await helper.awaitAnimationFrame();
    assertFormEnabled(printForm);

    // Let the slow printer settings resolve, the orientation shouldn't change.
    resolvePrinterInfo();
    // Wait so the settings event can trigger, if this case isn't handled.
    await helper.awaitAnimationFrame();
    helper.assertSettingsMatch({
      printerName: fastPrinterName,
      orientation: 0,
    });
    assertFormEnabled(printForm);

    await helper.closeDialog();
  });
});
