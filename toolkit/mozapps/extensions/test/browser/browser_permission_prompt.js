/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/*
 * Test Permission Popup for Sideloaded Extensions.
 */
const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const ADDON_ID = "addon1@test.mozilla.org";
const CUSTOM_THEME_ID = "theme1@test.mozilla.org";
const DEFAULT_THEME_ID = "default-theme@mozilla.org";

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

function clickEnableExtension(addonElement) {
  addonElement.querySelector('[action="toggle-disabled"]').click();
}

// Test for bug 1647931
// Install a theme, enable it and then enable the default theme again
add_task(async function test_theme_enable() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["xpinstall.signatures.required", false],
      ["extensions.autoDisableScopes", 15],
      ["extensions.ui.ignoreUnsigned", true],
    ],
  });

  let theme = {
    manifest: {
      browser_specific_settings: { gecko: { id: CUSTOM_THEME_ID } },
      name: "Theme 1",
      theme: {
        colors: {
          frame: "#000000",
          tab_background_text: "#ffffff",
        },
      },
    },
  };

  let xpi = AddonTestUtils.createTempWebExtensionFile(theme);
  await AddonTestUtils.manuallyInstall(xpi);

  let changePromise = new Promise(resolve =>
    ExtensionsUI.once("change", resolve)
  );
  ExtensionsUI._checkForSideloaded();
  await changePromise;

  // enable fresh installed theme
  let manager = await open_manager("addons://list/theme");
  let customTheme = getAddonCard(manager, CUSTOM_THEME_ID);
  clickEnableExtension(customTheme);

  // enable default theme again
  let defaultTheme = getAddonCard(manager, DEFAULT_THEME_ID);
  clickEnableExtension(defaultTheme);

  let addon = await AddonManager.getAddonByID(CUSTOM_THEME_ID);
  await close_manager(manager);
  await addon.uninstall();
});

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
      browser_specific_settings: { gecko: { id: ADDON_ID } },
      name: "Test 1",
      permissions: ["history", "https://*/*"],
      icons: { 64: "foo-icon.png" },
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
  let addon = getAddonCard(manager, ADDON_ID);

  Assert.notEqual(addon, null, "Found sideloaded addon in about:addons");

  assertDisabledSideloadedExtensionElement(manager, addon);

  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  clickEnableExtension(addon);
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
  addon = getAddonCard(manager, ADDON_ID);
  Assert.notEqual(addon, null, "Found sideloaded addon in about:addons");

  assertEnabledSideloadedExtensionElement(manager, addon);

  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  clickEnableExtension(addon);
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
