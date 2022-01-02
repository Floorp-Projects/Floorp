/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testCopyError() {
  await PrintHelper.withTestPage(async helper => {
    helper.addMockPrinter("A printer");
    await SpecialPowers.pushPrefEnv({
      set: [["print_printer", "A printer"]],
    });

    await helper.startPrint();

    let copyInput = helper.get("copies-count");
    let destinationPicker = helper.get("printer-picker");
    let copyError = helper.get("error-invalid-copies");

    await helper.assertSettingsChanged(
      { numCopies: 1 },
      { numCopies: 10000 },
      async () => {
        await helper.waitForSettingsEvent(() => {
          helper.text(copyInput, "10000");
        });

        is(copyError.hidden, true, "Copy error is hidden");
        EventUtils.sendChar("0", helper.win);

        // Initially, the copies will be more than the max.
        is(copyInput.checkValidity(), false, "Copy count is invalid");
        await BrowserTestUtils.waitForAttributeRemoval("hidden", copyError);
        is(copyError.hidden, false, "Copy error is showing");
        is(
          destinationPicker.disabled,
          false,
          "Destination picker is still enabled"
        );

        helper.text(copyInput, "10000");
        await helper.waitForSettingsEvent();
        is(copyInput.value, "10000", "Copies gets set to max value");
        is(copyInput.checkValidity(), true, "Copy count is valid again");
        await BrowserTestUtils.waitForCondition(
          () => copyError.hidden,
          "Wait for copy error to be hidden"
        );
      }
    );

    await helper.closeDialog();
  });
});
