/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

    helper.assertSettingsMatch({ duplex: Ci.nsIPrintSettings.kSimplex });

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
      duplex: Ci.nsIPrintSettings.kSimplex,
    });

    await helper.click(helper.get("duplex-enabled"));
    helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexHorizontal,
    });

    await helper.click(helper.get("duplex-enabled"));
    helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kSimplex,
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
      duplex: Ci.nsIPrintSettings.kSimplex,
    });

    await helper.dispatchSettingsChange({ orientation: 1 });
    await helper.awaitAnimationFrame();
    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kLandscapeOrientation,
      duplex: Ci.nsIPrintSettings.kSimplex,
    });

    await helper.click(helper.get("duplex-enabled"));
    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kLandscapeOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexHorizontal,
    });

    await helper.click(helper.get("duplex-enabled"));
    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kLandscapeOrientation,
      duplex: Ci.nsIPrintSettings.kSimplex,
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
      duplex: Ci.nsIPrintSettings.kSimplex,
    });

    await helper.click(helper.get("duplex-enabled"));
    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexHorizontal,
    });

    await helper.dispatchSettingsChange({ orientation: 1 });
    await helper.awaitAnimationFrame();
    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kLandscapeOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexHorizontal,
    });

    await helper.dispatchSettingsChange({ orientation: 0 });
    await helper.awaitAnimationFrame();
    await helper.assertSettingsMatch({
      orientation: Ci.nsIPrintSettings.kPortraitOrientation,
      duplex: Ci.nsIPrintSettings.kDuplexHorizontal,
    });

    await helper.closeDialog();
  });
});
