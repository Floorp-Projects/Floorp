/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_OPEN_PROTECTION_PANEL() {
  await BrowserTestUtils.withNewTab(EXAMPLE_URL, async browser => {
    const popupEl = document.getElementById("protections-popup");
    const popupshown = BrowserTestUtils.waitForEvent(popupEl, "popupshown");

    await SMATestUtils.executeAndValidateAction({
      type: "OPEN_PROTECTION_PANEL",
    });

    await popupshown;
    Assert.equal(popupEl.state, "open", "Protections popup is open.");
  });
});
