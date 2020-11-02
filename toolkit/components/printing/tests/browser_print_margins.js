/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function changeDefaultToCustom(helper) {
  let marginSelect = helper.get("margins-picker");
  marginSelect.focus();
  marginSelect.scrollIntoView({ block: "center" });
  EventUtils.sendKey("space", helper.win);
  EventUtils.sendKey("down", helper.win);
  EventUtils.sendKey("down", helper.win);
  EventUtils.sendKey("down", helper.win);
  EventUtils.sendKey("return", helper.win);
}

function changeCustomToDefault(helper) {
  let marginSelect = helper.get("margins-picker");
  marginSelect.focus();
  EventUtils.sendKey("space", helper.win);
  EventUtils.sendKey("up", helper.win);
  EventUtils.sendKey("up", helper.win);
  EventUtils.sendKey("up", helper.win);
  EventUtils.sendKey("return", helper.win);
}

function changeCustomToNone(helper) {
  let marginSelect = helper.get("margins-picker");
  marginSelect.focus();
  EventUtils.sendKey("space", helper.win);
  EventUtils.sendKey("up", helper.win);
  EventUtils.sendKey("return", helper.win);
}

function assertPendingMarginsUpdate(helper) {
  let marginsPicker = helper.get("margins-select");
  ok(
    marginsPicker._updateCustomMarginsTask.isArmed,
    "The update task is armed"
  );
}

function assertNoPendingMarginsUpdate(helper) {
  let marginsPicker = helper.get("margins-select");
  ok(
    !marginsPicker._updateCustomMarginsTask.isArmed,
    "The update task isn't armed"
  );
}

async function setupLetterPaper() {
  const INCHES_PER_POINT = 1 / 72;
  const printerList = Cc["@mozilla.org/gfx/printerlist;1"].createInstance(
    Ci.nsIPrinterList
  );
  let fallbackPaperList = await printerList.fallbackPaperList;
  let paper = fallbackPaperList.find(
    paper =>
      paper.width * INCHES_PER_POINT == 8.5 &&
      paper.height * INCHES_PER_POINT == 11
  );
  ok(paper, "Found a paper");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["print.printer_Mozilla_Save_to_PDF.print_paper_id", paper.id.toString()],
      ["print.printer_Mozilla_Save_to_PDF.print_paper_size_unit", 0],
      [
        "print.printer_Mozilla_Save_to_PDF.print_paper_width",
        (paper.width * INCHES_PER_POINT).toString(),
      ],
      [
        "print.printer_Mozilla_Save_to_PDF.print_paper_height",
        (paper.height * INCHES_PER_POINT).toString(),
      ],
    ],
  });
}

add_task(async function testPresetMargins() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();

    await helper.assertSettingsChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      { marginTop: 0.25, marginRight: 1, marginBottom: 2, marginLeft: 0.75 },
      async () => {
        let marginSelect = helper.get("margins-picker");
        let customMargins = helper.get("custom-margins");

        ok(customMargins.hidden, "Custom margins are hidden");
        is(marginSelect.value, "default", "Default margins set");

        this.changeDefaultToCustom(helper);

        is(marginSelect.value, "custom", "Custom margins are now set");
        ok(!customMargins.hidden, "Custom margins are present");
        // Check that values are initialized to correct values
        is(
          helper.get("custom-margin-top").value,
          "0.50",
          "Top margin placeholder is correct"
        );
        is(
          helper.get("custom-margin-right").value,
          "0.50",
          "Right margin placeholder is correct"
        );
        is(
          helper.get("custom-margin-bottom").value,
          "0.50",
          "Bottom margin placeholder is correct"
        );
        is(
          helper.get("custom-margin-left").value,
          "0.50",
          "Left margin placeholder is correct"
        );

        await helper.awaitAnimationFrame();

        await helper.text(helper.get("custom-margin-top"), "0.25");
        await helper.text(helper.get("custom-margin-right"), "1");
        await helper.text(helper.get("custom-margin-bottom"), "2");
        await helper.text(helper.get("custom-margin-left"), "0.75");

        assertPendingMarginsUpdate(helper);

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
      }
    );
    await helper.closeDialog();
  });
});

