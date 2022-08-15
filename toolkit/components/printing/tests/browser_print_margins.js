/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Chaos mode slowdown causes intermittent failures - See bug 1698240.
requestLongerTimeout(2);

async function changeMargin(helper, scroll, value) {
  let marginSelect = helper.get("margins-picker");

  info("  current value is " + marginSelect.value);

  marginSelect.focus();

  if (scroll) {
    marginSelect.scrollIntoView({ block: "center" });
  }

  marginSelect.value = value;
  marginSelect.dispatchEvent(
    new marginSelect.ownerGlobal.Event("input", {
      bubbles: true,
      composed: true,
    })
  );
  marginSelect.dispatchEvent(
    new marginSelect.ownerGlobal.Event("change", {
      bubbles: true,
    })
  );
}

function changeDefaultToCustom(helper) {
  info("Trying to change margin from default -> custom");
  return changeMargin(helper, true, "custom");
}

function changeCustomToDefault(helper) {
  info("Trying to change margin from custom -> default");
  return changeMargin(helper, false, "default");
}

function changeCustomToNone(helper) {
  info("Trying to change margin from custom -> none");
  return changeMargin(helper, false, "none");
}

function assertPendingMarginsUpdate(helper) {
  ok(
    Object.keys(helper.win.PrintEventHandler._delayedChanges).length,
    "At least one delayed task is added"
  );
  ok(
    helper.win.PrintEventHandler._delayedSettingsChangeTask.isArmed,
    "The update task is armed"
  );
}

function assertNoPendingMarginsUpdate(helper) {
  ok(
    !helper.win.PrintEventHandler._delayedSettingsChangeTask.isArmed,
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

add_task(async function testCustomMarginMaxAttrsSet() {
  await PrintHelper.withTestPage(async helper => {
    let paperList = [
      PrintHelper.createMockPaper({
        id: "unwriteableMargins",
        name: "Unwriteable Margins",
        // Numbers here demonstrate our truncating logic doesn't round up
        unwriteableMargin: {
          top: 18,
          bottom: 19,
          left: 18,
          right: 19,
          QueryInterface: ChromeUtils.generateQI([Ci.nsIPaperMargin]),
        },
      }),
    ];

    let mockPrinterName = "Mock printer";
    helper.addMockPrinter({ name: mockPrinterName, paperList });
    Services.prefs.setStringPref("print_printer", mockPrinterName);

    await helper.startPrint();
    await helper.openMoreSettings();
    await changeDefaultToCustom(helper);

    let marginsSelect = helper.get("margins-select");
    is(
      marginsSelect._maxHeight.toFixed(2),
      "10.49",
      "Max height would round up"
    );
    is(marginsSelect._maxWidth.toFixed(2), "7.99", "Max width would round up");
    helper.assertSettingsMatch({
      marginTop: 0.5,
      marginRight: 0.5,
      marginBottom: 0.5,
      marginLeft: 0.5,
    });
    is(
      helper.get("custom-margin-left").max,
      "7.48",
      "Left margin max attr is correct"
    );
    is(
      helper.get("custom-margin-right").max,
      "7.48",
      "Right margin max attr is correct"
    );
    is(
      helper.get("custom-margin-top").max,
      "9.98",
      "Top margin max attr is correct"
    );
    is(
      helper.get("custom-margin-bottom").max,
      "9.98",
      "Bottom margin max attr is correct"
    );
    await helper.closeDialog();
  });
});

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
        helper.assertSettingsMatch({ honorPageRuleMargins: true });

        await changeDefaultToCustom(helper);

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
    await changeDefaultToCustom(helper);

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
    await changeDefaultToCustom(helper);

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
    await changeDefaultToCustom(helper);
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

    await changeCustomToDefault(helper);
    assertNoPendingMarginsUpdate(helper);
    await BrowserTestUtils.waitForCondition(
      () => marginError.hidden,
      "Wait for margin error to be hidden"
    );
    await changeDefaultToCustom(helper);
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
    await changeDefaultToCustom(helper);
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
    await changeDefaultToCustom(helper);
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
        await changeDefaultToCustom(helper);
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
        await changeCustomToDefault(helper);
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
        await changeDefaultToCustom(helper);
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
        await changeCustomToDefault(helper);
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
    await changeDefaultToCustom(helper);

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
    await changeDefaultToCustom(helper);
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
        await changeCustomToNone(helper);
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
    await changeDefaultToCustom(helper);
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

    helper.dispatchSettingsChange({ printerName: mockPrinterName });
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

add_task(async function testRevalidateCustomMarginsAfterPaperChanges() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    helper.dispatchSettingsChange({ paperId: "iso_a3" });
    await helper.openMoreSettings();
    await changeDefaultToCustom(helper);
    await helper.awaitAnimationFrame();
    let marginError = helper.get("error-invalid-margin");

    await helper.assertSettingsChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      { marginTop: 5, marginRight: 5, marginBottom: 5, marginLeft: 5 },
      async () => {
        await helper.text(helper.get("custom-margin-top"), "5");
        await helper.text(helper.get("custom-margin-bottom"), "5");
        await helper.text(helper.get("custom-margin-right"), "5");
        await helper.text(helper.get("custom-margin-left"), "5");
        assertPendingMarginsUpdate(helper);

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
        ok(marginError.hidden, "Margin error is hidden");
      }
    );

    await helper.assertSettingsChanged(
      { marginTop: 5, marginRight: 5, marginBottom: 5, marginLeft: 5 },
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      async () => {
        helper.dispatchSettingsChange({ paperId: "iso_a5" });

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
        ok(marginError.hidden, "Margin error is hidden");
      }
    );
  });
});

