/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function changeToOption(helper, index) {
  return helper.waitForSettingsEvent(async function() {
    let select = helper.get("duplex-select");
    select.focus();
    select.scrollIntoView({ block: "center" });

    let popupOpen = BrowserTestUtils.waitForSelectPopupShown(window);
    EventUtils.sendKey("space", helper.win);
    await popupOpen;

    let selectedIndex = select.selectedIndex;
    info(`Looking for ${index} from ${selectedIndex}`);
    while (selectedIndex != index) {
      if (index > selectedIndex) {
        EventUtils.sendKey("down", window);
        selectedIndex++;
      } else {
        EventUtils.sendKey("up", window);
        selectedIndex--;
      }
    }
    EventUtils.sendKey("return", window);
  });
}

add_task(async function testPDFPrinterIsNonDuplex() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();

    is(
      helper.settings.printerName,
      "Mozilla Save to PDF",
      "Mozilla Save to PDF is the current printer."
    );

    const duplexSection = helper.get("two-sided-printing");
    ok(
      duplexSection.hidden,
      "The two-sided printing section should be hidden when the printer does not support duplex."
    );

    helper.assertSettingsMatch({ duplex: Ci.nsIPrintSettings.kDuplexNone });

    await helper.closeDialog();
  });
});

add_task(async function testToggleDuplexWithPortraitOrientation() {
  const mockPrinterName = "DuplexWithPortrait";
  await PrintHelper.withTestPage(async helper => {
    const printer = helper.addMockPrinter(mockPrinterName);
    printer.supportsDuplex = Promise.resolve(true);

    await helper.startPrint();
    await helper.dispatchSettingsChange({ printerName: mockPrinterName });
    await helper.awaitAnimationFrame();
    await helper.openMoreSettings();

    is(
      helper.settings.printerName,
      mockPrinterName,
      "The Fake Printer is current printer"
    );

    const duplexSection = helper.get("two-sided-printing");
    ok(
      !duplexSection.hidden,
      "The two-sided printing section should not be hidden when the printer supports duplex."
    );

    helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexNone,
    });

    await changeToOption(helper, 1);
    helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexFlipOnLongEdge,
    });

    await changeToOption(helper, 2);
    helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexFlipOnShortEdge,
    });

    await changeToOption(helper, 0);
    helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexNone,
    });

    await helper.closeDialog();
  });
});

add_task(async function testToggleDuplexWithLandscapeOrientation() {
  const mockPrinterName = "DuplexWithLandscape";
  await PrintHelper.withTestPage(async helper => {
    const printer = helper.addMockPrinter(mockPrinterName);
    printer.supportsDuplex = Promise.resolve(true);

    await helper.startPrint();
    await helper.dispatchSettingsChange({ printerName: mockPrinterName });
    await helper.awaitAnimationFrame();
    await helper.openMoreSettings();

    is(
      helper.settings.printerName,
      mockPrinterName,
      "The Fake Printer is current printer"
    );

    const duplexSection = helper.get("two-sided-printing");
    ok(
      !duplexSection.hidden,
      "The two-sided printing section should not be hidden when the printer supports duplex."
    );

    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexNone,
    });

    await helper.dispatchSettingsChange({ orientation: 1 });
    await helper.awaitAnimationFrame();
    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kLandscapeOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexNone,
    });

    await changeToOption(helper, 1);
    helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kLandscapeOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexFlipOnLongEdge,
    });

    await changeToOption(helper, 2);
    helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kLandscapeOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexFlipOnShortEdge,
    });

    await changeToOption(helper, 0);
    helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kLandscapeOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexNone,
    });

    await helper.closeDialog();
  });
});

add_task(async function testSwitchOrientationWithDuplexEnabled() {
  const mockPrinterName = "ToggleOrientationPrinter";
  await PrintHelper.withTestPage(async helper => {
    const printer = helper.addMockPrinter(mockPrinterName);
    printer.supportsDuplex = Promise.resolve(true);

    await helper.startPrint();
    await helper.dispatchSettingsChange({ printerName: mockPrinterName });
    await helper.awaitAnimationFrame();
    await helper.openMoreSettings();

    is(
      helper.settings.printerName,
      mockPrinterName,
      "The Fake Printer is current printer"
    );

    const duplexSection = helper.get("two-sided-printing");
    ok(
      !duplexSection.hidden,
      "The two-sided printing section should not be hidden when the printer supports duplex."
    );

    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexNone,
    });

    await changeToOption(helper, 1);

    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexFlipOnLongEdge,
    });

    await helper.dispatchSettingsChange({ orientation: 1 });
    await helper.awaitAnimationFrame();
    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kLandscapeOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexFlipOnLongEdge,
    });

    await helper.dispatchSettingsChange({ orientation: 0 });
    await helper.awaitAnimationFrame();
    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexFlipOnLongEdge,
    });

    await helper.closeDialog();
  });
});