add_task(async function testHeightError() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();
    this.changeDefaultToCustom(helper);

    await helper.assertSettingsNotChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      async () => {
        let marginError = helper.get("error-invalid-margin");
        ok(marginError.hidden, "Margin error is hidden");

        await helper.text(helper.get("custom-margin-top"), "20");
        await BrowserTestUtils.waitForAttributeRemoval("hidden", marginError);

        ok(!marginError.hidden, "Margin error is showing");
        assertNoPendingMarginsUpdate(helper);
      }
    );
    await helper.closeDialog();
  });
});

add_task(async function testWidthError() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();
    this.changeDefaultToCustom(helper);

    await helper.assertSettingsNotChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      async () => {
        let marginError = helper.get("error-invalid-margin");
        ok(marginError.hidden, "Margin error is hidden");

        await helper.text(helper.get("custom-margin-right"), "20");
        await BrowserTestUtils.waitForAttributeRemoval("hidden", marginError);

        ok(!marginError.hidden, "Margin error is showing");
        assertNoPendingMarginsUpdate(helper);
      }
    );
    await helper.closeDialog();
  });
});

add_task(async function testInvalidMarginsReset() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();
    this.changeDefaultToCustom(helper);
    let marginError = helper.get("error-invalid-margin");

    await helper.assertSettingsNotChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      async () => {
        ok(marginError.hidden, "Margin error is hidden");

        await helper.awaitAnimationFrame();
        await helper.text(helper.get("custom-margin-top"), "20");
        await helper.text(helper.get("custom-margin-right"), "20");
        assertNoPendingMarginsUpdate(helper);
        await BrowserTestUtils.waitForAttributeRemoval("hidden", marginError);
      }
    );

    this.changeCustomToDefault(helper);
    assertNoPendingMarginsUpdate(helper);
    await BrowserTestUtils.waitForCondition(
      () => marginError.hidden,
      "Wait for margin error to be hidden"
    );
    this.changeDefaultToCustom(helper);
    helper.assertSettingsMatch({
      marginTop: 0.5,
      marginRight: 0.5,
      marginBottom: 0.5,
      marginLeft: 0.5,
    });

    is(
      helper.get("margins-picker").value,
      "custom",
      "The custom option is selected"
    );
    is(
      helper.get("custom-margin-top").value,
      "0.50",
      "Top margin placeholder is correct"
    );
    is(
      helper.get("custom-margin-right").value,
      "0.50",
      "Right margin placeholder is correct"
    );
    is(
      helper.get("custom-margin-bottom").value,
      "0.50",
      "Bottom margin placeholder is correct"
    );
    is(
      helper.get("custom-margin-left").value,
      "0.50",
      "Left margin placeholder is correct"
    );
    assertNoPendingMarginsUpdate(helper);
    await BrowserTestUtils.waitForCondition(
      () => marginError.hidden,
      "Wait for margin error to be hidden"
    );
    await helper.closeDialog();
  });
});

