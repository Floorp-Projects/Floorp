"use strict";

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

async function selectPaperOptionWithValue(helper, value) {
  let paperSelect = helper.get("paper-size-picker");
  paperSelect.dispatchSettingsChange({
    paperId: value,
  });
  await helper.awaitAnimationFrame();
}

add_task(async function testBadPaperSizeUnitCorrection() {
  await PrintHelper.withTestPage(async helper => {
    // Set prefs to select a non-default paper size
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.printer_Mozilla_Save_to_PDF.print_paper_id", "na_letter"],
        // paperSizeUnit is a bogus value, but the dimensions are correct for inches
        ["print.printer_Mozilla_Save_to_PDF.print_paper_size_unit", 99],
        ["print.printer_Mozilla_Save_to_PDF.print_paper_height", "11.0"],
        ["print.printer_Mozilla_Save_to_PDF.print_paper_width", "8.50"],
      ],
    });
    await helper.startPrint();

    let paperSelect = helper.get("paper-size-picker");
    is(paperSelect.value, "na_letter", "The expected paper size is selected");
    is(
      helper.viewSettings.paperId,
      "na_letter",
      "The settings have the expected paperId"
    );
    is(
      helper.viewSettings.paperSizeUnit,
      helper.settings.kPaperSizeInches,
      "Check paperSizeUnit"
    );
    is(helper.viewSettings.paperWidth.toFixed(1), "8.5", "Check paperWidth");
    is(helper.viewSettings.paperHeight.toFixed(1), "11.0", "Check paperHeight");

    await selectPaperOptionWithValue(helper, "iso_a3");
    is(paperSelect.value, "iso_a3", "The expected paper size is selected");
    is(
      helper.viewSettings.paperId,
      "iso_a3",
      "The settings have the expected paperId"
    );
    is(
      helper.viewSettings.paperSizeUnit,
      helper.settings.kPaperSizeInches,
      "Check paperSizeUnit"
    );
    is(helper.viewSettings.paperWidth.toFixed(1), "11.7", "Check paperWidth");
    is(helper.viewSettings.paperHeight.toFixed(1), "16.5", "Check paperHeight");

    await SpecialPowers.popPrefEnv();
    await helper.closeDialog();
  });
});

add_task(async function testMismatchedPaperSizeUnitCorrection() {
  await PrintHelper.withTestPage(async helper => {
    // Set prefs to select a non-default paper size
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.printer_Mozilla_Save_to_PDF.print_paper_id", "na_ledger"],
        // paperSizeUnit is millimeters, but the dimensions are correct for inches
        ["print.printer_Mozilla_Save_to_PDF.print_paper_size_unit", 1],
        ["print.printer_Mozilla_Save_to_PDF.print_paper_width", "11.0"],
        ["print.printer_Mozilla_Save_to_PDF.print_paper_height", "17.0"],
      ],
    });
    await helper.startPrint();

    let paperSelect = helper.get("paper-size-picker");
    is(paperSelect.value, "na_ledger", "The expected paper size is selected");

    // We expect to honor the paperSizeUnit, and convert paperWidth/Height to that unit
    is(
      helper.viewSettings.paperId,
      "na_ledger",
      "The settings have the expected paperId"
    );
    is(
      helper.viewSettings.paperSizeUnit,
      helper.settings.kPaperSizeMillimeters,
      "Check paperSizeUnit"
    );
    is(helper.viewSettings.paperWidth.toFixed(1), "279.4", "Check paperWidth");
    is(
      helper.viewSettings.paperHeight.toFixed(1),
      "431.8",
      "Check paperHeight"
    );

    await selectPaperOptionWithValue(helper, "iso_a3");
    is(paperSelect.value, "iso_a3", "The expected paper size is selected");
    is(
      helper.viewSettings.paperId,
      "iso_a3",
      "The settings have the expected paperId"
    );
    is(
      helper.viewSettings.paperSizeUnit,
      helper.settings.kPaperSizeMillimeters,
      "Check paperSizeUnit"
    );
    is(helper.viewSettings.paperWidth.toFixed(1), "297.0", "Check paperWidth");
    is(
      helper.viewSettings.paperHeight.toFixed(1),
      "420.0",
      "Check paperHeight"
    );

    await SpecialPowers.popPrefEnv();
    await helper.closeDialog();
  });
});
