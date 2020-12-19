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

    await helper.assertSettingsChanged(
      { numCopies: 1 },
      { numCopies: 10000 },
      async () => {
        helper.text(copyInput, "10000000000");

        // Initially, the copies will be more than the max.
        is(copyInput.checkValidity(), false, "Copy count is invalid");
        is(
          destinationPicker.disabled,
          false,
          "Destination picker is still enabled"
        );

        // When the settings event happens, copies resolves to max.
        await helper.waitForSettingsEvent();
        is(copyInput.value, "10000", "Copies gets set to max value");
        is(copyInput.checkValidity(), true, "Copy count is valid again");
      }
    );

    await helper.closeDialog();
  });
});