add_task(async function testChangeInvalidToValidUpdate() {
  await PrintHelper.withTestPage(async helper => {
    await setupLetterPaper();
    await helper.startPrint();
    await helper.openMoreSettings();
    this.changeDefaultToCustom(helper);
    await helper.awaitAnimationFrame();
    let marginError = helper.get("error-invalid-margin");

    await helper.text(helper.get("custom-margin-bottom"), "11");
    assertNoPendingMarginsUpdate(helper);
    await BrowserTestUtils.waitForAttributeRemoval("hidden", marginError);
    ok(!marginError.hidden, "Margin error is showing");
    helper.assertSettingsMatch({
      marginTop: 0.5,
      marginRight: 0.5,
      marginBottom: 0.5,
      marginLeft: 0.5,
      paperId: "na_letter",
    });

    await helper.text(helper.get("custom-margin-top"), "1");
    assertNoPendingMarginsUpdate(helper);
    helper.assertSettingsMatch({
      marginTop: 0.5,
      marginRight: 0.5,
      marginBottom: 0.5,
      marginLeft: 0.5,
    });
    ok(!marginError.hidden, "Margin error is showing");

    await helper.assertSettingsChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      { marginTop: 1, marginRight: 0.5, marginBottom: 1, marginLeft: 0.5 },
      async () => {
        await helper.text(helper.get("custom-margin-bottom"), "1");

        assertPendingMarginsUpdate(helper);

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
        ok(marginError.hidden, "Margin error is hidden");
      }
    );
  });
});

add_task(async function testChangeInvalidCanRevalidate() {
  await PrintHelper.withTestPage(async helper => {
    await setupLetterPaper();
    await helper.startPrint();
    await helper.openMoreSettings();
    this.changeDefaultToCustom(helper);
    await helper.awaitAnimationFrame();
    let marginError = helper.get("error-invalid-margin");

    await helper.assertSettingsChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      { marginTop: 5, marginRight: 0.5, marginBottom: 3, marginLeft: 0.5 },
      async () => {
        await helper.text(helper.get("custom-margin-top"), "5");
        await helper.text(helper.get("custom-margin-bottom"), "3");
        assertPendingMarginsUpdate(helper);

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
        ok(marginError.hidden, "Margin error is hidden");
      }
    );

    await helper.text(helper.get("custom-margin-top"), "9");
    assertNoPendingMarginsUpdate(helper);
    await BrowserTestUtils.waitForAttributeRemoval("hidden", marginError);
    ok(!marginError.hidden, "Margin error is showing");
    helper.assertSettingsMatch({
      marginTop: 5,
      marginRight: 0.5,
      marginBottom: 3,
      marginLeft: 0.5,
      paperId: "na_letter",
    });

    await helper.assertSettingsChanged(
      { marginTop: 5, marginRight: 0.5, marginBottom: 3, marginLeft: 0.5 },
      { marginTop: 9, marginRight: 0.5, marginBottom: 2, marginLeft: 0.5 },
      async () => {
        await helper.text(helper.get("custom-margin-bottom"), "2");
        assertPendingMarginsUpdate(helper);

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
        ok(marginError.hidden, "Margin error is hidden");
      }
    );
  });
});

add_task(async function testCustomMarginsPersist() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();

    await helper.assertSettingsChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      { marginTop: 0.25, marginRight: 1, marginBottom: 2, marginLeft: 0 },
      async () => {
        this.changeDefaultToCustom(helper);
        await helper.awaitAnimationFrame();

        await helper.text(helper.get("custom-margin-top"), "0.25");
        await helper.text(helper.get("custom-margin-right"), "1");
        await helper.text(helper.get("custom-margin-bottom"), "2");
        await helper.text(helper.get("custom-margin-left"), "0");

        assertPendingMarginsUpdate(helper);

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
      }
    );

    await helper.closeDialog();

    await helper.startPrint();
    await helper.openMoreSettings();

    helper.assertSettingsMatch({
      marginTop: 0.25,
      marginRight: 1,
      marginBottom: 2,
      marginLeft: 0,
    });

    is(
      helper.get("margins-picker").value,
      "custom",
      "The custom option is selected"
    );
    is(
      helper.get("custom-margin-top").value,
      "0.25",
      "Top margin placeholder is correct"
    );
    is(
      helper.get("custom-margin-right").value,
      "1.00",
      "Right margin placeholder is correct"
    );
    is(
      helper.get("custom-margin-bottom").value,
      "2.00",
      "Bottom margin placeholder is correct"
    );
    is(
      helper.get("custom-margin-left").value,
      "0.00",
      "Left margin placeholder is correct"
    );
    await helper.assertSettingsChanged(
      { marginTop: 0.25, marginRight: 1, marginBottom: 2, marginLeft: 0 },
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      async () => {
        await helper.awaitAnimationFrame();

        await helper.text(helper.get("custom-margin-top"), "0.50");
        await helper.text(helper.get("custom-margin-right"), "0.50");
        await helper.text(helper.get("custom-margin-bottom"), "0.50");
        await helper.text(helper.get("custom-margin-left"), "0.50");

        assertPendingMarginsUpdate(helper);

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
      }
    );
    await helper.closeDialog();
  });
});

