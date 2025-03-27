/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const emptyPage =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

async function assertShown(task) {
  const otherTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    emptyPage
  );

  return BrowserTestUtils.withNewTab(emptyPage, async function (browser) {
    let popupshown = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popupshown"
    );

    await BrowserTestUtils.switchTab(gBrowser, otherTab);

    await SpecialPowers.spawn(browser, [], task);

    BrowserTestUtils.removeTab(otherTab);

    await popupshown;

    ok(true, "Permission prompt was shown");
  });
}

add_task(async function testPermissionShown() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      [
        "privacy.fingerprintingProtection.overrides",
        "-AllTargets,+CanvasRandomization,+CanvasImageExtractionPrompt",
      ],
    ],
  });

  await assertShown(function () {
    // We need to run without system principal to trigger the permission prompt
    content.eval("document.createElement('canvas').toDataURL()");
  });

  await SpecialPowers.popPrefEnv();
});
