/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function changeDefaultToCustom(helper) {
  let marginSelect = helper.get("margins-picker");
  marginSelect.focus();
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

function assertPendingMarginsUpdate(helper) {
  let marginsPicker = helper.get("margins-select");
  ok(marginsPicker._updateMarginTask.isArmed, "The update task is armed");
}

function assertNoPendingMarginsUpdate(helper) {
  let marginsPicker = helper.get("margins-select");
  ok(!marginsPicker._updateMarginTask.isArmed, "The update task isn't armed");
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
    await helper.assertSettingsMatch({
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

    await helper.assertSettingsMatch({
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