add_task(async function testRevalidateCustomMarginsAfterOrientationChanges() {
  await PrintHelper.withTestPage(async helper => {
    await setupLetterPaper();
    await helper.startPrint();
    await helper.openMoreSettings();
    await changeDefaultToCustom(helper);
    await helper.awaitAnimationFrame();
    let marginError = helper.get("error-invalid-margin");

    await helper.assertSettingsChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      { marginTop: 5, marginRight: 0.5, marginBottom: 5, marginLeft: 0.5 },
      async () => {
        await helper.text(helper.get("custom-margin-top"), "5");
        await helper.text(helper.get("custom-margin-bottom"), "5");
        assertPendingMarginsUpdate(helper);

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
        ok(marginError.hidden, "Margin error is hidden");
      }
    );

    await helper.assertSettingsChanged(
      { marginTop: 5, marginRight: 0.5, marginBottom: 5, marginLeft: 0.5 },
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      async () => {
        helper.dispatchSettingsChange({ orientation: 1 });
        await helper.waitForSettingsEvent();
        ok(marginError.hidden, "Margin error is hidden");
      }
    );
  });
});

add_task(async function testResetMarginPersists() {
  await PrintHelper.withTestPage(async helper => {
    await setupLetterPaper();
    await helper.startPrint();

    await helper.openMoreSettings();
    await changeDefaultToCustom(helper);
    await helper.awaitAnimationFrame();
    let marginError = helper.get("error-invalid-margin");

    await helper.assertSettingsChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      { marginTop: 0.5, marginRight: 4, marginBottom: 0.5, marginLeft: 4.5 },
      async () => {
        await helper.text(helper.get("custom-margin-right"), "4");
        await helper.text(helper.get("custom-margin-left"), "4.5");

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
        ok(marginError.hidden, "Margin error is hidden");
      }
    );

    await helper.assertSettingsChanged(
      { marginTop: 0.5, marginRight: 4, marginBottom: 0.5, marginLeft: 4.5 },
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      async () => {
        helper.dispatchSettingsChange({ paperId: "iso_a4" });

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
        ok(marginError.hidden, "Margin error is hidden");
      }
    );

    await helper.assertSettingsNotChanged(
      { marginTop: 0.5, marginRight: 0.5, marginBottom: 0.5, marginLeft: 0.5 },
      async () => {
        helper.dispatchSettingsChange({ paperId: "iso_a5" });
        await helper.waitForSettingsEvent();
        ok(marginError.hidden, "Margin error is hidden");
      }
    );

    await helper.closeDialog();
  });
});

