/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/*
 * Test Permission Popup for Sideloaded Extensions.
 */
const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const ADDON_ID = "addon1@test.mozilla.org";

AddonTestUtils.initMochitest(this);

function assertDisabledSideloadedExtensionElement(managerWindow, addonElement) {
  const doc = addonElement.ownerDocument;
  const toggleDisabled = addonElement.querySelector(
    '[action="toggle-disabled"]'
  );
  is(
    doc.l10n.getAttributes(toggleDisabled).id,
    "extension-enable-addon-button-label",
    "Addon toggle-disabled action has the enable label"
  );
  ok(!toggleDisabled.checked, "toggle-disable isn't checked");
}

function assertEnabledSideloadedExtensionElement(managerWindow, addonElement) {
  const doc = addonElement.ownerDocument;
  const toggleDisabled = addonElement.querySelector(
    '[action="toggle-disabled"]'
  );
  is(
    doc.l10n.getAttributes(toggleDisabled).id,
    "extension-enable-addon-button-label",
    "Addon toggle-disabled action has the enable label"
  );
  ok(!toggleDisabled.checked, "toggle-disable isn't checked");
}

function clickEnableExtension(managerWindow, addonElement) {
  addonElement.querySelector('[action="toggle-disabled"]').click();
}

// Loading extension by sideloading method
add_task(async function test_sideloaded_extension_permissions_prompt() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["xpinstall.signatures.required", false],
      ["extensions.autoDisableScopes", 15],
      ["extensions.ui.ignoreUnsigned", true],
    ],
  });

  let options = {
    manifest: {
      applications: { gecko: { id: ADDON_ID } },
      name: "Test 1",
      permissions: ["history", "https://*/*"],
      icons: { "64": "foo-icon.png" },
    },
  };

  let xpi = AddonTestUtils.createTempWebExtensionFile(options);
  await AddonTestUtils.manuallyInstall(xpi);

  let changePromise = new Promise(resolve =>
    ExtensionsUI.once("change", resolve)
  );
  ExtensionsUI._checkForSideloaded();
  await changePromise;

  // Test click event on permission cancel option.
  let manager = await open_manager("addons://list/extension");
  let addon = get_addon_element(manager, ADDON_ID);

  Assert.notEqual(addon, null, "Found sideloaded addon in about:addons");

  assertDisabledSideloadedExtensionElement(manager, addon);

  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  clickEnableExtension(manager, addon);
  let panel = await popupPromise;

  ok(PopupNotifications.isPanelOpen, "Permission popup should be visible");
  panel.secondaryButton.click();
  ok(
    !PopupNotifications.isPanelOpen,
    "Permission popup should be closed / closing"
  );

  addon = await AddonManager.getAddonByID(ADDON_ID);
  ok(
    !addon.seen,
    "Seen flag should remain false after permissions are refused"
  );

  // Test click event on permission accept option.
  addon = get_addon_element(manager, ADDON_ID);
  Assert.notEqual(addon, null, "Found sideloaded addon in about:addons");

  assertEnabledSideloadedExtensionElement(manager, addon);

  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  clickEnableExtension(manager, addon);
  panel = await popupPromise;

  ok(PopupNotifications.isPanelOpen, "Permission popup should be visible");

  let notificationPromise = acceptAppMenuNotificationWhenShown(
    "addon-installed",
    ADDON_ID
  );

  panel.button.click();
  ok(
    !PopupNotifications.isPanelOpen,
    "Permission popup should be closed / closing"
  );
  await notificationPromise;

  addon = await AddonManager.getAddonByID(ADDON_ID);
  ok(addon.seen, "Seen flag should be true after permissions are accepted");

  ok(!PopupNotifications.isPanelOpen, "Permission popup should not be visible");

  await close_manager(manager);
  await addon.uninstall();
});
