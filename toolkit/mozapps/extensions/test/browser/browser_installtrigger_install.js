/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const XPI_URL = `${SECURE_TESTROOT}../xpinstall/amosigned.xpi`;
const XPI_ADDON_ID = "amosigned-xpi@tests.mozilla.org";

AddonTestUtils.initMochitest(this);

add_task(async function testInstallAfterHistoryPushState() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.webapi.testing", true],
      ["extensions.install.requireBuiltInCerts", false],
    ],
  });

  PermissionTestUtils.add(
    "https://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  registerCleanupFunction(async () => {
    PermissionTestUtils.remove("https://example.com", "install");
    await SpecialPowers.popPrefEnv();
  });

  await BrowserTestUtils.withNewTab(SECURE_TESTROOT, async browser => {
    let installPromptPromise = promisePopupNotificationShown(
      "addon-webext-permissions"
    ).then(panel => {
      panel.button.click();
    });

    let promptPromise = acceptAppMenuNotificationWhenShown(
      "addon-installed",
      XPI_ADDON_ID
    );

    await SpecialPowers.spawn(
      browser,
      [SECURE_TESTROOT, XPI_URL],
      (secureTestRoot, xpiUrl) => {
        // `sourceURL` should match the exact location, even after a location
        // update using the history API. In this case, we update the URL with
        // query parameters and expect `sourceURL` to contain those parameters.
        content.history.pushState(
          {}, // state
          "", // title
          `${secureTestRoot}?some=query&par=am`
        );

        content.InstallTrigger.install({ URL: xpiUrl });
      }
    );

    await Promise.all([installPromptPromise, promptPromise]);

    let addon = await promiseAddonByID(XPI_ADDON_ID);

    registerCleanupFunction(async () => {
      await addon.uninstall();
    });

    // Check that the expected installTelemetryInfo has been stored in the
    // addon details.
    AddonTestUtils.checkInstallInfo(addon, {
      method: "installTrigger",
      source: "test-host",
      sourceURL:
        "https://example.com/browser/toolkit/mozapps/extensions/test/browser/?some=query&par=am",
    });
  });
});