add_task(async function testCustomMarginUnits() {
  const mockPrinterName = "MetricPrinter";
  await PrintHelper.withTestPage(async helper => {
    // Add a metric-unit printer we can test with
    helper.addMockPrinter({
      name: mockPrinterName,
      paperSizeUnit: Ci.nsIPrintSettings.kPaperSizeMillimeters,
      paperList: [],
    });

    // settings are saved in inches
    const persistedMargins = {
      top: 0.5,
      right: 5,
      bottom: 0.5,
      left: 1,
    };
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "print.printer_Mozilla_Save_to_PDF.print_margin_right",
          persistedMargins.right.toString(),
        ],
        [
          "print.printer_Mozilla_Save_to_PDF.print_margin_left",
          persistedMargins.left.toString(),
        ],
        [
          "print.printer_Mozilla_Save_to_PDF.print_margin_top",
          persistedMargins.top.toString(),
        ],
        [
          "print.printer_Mozilla_Save_to_PDF.print_margin_bottom",
          persistedMargins.bottom.toString(),
        ],
      ],
    });
    await helper.startPrint();
    await helper.openMoreSettings();

    helper.assertSettingsMatch({
      paperId: "na_letter",
      marginTop: persistedMargins.top,
      marginRight: persistedMargins.right,
      marginBottom: persistedMargins.bottom,
      marginLeft: persistedMargins.left,
    });

    is(
      helper.settings.printerName,
      DEFAULT_PRINTER_NAME,
      "The PDF (inch-unit) printer is current"
    );

    is(
      helper.get("margins-picker").value,
      "custom",
      "The margins picker has the expected value"
    );
    is(
      helper.get("margins-picker").selectedOptions[0].dataset.l10nId,
      "printui-margins-custom-inches",
      "The custom margins option has correct unit string id"
    );
    // the unit value should be correct for inches
    for (let edgeName of Object.keys(persistedMargins)) {
      is(
        helper.get(`custom-margin-${edgeName}`).value,
        persistedMargins[edgeName].toFixed(2),
        `Has the expected unit-converted ${edgeName}-margin value`
      );
    }

    await helper.assertSettingsChanged(
      { marginTop: persistedMargins.top },
      { marginTop: 1 },
      async () => {
        // update the top margin to 1"
        await helper.text(helper.get("custom-margin-top"), "1");
        assertPendingMarginsUpdate(helper);

        // Wait for the preview to update, the margin options delay updates by
        // INPUT_DELAY_MS, which is 500ms.
        await helper.waitForSettingsEvent();
        // ensure any round-trip correctly re-converts the setting value back to the displayed mm value
        is(
          helper.get("custom-margin-top").value,
          "1",
          "Converted custom margin value is expected value"
        );
      }
    );
    // put it back to how it was
    await helper.text(
      helper.get("custom-margin-top"),
      persistedMargins.top.toString()
    );
    await helper.waitForSettingsEvent();

    // Now switch to the metric printer
    await helper.dispatchSettingsChange({ printerName: mockPrinterName });
    await helper.waitForSettingsEvent();

    is(
      helper.settings.printerName,
      mockPrinterName,
      "The metric printer is current"
    );
    is(
      helper.get("margins-picker").value,
      "custom",
      "The margins picker has the expected value"
    );
    is(
      helper.get("margins-picker").selectedOptions[0].dataset.l10nId,
      "printui-margins-custom-mm",
      "The custom margins option has correct unit string id"
    );
    // the unit value should be correct for mm
    for (let edgeName of Object.keys(persistedMargins)) {
      is(
        helper.get(`custom-margin-${edgeName}`).value,
        (persistedMargins[edgeName] * 25.4).toFixed(2),
        `Has the expected unit-converted ${edgeName}-margin value`
      );
    }

    await helper.assertSettingsChanged(
      { marginTop: persistedMargins.top },
      { marginTop: 1 },
      async () => {
        let marginError = helper.get("error-invalid-margin");
        ok(marginError.hidden, "Margin error is hidden");

        // update the top margin to 1" in mm
        await helper.text(helper.get("custom-margin-top"), "25.4");
        // Check the constraints validation is using the right max
        // as 25" top margin would be an error, but 25mm is ok
        ok(marginError.hidden, "Margin error is hidden");

        assertPendingMarginsUpdate(helper);
        // Wait for the preview to update, the margin options delay updates by INPUT_DELAY_MS
        await helper.waitForSettingsEvent();
        // ensure any round-trip correctly re-converts the setting value back to the displayed mm value
        is(
          helper.get("custom-margin-top").value,
          "25.4",
          "Converted custom margin value is expected value"
        );
      }
    );

    // check margin validation is actually working with unit-appropriate max
    await helper.assertSettingsNotChanged({ marginTop: 1 }, async () => {
      let marginError = helper.get("error-invalid-margin");
      ok(marginError.hidden, "Margin error is hidden");

      await helper.text(helper.get("custom-margin-top"), "300");
      await BrowserTestUtils.waitForAttributeRemoval("hidden", marginError);

      ok(!marginError.hidden, "Margin error is showing");
      assertNoPendingMarginsUpdate(helper);
    });

    await SpecialPowers.popPrefEnv();
    await helper.closeDialog();
  });
});