add_task(async function testChangingBetweenMargins() {
  await PrintHelper.withTestPage(async helper => {
    await SpecialPowers.pushPrefEnv({
      set: [["print.printer_Mozilla_Save_to_PDF.print_margin_left", "1"]],
    });

    await helper.startPrint();
    await helper.openMoreSettings();

    let marginsPicker = helper.get("margins-picker");
    is(marginsPicker.value, "custom", "First margin is custom");

    helper.assertSettingsMatch({
      marginTop: 0.5,
      marginBottom: 0.5,
      marginLeft: 1,
      marginRight: 0.5,
    });

    info("Switch to Default margins");
    await helper.assertSettingsChanged(
      { marginLeft: 1 },
      { marginLeft: 0.5 },
      async () => {
        let settingsChanged = helper.waitForSettingsEvent();
        changeCustomToDefault(helper);
        await settingsChanged;
      }
    );

    is(marginsPicker.value, "default", "Default preset selected");

    info("Switching back to Custom, should restore old margins");
    await helper.assertSettingsChanged(
      { marginLeft: 0.5 },
      { marginLeft: 1 },
      async () => {
        let settingsChanged = helper.waitForSettingsEvent();
        changeDefaultToCustom(helper);
        await settingsChanged;
      }
    );

    is(marginsPicker.value, "custom", "Custom is now selected");

    info("Switching back to Default, should restore 0.5");
    await helper.assertSettingsChanged(
      { marginLeft: 1 },
      { marginLeft: 0.5 },
      async () => {
        let settingsChanged = helper.waitForSettingsEvent();
        changeCustomToDefault(helper);
        await settingsChanged;
      }
    );

    is(marginsPicker.value, "default", "Default preset is selected again");
  });
});

add_task(async function testChangeHonoredInPrint() {
  const mockPrinterName = "Fake Printer";
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter(mockPrinterName);
    await helper.startPrint();
    await helper.setupMockPrint();

    helper.mockFilePicker("changedMargin.pdf");

    await helper.openMoreSettings();
    helper.assertSettingsMatch({ marginRight: 0.5 });
    this.changeDefaultToCustom(helper);

    await helper.withClosingFn(async () => {
      await helper.text(helper.get("custom-margin-right"), "1");
      EventUtils.sendKey("return", helper.win);
      helper.resolvePrint();
    });
    helper.assertPrintedWithSettings({ marginRight: 1 });
  });
});

add_task(async function testInvalidPrefValueHeight() {
  await PrintHelper.withTestPage(async helper => {
    // Set some bad prefs
    await SpecialPowers.pushPrefEnv({
      set: [["print.printer_Mozilla_Save_to_PDF.print_margin_top", "-1"]],
    });
    await helper.startPrint();
    helper.assertSettingsMatch({
      marginTop: 0.5,
      marginRight: 0.5,
      marginBottom: 0.5,
      marginLeft: 0.5,
    });
    await helper.closeDialog();
  });
});

