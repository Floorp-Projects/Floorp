/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/*
* Test Permission Popup for Sideloaded Extensions.
*/
const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
const ADDON_ID = "addon1@test.mozilla.org";

AddonTestUtils.initMochitest(this);

// Loading extension by sideloading method
add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["xpinstall.signatures.required", false],
      ["extensions.autoDisableScopes", 15],
      ["extensions.ui.ignoreUnsigned", true],
    ],
  });

  let options = {
    manifest: {
      applications: {gecko: {id: ADDON_ID}},
      name: "Test 1",
      permissions: ["history", "https://*/*"],
      icons: {"64": "foo-icon.png"},
    },
  };

  let xpi = AddonTestUtils.createTempWebExtensionFile(options);
  await AddonTestUtils.manuallyInstall(xpi);

  let changePromise = new Promise(resolve => ExtensionsUI.once("change", resolve));
  ExtensionsUI._checkForSideloaded();
  await changePromise;

  // Test click event on permission cancel option.
  let manager = await open_manager("addons://list/extension");
  let addon = get_addon_element(manager, ADDON_ID);

  Assert.notEqual(addon, null, "Found sideloaded addon in about:addons");
  let el = addon.ownerDocument.getAnonymousElementByAttribute(addon, "anonid", "disable-btn");
  is_element_hidden(el, "Disable button not visible.");
  el = addon.ownerDocument.getAnonymousElementByAttribute(addon, "anonid", "enable-btn");
  is_element_visible(el, "Enable button visible");

  let popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  EventUtils.synthesizeMouseAtCenter(
    el,
    { clickCount: 1 },
    manager
  );

  let panel = await popupPromise;
  ok(PopupNotifications.isPanelOpen, "Permission popup should be visible");
  panel.secondaryButton.click();
  ok(!PopupNotifications.isPanelOpen, "Permission popup should be closed / closing");

  addon = await AddonManager.getAddonByID(ADDON_ID);
  ok(!addon.seen, "Seen flag should remain false after permissions are refused");

  // Test click event on permission accept option.
  addon = get_addon_element(manager, ADDON_ID);
  Assert.notEqual(addon, null, "Found sideloaded addon in about:addons");

  el = addon.ownerDocument.getAnonymousElementByAttribute(addon, "anonid", "disable-btn");
  is_element_hidden(el, "Disable button not visible.");
  el = addon.ownerDocument.getAnonymousElementByAttribute(addon, "anonid", "enable-btn");
  is_element_visible(el, "Enable button visible");

  popupPromise = promisePopupNotificationShown("addon-webext-permissions");
  EventUtils.synthesizeMouseAtCenter(
    manager.document.getAnonymousElementByAttribute(addon, "anonid", "enable-btn"),
    { clickCount: 1 },
    manager
  );

  panel = await popupPromise;
  ok(PopupNotifications.isPanelOpen, "Permission popup should be visible");

  let notificationPromise = acceptAppMenuNotificationWhenShown("addon-installed", ADDON_ID);

  panel.button.click();
  ok(!PopupNotifications.isPanelOpen, "Permission popup should be closed / closing");
  await notificationPromise;

  addon = await AddonManager.getAddonByID(ADDON_ID);
  ok(addon.seen, "Seen flag should be true after permissions are accepted");

  ok(!PopupNotifications.isPanelOpen, "Permission popup should not be visible");

  await addon.uninstall();

  await close_manager(manager);
});