add_task(async function testHonorPageRuleMargins() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();
    let marginsPicker = helper.get("margins-picker");

    is(marginsPicker.value, "default", "Started with default margins");
    helper.assertSettingsMatch({ honorPageRuleMargins: true });

    await helper.waitForSettingsEvent(() => changeDefaultToCustom(helper));

    is(marginsPicker.value, "custom", "Changed to custom margins");
    helper.assertSettingsMatch({ honorPageRuleMargins: false });

    await helper.waitForSettingsEvent(() => changeCustomToNone(helper));

    is(marginsPicker.value, "none", "Changed to no margins");
    helper.assertSettingsMatch({ honorPageRuleMargins: false });

    await helper.waitForSettingsEvent(() => changeCustomToDefault(helper));

    is(marginsPicker.value, "default", "Back to default margins");
    helper.assertSettingsMatch({ honorPageRuleMargins: true });
  });
});

add_task(async function testIgnoreUnwriteableMargins() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();
    let marginsPicker = helper.get("margins-picker");

    is(marginsPicker.value, "default", "Started with default margins");
    helper.assertSettingsMatch({ ignoreUnwriteableMargins: false });

    await helper.waitForSettingsEvent(() => changeDefaultToCustom(helper));

    is(marginsPicker.value, "custom", "Changed to custom margins");
    helper.assertSettingsMatch({ ignoreUnwriteableMargins: false });

    await helper.waitForSettingsEvent(() => changeCustomToNone(helper));

    is(marginsPicker.value, "none", "Changed to no margins");
    helper.assertSettingsMatch({ ignoreUnwriteableMargins: true });

    await helper.waitForSettingsEvent(() => changeCustomToDefault(helper));

    is(marginsPicker.value, "default", "Back to default margins");
    helper.assertSettingsMatch({ ignoreUnwriteableMargins: false });
  });
});

add_task(async function testCustomMarginZeroRespectsUnwriteableMargins() {
  await PrintHelper.withTestPage(async helper => {
    await helper.startPrint();
    await helper.openMoreSettings();
    let marginsPicker = helper.get("margins-picker");

    await helper.waitForSettingsEvent(() => changeDefaultToCustom(helper));
    is(marginsPicker.value, "custom", "Changed to custom margins");

    await helper.text(helper.get("custom-margin-top"), "0");
    await helper.text(helper.get("custom-margin-right"), "0");
    await helper.text(helper.get("custom-margin-bottom"), "0");
    await helper.text(helper.get("custom-margin-left"), "0");

    assertPendingMarginsUpdate(helper);
    await helper.waitForSettingsEvent();
    helper.assertSettingsMatch({ ignoreUnwriteableMargins: false });
  });
});

add_task(async function testDefaultMarginsInvalidStartup() {
  await PrintHelper.withTestPage(async helper => {
    let paperList = [
      PrintHelper.createMockPaper({
        id: "smallestPaper",
        name: "Default Margins Invalid",
        width: 50,
        height: 50,
        unwriteableMargin: {
          top: 10,
          bottom: 10,
          left: 10,
          right: 10,
          QueryInterface: ChromeUtils.generateQI([Ci.nsIPaperMargin]),
        },
      }),
    ];

    let mockPrinterName = "Mock printer";
    helper.addMockPrinter({ name: mockPrinterName, paperList });
    Services.prefs.setStringPref("print_printer", mockPrinterName);

    await helper.startPrint();

    helper.assertSettingsMatch({
      marginTop: 0,
      marginRight: 0,
      marginBottom: 0,
      marginLeft: 0,
    });

    let marginSelect = helper.get("margins-picker");
    is(marginSelect.value, "none", "Margins picker set to 'None'");

    let printForm = helper.get("print");
    ok(printForm.checkValidity(), "The print form is valid");

    await helper.closeDialog();
  });
});
