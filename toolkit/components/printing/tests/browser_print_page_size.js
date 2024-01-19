/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const pdfPrinterName = "Mozilla Save to PDF";
const someOtherPrinter = "Other Printer";

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

add_task(async function testShowAndHidePaperSizeSectionWithPageSize() {
  await PrintHelper.withTestPage(async helper => {
    await helper.addMockPrinter(someOtherPrinter);
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.save_as_pdf.use_page_rule_size_as_paper_size.enabled", true],
      ],
    });
    await helper.startPrint();
    await helper.setupMockPrint();

    helper.assertSettingsMatch({
      printerName: pdfPrinterName,
      usePageRuleSizeAsPaperSize: true,
    });

    await helper.openMoreSettings();

    let paperSize = helper.get("paper-size");

    ok(BrowserTestUtils.isHidden(paperSize), "Paper size section is hidden");

    await helper.waitForSettingsEvent(async () => {
      await changeDestination(helper, "down");
    });
    await helper.awaitAnimationFrame();

    helper.assertSettingsMatch({
      printerName: someOtherPrinter,
      usePageRuleSizeAsPaperSize: false,
    });

    ok(BrowserTestUtils.isVisible(paperSize), "Paper size section is shown");

    await helper.closeDialog();
  }, "page_size.html");
});

add_task(async function testShowPaperSizeSectionWithoutPageSize() {
  await PrintHelper.withTestPage(async helper => {
    await helper.addMockPrinter(someOtherPrinter);
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.save_as_pdf.use_page_rule_size_as_paper_size.enabled", true],
      ],
    });
    await helper.startPrint();
    await helper.setupMockPrint();

    helper.assertSettingsMatch({
      printerName: pdfPrinterName,
      usePageRuleSizeAsPaperSize: true,
    });

    await helper.openMoreSettings();

    let paperSize = helper.get("paper-size");

    ok(BrowserTestUtils.isVisible(paperSize), "Paper size section is shown");

    await helper.waitForSettingsEvent(async () => {
      await changeDestination(helper, "down");
    });
    await helper.awaitAnimationFrame();

    helper.assertSettingsMatch({
      printerName: someOtherPrinter,
      usePageRuleSizeAsPaperSize: false,
    });

    ok(
      BrowserTestUtils.isVisible(paperSize),
      "Paper size section is still shown"
    );

    await helper.closeDialog();
  });
});

add_task(async function testCustomPageSizePassedToPrinter() {
  await PrintHelper.withTestPage(async helper => {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.save_as_pdf.use_page_rule_size_as_paper_size.enabled", true],
      ],
    });
    await helper.startPrint();
    await helper.setupMockPrint();

    is(helper.settings.paperWidth.toFixed(1), "4.0", "Check paperWidth");
    is(helper.settings.paperHeight.toFixed(1), "12.0", "Check paperHeight");
    is(
      helper.settings.orientation,
      Ci.nsIPrintSettings.kPortraitOrientation,
      "Check orientation"
    );
    is(
      helper.settings.paperSizeUnit,
      helper.settings.kPaperSizeInches,
      "Check paperSizeUnit"
    );

    await helper.closeDialog();
  }, "page_size.html");
});

add_task(async function testNamedPageSizePassedToPrinter() {
  await PrintHelper.withTestPage(async helper => {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.save_as_pdf.use_page_rule_size_as_paper_size.enabled", true],
      ],
    });
    await helper.startPrint();
    await helper.setupMockPrint();

    helper.assertSettingsMatch({
      printerName: pdfPrinterName,
      usePageRuleSizeAsPaperSize: true,
    });

    is(helper.settings.paperId, "iso_a5", "Check paper id is A5");
    is(
      helper.settings.paperWidth.toFixed(1),
      "5.8",
      "Check paperWidth is ~148mm (in inches)"
    );
    is(
      helper.settings.paperHeight.toFixed(1),
      "8.3",
      "Check paperHeight is ~210mm (in inches)"
    );
    is(
      helper.settings.paperSizeUnit,
      helper.settings.kPaperSizeInches,
      "Check paperSizeUnit"
    );
    is(
      helper.settings.orientation,
      Ci.nsIPrintSettings.kPortraitOrientation,
      "Check orientation"
    );
    await helper.closeDialog();
  }, "page_size_a5.html");
});

add_task(async function testLandscapePageSizePassedToPrinter() {
  await PrintHelper.withTestPage(async helper => {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.save_as_pdf.use_page_rule_size_as_paper_size.enabled", true],
      ],
    });
    await helper.startPrint();
    await helper.setupMockPrint();

    helper.assertSettingsMatch({
      printerName: pdfPrinterName,
      usePageRuleSizeAsPaperSize: true,
    });

    is(helper.settings.paperId, "iso_a5", "Check paper id is A5");
    is(
      helper.settings.orientation,
      Ci.nsIPrintSettings.kLandscapeOrientation,
      "Check orientation is landscape"
    );
    is(
      helper.settings.paperWidth.toFixed(1),
      "5.8",
      "Check paperWidth is ~148mm (in inches)"
    );
    is(
      helper.settings.paperHeight.toFixed(1),
      "8.3",
      "Check paperHeight is ~210mm (in inches)"
    );
    is(
      helper.settings.paperSizeUnit,
      helper.settings.kPaperSizeInches,
      "Check paperSizeUnit"
    );

    await helper.closeDialog();
  }, "page_size_a5_landscape.html");
});

add_task(async function testZeroSizePassedToPrinter() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.setupMockPrint();

    let pageSize = helper.get("paper-size");
    let orientation = helper.get("orientation");

    ok(BrowserTestUtils.isVisible(pageSize), "Fallback to page size section");
    ok(BrowserTestUtils.isVisible(orientation), "Orientation picker is shown");

    await helper.closeDialog();
  });
}, "page_size_zero.html");

add_task(async function testZeroWidthPassedToPrinter() {
  await PrintHelper.withTestPage(async helper => {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.save_as_pdf.use_page_rule_size_as_paper_size.enabled", true],
      ],
    });
    await helper.startPrint();
    await helper.setupMockPrint();

    let pageSize = helper.get("paper-size");
    let orientation = helper.get("orientation");

    ok(BrowserTestUtils.isVisible(pageSize), "Fallback to page size section");
    ok(BrowserTestUtils.isVisible(orientation), "Orientation picker is shown");

    await helper.closeDialog();
  });
}, "page_size_zero_width.html");

add_task(async function testDefaultSizePassedToPrinter() {
  await PrintHelper.withTestPage(async helper => {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.save_as_pdf.use_page_rule_size_as_paper_size.enabled", true],
      ],
    });
    await helper.startPrint();
    await helper.setupMockPrint();

    helper.assertSettingsMatch({
      printerName: pdfPrinterName,
      usePageRuleSizeAsPaperSize: true,
    });

    is(helper.settings.paperWidth.toFixed(1), "8.5", "Check paperWidth");
    is(helper.settings.paperHeight.toFixed(1), "11.0", "Check paperHeight");
    is(
      helper.settings.paperSizeUnit,
      helper.settings.kPaperSizeInches,
      "Check paperSizeUnit"
    );

    await helper.closeDialog();
  });
});