add_task(async function testInvalidPrefValueWidth() {
  await PrintHelper.withTestPage(async helper => {
    // Set some bad prefs
    await SpecialPowers.pushPrefEnv({
      set: [["print.printer_Mozilla_Save_to_PDF.print_margin_left", "-1"]],
    });
    await helper.startPrint();
    helper.assertSettingsMatch({
      marginTop: 0.5,
      marginRight: 0.5,
      marginBottom: 0.5,
      marginLeft: 0.5,
    });
    await helper.closeDialog();
  });
});

add_task(async function testInvalidMarginStartup() {
  await PrintHelper.withTestPage(async helper => {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.printer_Mozilla_Save_to_PDF.print_margin_right", "4"],
        ["print.printer_Mozilla_Save_to_PDF.print_margin_left", "5"],
      ],
    });
    await setupLetterPaper();
    await helper.startPrint();
    helper.assertSettingsMatch({
      paperId: "na_letter",
      marginLeft: 0.5,
      marginRight: 0.5,
    });
    helper.closeDialog();
  });
});

add_task(async function testRevalidateSwitchToNone() {
  await PrintHelper.withTestPage(async helper => {
    await setupLetterPaper();
    await helper.startPrint();
    await helper.openMoreSettings();
    this.changeDefaultToCustom(helper);
    await helper.awaitAnimationFrame();

    await helper.text(helper.get("custom-margin-bottom"), "6");
    await helper.text(helper.get("custom-margin-top"), "6");
    assertNoPendingMarginsUpdate(helper);
    helper.assertSettingsMatch({
      marginTop: 0.5,
      marginRight: 0.5,
      marginBottom: 0.5,
      marginLeft: 0.5,
      paperId: "na_letter",
    });

    await helper.assertSettingsChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      { marginTop: 6, marginRight: 0.5, marginBottom: 3, marginLeft: 0.5 },
      async () => {
        await helper.text(helper.get("custom-margin-bottom"), "3");
        assertPendingMarginsUpdate(helper);

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
      }
    );

    await helper.assertSettingsChanged(
      { marginTop: 6, marginRight: 0.5, marginBottom: 3, marginLeft: 0.5 },
      { marginTop: 0, marginRight: 0, marginBottom: 0, marginLeft: 0 },
      async () => {
        this.changeCustomToNone(helper);
        is(
          helper.get("margins-picker").value,
          "none",
          "No margins are now set"
        );

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
      }
    );
  });
});

add_task(async function testInvalidMarginResetAfterDestinationChange() {
  const mockPrinterName = "Fake Printer";
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter(mockPrinterName);
    await SpecialPowers.pushPrefEnv({
      set: [
        ["print.printer_Fake_Printer.print_paper_id", "na_letter"],
        ["print.printer_Fake_Printer.print_paper_size_unit", 0],
        ["print.printer_Fake_Printer.print_paper_width", "8.5"],
        ["print.printer_Fake_Printer.print_paper_height", "11"],
      ],
    });
    await helper.startPrint();

    let destinationPicker = helper.get("printer-picker");

    await helper.openMoreSettings();
    changeDefaultToCustom(helper);
    await helper.awaitAnimationFrame();

    let marginError = helper.get("error-invalid-margin");

    await helper.assertSettingsNotChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      async () => {
        ok(marginError.hidden, "Margin error is hidden");

        await helper.text(helper.get("custom-margin-top"), "20");
        await BrowserTestUtils.waitForAttributeRemoval("hidden", marginError);

        ok(!marginError.hidden, "Margin error is showing");
        assertNoPendingMarginsUpdate(helper);
      }
    );

    is(destinationPicker.disabled, false, "Destination picker is enabled");

    await helper.dispatchSettingsChange({ printerName: mockPrinterName });
    await BrowserTestUtils.waitForCondition(
      () => marginError.hidden,
      "Wait for margin error to be hidden"
    );

    helper.assertSettingsMatch({
      marginTop: 0.5,
      marginRight: 0.5,
      marginBottom: 0.5,
      marginLeft: 0.5,
    });

    await helper.closeDialog();
  });
});
